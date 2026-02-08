// Note: The I2S parallel driver is not compatible with ESP32-S3 (AtomS3).
// FastLED on S3 defaults to the RMT driver which is stable.
// If you experience hangs, ensure you are using FastLED 3.6 or higher.
#include <FastLED.h>

#include <M5Unified.h>
#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <stdarg.h>

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
const int LOGICAL_INITIAL_SPEED = 500; // The default logical speed
const int LOGICAL_REVERSE_INTERMEDIATE_SPEED = 200; // The speed to ramp down to during a reversal

// Ramping settings
const int RAMP_STEP = 5;            // How much to change speed in each ramp step
const int DEFAULT_RAMP_DURATION_MS = 4000; // Default duration for a full ramp
int currentRampDuration = DEFAULT_RAMP_DURATION_MS; // Variable to change ramp duration (in milliseconds)

// --- LED Strip Settings ---
#define ONBOARD_LED_PIN 35
const int LED_STRIP_PIN = 2;  // Grove Port Pin (Yellow wire) on AtomS3. (G1 is Pin 1).
const int NUM_LEDS = 200;      // Number of LEDs on your strip.

// --- Remote Control ---
// See the following for generating UUIDs:
// https://www.uuidgenerator.net/
#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define COMMAND_CHAR_UUID   "beb5483e-36e1-4688-b7f5-ea07361b26a8"

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

bool isDirectionClockwise = false; //runs a bit quieter in this direction.
bool isMotorRunning = false;

// --- LED Strip Objects & State ---
CRGB onboard_led[1];
CRGB leds[NUM_LEDS];
int led_position = 0;
unsigned long last_led_strip_update = 0;


// --- Throttled Logging --- DO NOT REMOVE THIS. It is useful to have. 
// A helper function for timestamped logs that can be throttled to prevent flooding the serial port.
// Set MIN_LOG_GAP_MS to a value like 50 or 200 to enable throttling.
unsigned long MIN_LOG_GAP_MS = 50; // Reduced gap to allow command + action logs to appear.
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
    if (logicalSpeed <= 0) return 0;
    // Map the logical speed (1-1000) to the physical PWM duty range (MIN to MAX)
    return map(logicalSpeed, 1, LOGICAL_MAX_SPEED, PHYSICAL_MIN_SPEED, PHYSICAL_MAX_SPEED);
}

/**
 * @brief Calculates the delay between ramp steps to achieve a specific duration.
 */
void updateRampTiming() {
    int delta = abs(targetLogicalSpeed - currentLogicalSpeed);
    int numSteps = (delta > 0) ? (delta / RAMP_STEP) : 1;
    rampStepDelay = (numSteps > 0) ? (currentRampDuration / numSteps) : 1;
}

// --- State Change Functions (Non-Blocking) ---
void triggerStart() {
    log_t("Triggering start...");
    if (!isMotorRunning) {
        led_position = 0; // Start LED cycle at the beginning
        targetLogicalSpeed = speedSetting;
        updateRampTiming();
        motorState = MOTOR_RAMPING_UP;
        isMotorRunning = true;
        onboard_led[0] = isDirectionClockwise ? CRGB(0, 50, 0) : CRGB(0, 0, 50);
        FastLED.show();
    }
}

void triggerReverse() {
    log_t("Triggering smooth reverse...");
    if (isMotorRunning) {
        reverseAfterRampDown = true;
        targetLogicalSpeed = LOGICAL_REVERSE_INTERMEDIATE_SPEED; // Ramp down to intermediate speed
        updateRampTiming();
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
        updateRampTiming();
        motorState = MOTOR_RAMPING_UP;
    }
}

void triggerSpeedDown() {
    speedSetting = max(LOGICAL_INITIAL_SPEED, speedSetting - LOGICAL_SPEED_INCREMENT);
    log_t("Triggering speed down. New setting: %d", speedSetting);
    if (isMotorRunning)
    {
        targetLogicalSpeed = speedSetting;
        updateRampTiming();
        motorState = MOTOR_RAMPING_DOWN;
    }
}

void triggerStop() {
    log_t("Triggering stop...");
    targetLogicalSpeed = 0;
    updateRampTiming();
    motorState = MOTOR_RAMPING_DOWN;
    reverseAfterRampDown = false; // Ensure this is false for a normal stop
}

