#include <M5Unified.h>
#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <stdarg.h>

#define LED_PIN 35

// Atomic H-Driver Pin Definitions
const int IN1_PIN = 6;
const int IN2_PIN = 7;

// PWM Settings
const int freq = 25000;
const int resolution = 10; // 0-1023
const int ledChannel1 = 0;
const int ledChannel2 = 1;

// Speed Control Settings
const int PHYSICAL_MAX_SPEED = 900; // The PWM duty cycle for maximum speed
const int PHYSICAL_MIN_SPEED = 500; // The PWM duty cycle to overcome friction and start moving

const int LOGICAL_MAX_SPEED = 1000; // A linear scale for speed control
const int LOGICAL_SPEED_INCREMENT = 50;
const int LOGICAL_INITIAL_SPEED = 600; // The default logical speed

// Ramping settings
const int RAMP_STEP = 5;            // How much to change speed in each ramp step
const int DEFAULT_RAMP_DURATION_MS = 4000; // Default duration for a full ramp
int currentRampDuration = DEFAULT_RAMP_DURATION_MS; // Variable to change ramp duration (in milliseconds)

// --- Remote Control ---
// See the following for generating UUIDs:
// https://www.uuidgenerator.net/
#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define COMMAND_CHAR_UUID   "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define RAMP_CHAR_UUID      "c8a9353c-3893-43a8-9233-359c43530857"
BLECharacteristic *pRampCharacteristic = nullptr;

// --- Motor State Machine ---
// This replaces the blocking delay() functions with a responsive state machine.
enum MotorState {
    MOTOR_IDLE,
    MOTOR_RAMPING_DOWN,
    MOTOR_RAMPING_UP
};
MotorState motorState = MOTOR_IDLE;
int currentLogicalSpeed = 0; // The actual current speed of the motor
int speedSetting = LOGICAL_INITIAL_SPEED; // The user's desired speed setting
int targetLogicalSpeed = 0; // The immediate target for the current ramp
unsigned long lastRampStepTime = 0;
unsigned long rampStepDelay = 0;
bool reverseAfterRampDown = false;

bool isDirectionClockwise = true;
bool isMotorRunning = false;

// --- Throttled Logging --- DO NOT REMOVE THIS. It is useful to have. 
// A helper function for timestamped logs that can be throttled to prevent flooding the serial port.
// Set MIN_LOG_GAP_MS to a value like 50 or 200 to enable throttling.
unsigned long MIN_LOG_GAP_MS = 250; // Throttle logs to one line every 250ms for debugging.
unsigned long lastLogTimestamp = 0;

void log_t(const char* format, ...) {
    unsigned long now = millis();
    if (MIN_LOG_GAP_MS == 0 || (now - lastLogTimestamp > MIN_LOG_GAP_MS)) {
        char buf[256];
        va_list args;
        va_start(args, format);
        vsnprintf(buf, sizeof(buf), format, args);
        va_end(args);
        Serial.printf("%lu ms: %s\n", now, buf);
        lastLogTimestamp = now;
    }
}

// --- Core Motor & Mapping Functions ---
/**
 * @brief Sets the raw PWM duty cycle for the motor channels.
 * 
 * @param duty The PWM duty cycle (0-1023).
 * @param clockwise The direction of rotation.
 */
void setMotorDuty(int duty, bool clockwise) {
    if (clockwise) {
        ledcWrite(ledChannel1, 0);
        ledcWrite(ledChannel2, duty);
    } else {
        ledcWrite(ledChannel1, duty);
        ledcWrite(ledChannel2, 0);
    }
}

/**
 * @brief Maps a linear logical speed (0-1000) to the non-linear physical PWM duty cycle.
 * This accounts for the motor's dead zone.
 * 
 * @param logicalSpeed The desired speed on a linear scale.
 * @return The calculated PWM duty cycle for the motor.
 */
int mapSpeedToDuty(int logicalSpeed) {
    if (logicalSpeed <= 0) {
        return 0;
    }
    // Apply a quadratic easing function to make the ramp-up feel smoother.
    // This compensates for the motor's non-linear response at low speeds.
    float normalizedSpeed = (float)logicalSpeed / LOGICAL_MAX_SPEED;
    float easedSpeed = normalizedSpeed * normalizedSpeed;

    // Map the eased, normalized speed to the physical PWM duty range.
    return PHYSICAL_MIN_SPEED + (easedSpeed * (PHYSICAL_MAX_SPEED - PHYSICAL_MIN_SPEED));
}

// --- State Change Functions (Non-Blocking) ---
void triggerStart() {
    log_t("Triggering start...");
    if (!isMotorRunning) {
        targetLogicalSpeed = speedSetting;
        motorState = MOTOR_RAMPING_UP;
        isMotorRunning = true;
        neopixelWrite(LED_PIN, 0, isDirectionClockwise ? 50 : 0, isDirectionClockwise ? 0 : 50);
    }
}

void triggerReverse() {
    log_t("Triggering smooth reverse...");
    if (isMotorRunning) {
        reverseAfterRampDown = true;
        targetLogicalSpeed = 0; // Set the immediate target to ramp down to zero
        motorState = MOTOR_RAMPING_DOWN;
    } else {
        // If motor is stopped, just start it in the new direction
        isDirectionClockwise = !isDirectionClockwise;
        triggerStart();
    }
}

void triggerSpeedUp() {
    speedSetting = min(LOGICAL_MAX_SPEED, speedSetting + LOGICAL_SPEED_INCREMENT);
    log_t("Triggering speed up. New setting: %d", speedSetting);
    if (isMotorRunning)
    {
        targetLogicalSpeed = speedSetting;
        motorState = MOTOR_RAMPING_UP;
    }
}

void triggerSpeedDown() {
    speedSetting = max(LOGICAL_INITIAL_SPEED, speedSetting - LOGICAL_SPEED_INCREMENT);
    log_t("Triggering speed down. New setting: %d", speedSetting);
    if (isMotorRunning)
    {
        targetLogicalSpeed = speedSetting;
        motorState = MOTOR_RAMPING_DOWN;
    }
}

void triggerStop() {
    log_t("Triggering stop...");
    targetLogicalSpeed = 0;
    motorState = MOTOR_RAMPING_DOWN;
    reverseAfterRampDown = false; // Ensure this is false for a normal stop
}

void triggerSetSpeed(int newSpeed) {
    speedSetting = constrain(newSpeed, 0, LOGICAL_MAX_SPEED);
    log_t("Triggering set speed. New setting: %d", speedSetting);
    if (isMotorRunning) {
        targetLogicalSpeed = speedSetting;
        if (targetLogicalSpeed > currentLogicalSpeed) {
            motorState = MOTOR_RAMPING_UP;
        } else if (targetLogicalSpeed < currentLogicalSpeed) {
            motorState = MOTOR_RAMPING_DOWN;
        }
    }
}

// --- BLE Callbacks ---
class CommandCallback : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
        std::string value = pCharacteristic->getValue();
        if (value.length() == 0) {
            return;
        }

        log_t("Received command: %s", value.c_str());

        size_t colon_pos = value.find(':');

        if (colon_pos != std::string::npos) {
            // Command with a value, e.g., "ms:800" or "ramp:4000"
            std::string cmd = value.substr(0, colon_pos);
            int val = atoi(value.substr(colon_pos + 1).c_str());

            if (cmd == "ms") {
                triggerSetSpeed(val);
            } else if (cmd == "ramp") {
                currentRampDuration = val;
                log_t("Set Ramp Duration: %d", currentRampDuration);
                // Also update the dedicated ramp characteristic's value
                if (pRampCharacteristic != nullptr) {
                    pRampCharacteristic->setValue(String(currentRampDuration).c_str());
                }
            } else {
                log_t("Unknown command prefix: %s", cmd.c_str());
            }

        } else if (value.length() == 1) {
            // Single-letter command
            char cmd_char = value[0];
            switch(cmd_char) {
                case 'g': triggerStart(); break;
                case 's': triggerStop(); break;
                case 'r': triggerReverse(); break;
                case 'u': triggerSpeedUp(); break;
                case 'd': triggerSpeedDown(); break;
                default: log_t("Unknown command: %c", cmd_char); break;
            }
        } else {
            log_t("Invalid command format: %s", value.c_str());
        }

        // Update the characteristic's value so the last command can be read back.
        pCharacteristic->setValue(value);
    }
};

class SettingCallback : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
        std::string uuid_str = pCharacteristic->getUUID().toString();
        std::string value_str = pCharacteristic->getValue();
        if (value_str.length() > 0) {
            int value = atoi(value_str.c_str());

            if (uuid_str == BLEUUID(RAMP_CHAR_UUID).toString()) {
                currentRampDuration = value;
                log_t("Set Ramp Duration: %d", currentRampDuration);
            }
        }
    }
};