void triggerSetSpeed(int newSpeed) {
    // This function can now start the motor from a stopped state.
    speedSetting = constrain(newSpeed, 0, LOGICAL_MAX_SPEED); // Allow setting speed to 0 to stop.
    log_t("Triggering set speed. New setting: %d", speedSetting);

    if (speedSetting == 0) {
        triggerStop();
        return;
    }

    targetLogicalSpeed = speedSetting;
    updateRampTiming();
    if (!isMotorRunning) {
        led_position = 0; // Start LED cycle at the beginning
        isMotorRunning = true;
        onboard_led[0] = isDirectionClockwise ? CRGB(0, 50, 0) : CRGB(0, 0, 50);
        FastLED.show();
    }

    if (targetLogicalSpeed > currentLogicalSpeed) {
        motorState = MOTOR_RAMPING_UP;
    } else if (targetLogicalSpeed < currentLogicalSpeed) {
        motorState = MOTOR_RAMPING_DOWN;
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
                val = constrain(val, 0, LOGICAL_MAX_SPEED);
                triggerSetSpeed(val);
            } else if (cmd == "ramp") {
                val = constrain(val, 0, 10000);
                currentRampDuration = val;
                log_t("Set Ramp Duration: %d", currentRampDuration);
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
    
    // --- FastLED Strip Setup ---
    FastLED.addLeds<WS2812B, ONBOARD_LED_PIN, GRB>(onboard_led, 1);
    FastLED.addLeds<WS2812B, LED_STRIP_PIN, GRB>(leds, NUM_LEDS);
    FastLED.clear();
    FastLED.show();

    // Initial State: Stopped
    ledcWrite(ledChannel1, 0);
    ledcWrite(ledChannel2, 0);
    isMotorRunning = false;

    speedSetting = LOGICAL_INITIAL_SPEED;

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

    pService->start();
    pServer->getAdvertising()->start();
    log_t("BLE Server started. Waiting for a client connection...");

    onboard_led[0] = CRGB::White;
    FastLED.show();

    // Start the motor running automatically on initialization.
    triggerStart();
}


void loop() {
    //log_t("Loop start."); // Diagnostic: Check if the main loop is running. However, this bogs down all logging.
    M5.update(); // Required for button state updates

    // --- LED Strip Animation ---
    // The LEDs only animate if the motor is logically running.
    if (isMotorRunning && currentLogicalSpeed > 0) {
        // Linear interpolation between two points: (400, 19.4ms) and (1000, 4.433ms).
        // We use a normalized fraction of the logical speed range for smoother scaling.
        float fraction = (float)currentLogicalSpeed / (float)LOGICAL_MAX_SPEED;
        float intervalMs = 28.8f - (fraction * 24.0f); // Scales from ~28.8ms at idle to ~4.8ms at max
        unsigned long dynamicInterval = (unsigned long)max(1.0f, intervalMs);

        if (millis() - last_led_strip_update > dynamicInterval) {
            last_led_strip_update = millis();
            
            // Dynamic Motion Blur: Higher speed = lower fade value = longer tail.
            // At max speed (1000), fade is 30 (long tail). At low speed (200), fade is 140 (short tail).
            uint8_t fadeAmount = map(currentLogicalSpeed, 0, LOGICAL_MAX_SPEED, 180, 30);
            fadeToBlackBy(leds, NUM_LEDS, fadeAmount);

            if (isDirectionClockwise) {
                led_position = (led_position + 1) % NUM_LEDS;
            } else {
                led_position = (led_position - 1 + NUM_LEDS) % NUM_LEDS;
            }

            // Directional Color: Match the strip color to the rotation direction.
            // Clockwise = Green, Counter-Clockwise = Blue.
            leds[led_position] = isDirectionClockwise ? CRGB::Green : CRGB::Blue;
            FastLED.show();
        }
    }


    // --- Non-Blocking Motor State Machine ---
    if (motorState != MOTOR_IDLE) {
        // Use the pre-calculated rampStepDelay for the timer check.
        if (millis() - lastRampStepTime > rampStepDelay) {
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
                // If a reversal was triggered, the first ramp-down to the intermediate speed is complete.
                // Now, start the ramp-up in the other direction.
                if (reverseAfterRampDown) {
                    isDirectionClockwise = !isDirectionClockwise;
                    onboard_led[0] = isDirectionClockwise ? CRGB(0, 50, 0) : CRGB(0, 0, 50);
                    FastLED.show();
                    targetLogicalSpeed = speedSetting; // Ramp up to the desired speed setting
                    updateRampTiming();
                    motorState = MOTOR_RAMPING_UP;
                    reverseAfterRampDown = false;
                    isMotorRunning = true;
                } else {
                    motorState = MOTOR_IDLE;
                    if (currentLogicalSpeed == 0) {
                        isMotorRunning = false;
                        speedSetting = LOGICAL_INITIAL_SPEED; // Reset for next start
                        onboard_led[0] = CRGB(50, 0, 0); // Red
                        FastLED.show();
                    }
                    log_t("Ramp complete. Current Speed: %d", currentLogicalSpeed);
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