void setup() {
    auto cfg = M5.config();
    cfg.serial_baudrate = 115200;
    M5.begin(cfg);

    // Reset pins to ensure no other peripheral is holding them
    gpio_reset_pin((gpio_num_t)IN1_PIN);
    gpio_reset_pin((gpio_num_t)IN2_PIN);

    delay(1000); // Give serial monitor time to connect

    log_t("System Initialized");

    // Configure PWM for H-Bridge
    ledcSetup(ledChannel1, freq, resolution);
    ledcSetup(ledChannel2, freq, resolution);
    ledcAttachPin(IN1_PIN, ledChannel1);
    ledcAttachPin(IN2_PIN, ledChannel2);
    
    // Initial State: Stopped
    ledcWrite(ledChannel1, 0);
    ledcWrite(ledChannel2, 0);
    isMotorRunning = false;

    // --- BLE Setup ---
    BLEDevice::init("Spiral Sculpture");
    BLEServer *pServer = BLEDevice::createServer();
    BLEService *pService = pServer->createService(SERVICE_UUID);

    // Command Characteristic
    BLECharacteristic *pCommandCharacteristic = pService->createCharacteristic(
                                         COMMAND_CHAR_UUID,
                                         BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE
                                       );
    pCommandCharacteristic->setCallbacks(new CommandCallback());
    pCommandCharacteristic->setValue(" "); // Set an initial value

    // Ramp Duration Characteristic
    pRampCharacteristic = pService->createCharacteristic(
                                         RAMP_CHAR_UUID,
                                         BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE
                                       );
    pRampCharacteristic->setValue(String(currentRampDuration).c_str());
    pRampCharacteristic->setCallbacks(new SettingCallback());

    pService->start();
    pServer->getAdvertising()->start();
    log_t("BLE Server started. Waiting for a client connection...");

    neopixelWrite(LED_PIN, 255, 255, 255);
}


void loop() {
    M5.update(); // Required for button state updates

    // --- Non-Blocking Motor State Machine ---
    if (motorState != MOTOR_IDLE) {
        // Calculate a constant step delay based on a full-range ramp to ensure a smooth, consistent ramp rate.
        int fullRangeNumSteps = LOGICAL_MAX_SPEED / RAMP_STEP;
        unsigned long constantRampStepDelay = (fullRangeNumSteps > 0) ? (currentRampDuration / fullRangeNumSteps) : 1;
        if (constantRampStepDelay == 0) {
            constantRampStepDelay = 1;
        }

        // Use the corrected, constant delay for the timer check.
        if (millis() - lastRampStepTime > constantRampStepDelay) {
            lastRampStepTime = millis();

            if (motorState == MOTOR_RAMPING_UP) {
                currentLogicalSpeed = min(targetLogicalSpeed, currentLogicalSpeed + RAMP_STEP);
            } else { // RAMPING_DOWN
                currentLogicalSpeed = max(targetLogicalSpeed, currentLogicalSpeed - RAMP_STEP);
            }

            // log_t("Ramping... Current Speed: %d", currentLogicalSpeed); // This line is too verbose for normal operation.

            setMotorDuty(mapSpeedToDuty(currentLogicalSpeed), isDirectionClockwise);

            // Check if ramp is complete
            if (currentLogicalSpeed == targetLogicalSpeed) {
                if (reverseAfterRampDown && currentLogicalSpeed == 0) {
                    isDirectionClockwise = !isDirectionClockwise;
                    neopixelWrite(LED_PIN, 0, isDirectionClockwise ? 50 : 0, isDirectionClockwise ? 0 : 50);
                    targetLogicalSpeed = speedSetting; // Ramp up to the desired speed setting
                    motorState = MOTOR_RAMPING_UP;
                    reverseAfterRampDown = false;
                    isMotorRunning = true;
                } else {
                    motorState = MOTOR_IDLE;
                    if (currentLogicalSpeed == 0) {
                        isMotorRunning = false;
                        speedSetting = LOGICAL_INITIAL_SPEED; // Reset for next start
                        neopixelWrite(LED_PIN, 50, 0, 0); // Red
                    }
                    log_t("Ramp complete.");
                }
            }
        }
    }

    // Priority 1: Long Press. This is the highest priority and cancels any pending clicks.
    if (M5.BtnA.pressedFor(2000)) {
        // Only trigger a stop if the motor is running and not already in the process of stopping.
        if (isMotorRunning && !(motorState == MOTOR_RAMPING_DOWN && targetLogicalSpeed == 0)) {
            triggerStop();
        }
    }
    else if (M5.BtnA.wasSingleClicked()) {
        triggerReverse();
    }
    else if (M5.BtnA.wasDoubleClicked()) {
        triggerSpeedUp();
    }

    // Yield to other tasks, especially the BLE stack, to prevent task starvation.
    delay(1);
}