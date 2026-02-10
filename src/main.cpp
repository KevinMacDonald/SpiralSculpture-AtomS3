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
static const int __IN1_PIN = 6;
static const int __IN2_PIN = 7;

// PWM Settings
static const int __freq = 25000;
static const int __resolution = 10; // 0-1023
static const int __ledChannel1 = 0;
static const int __ledChannel2 = 1;

// Speed Control Settings
static const int __PHYSICAL_MAX_SPEED = 900; // The PWM duty cycle for maximum speed
static const int __PHYSICAL_MIN_SPEED = 500; // The PWM duty cycle to overcome friction and start moving

static const int __LOGICAL_MAX_SPEED = 1000; // A linear scale for speed control
static const int __LOGICAL_SPEED_INCREMENT = 50;
static const int __LOGICAL_INITIAL_SPEED = 400; // The default logical speed
static const int __LOGICAL_REVERSE_INTERMEDIATE_SPEED = 200; // The speed to ramp down to during a reversal

// Ramping settings
static const int __RAMP_STEP = 5;            // How much to change speed in each ramp step
static const int __DEFAULT_RAMP_DURATION_MS = 4000; // Default duration for a full ramp
static int __currentRampDuration = __DEFAULT_RAMP_DURATION_MS; // Variable to change ramp duration (in milliseconds)

// --- LED Strip Settings ---
static const int __ONBOARD_LED_PIN = 35;
static const int __LED_STRIP_PIN = 2;  // Grove Port Pin (Yellow wire) on AtomS3. (G1 is Pin 1).
static const int __NUM_LEDS = 198;      // Number of LEDs on your strip.
static const int __VIRTUAL_GAP = 25;    // Non-existent pixels to match mechanical rotation
static const int __LOGICAL_NUM_LEDS = __NUM_LEDS + __VIRTUAL_GAP;

// --- Remote Control ---
// See the following for generating UUIDs:
// https://www.uuidgenerator.net/
static const char* __SERVICE_UUID = "4fafc201-1fb5-459e-8fcc-c5c9c331914b";
static const char* __COMMAND_CHAR_UUID = "beb5483e-36e1-4688-b7f5-ea07361b26a8";

// --- Motor State Machine ---
// This replaces the blocking delay() functions with a responsive state machine.
enum MotorState {
    __MOTOR_IDLE,
    __MOTOR_RAMPING_DOWN,
    __MOTOR_RAMPING_UP
};
static MotorState __motorState = __MOTOR_IDLE;
static int __currentLogicalSpeed = 0; // The actual current speed of the motor
static int __speedSetting = __LOGICAL_INITIAL_SPEED; // The user's desired speed setting
static int __targetLogicalSpeed = 0; // The immediate target for the current ramp
static unsigned long __lastRampStepTime = 0;
static int __rampStartSpeed = 0;      // Speed at the beginning of the current ramp maneuver
static unsigned long __rampStartTime = 0; // Time when the current ramp maneuver started
static unsigned long __rampStepDelay = 0;
static bool __reverseAfterRampDown = false;

static bool __pendingOff = false; // Flag to handle "off" command safely in the main loop
static bool __isDirectionClockwise = false; //runs a bit quieter in this direction.
static bool __isMotorRunning = false;

// --- Speed Sync Lookup Table ---
struct SpeedSyncPair {
    int logicalSpeed;
    int revTimeMs;
};

static const SpeedSyncPair __speedSyncTable[] = {
    { 400, 5200 },
    { 700, 2096 },
    { 1000, 1250 }
};
static const int __speedSyncTableSize = sizeof(__speedSyncTable) / sizeof(SpeedSyncPair);

// --- LED Strip Objects & State ---
static CRGB __onboard_led[1];
static CRGB __leds[__NUM_LEDS];
static int __led_position = 0;
static uint8_t __masterBrightness = 255; // Global master brightness (0-255)
static uint8_t __bgHue = 160;          // Default to Blue (160)
static uint8_t __bgBrightness = 76;    // Default to 30% (76/255)
static uint8_t __cometHue = 0;         // Default to Red (0)
static int __cometTailLength = 10;     // Default tail length
static int __cometCount = 3;           // Default number of moving points
static float __ledIntervalMs = 20.0f;  // Absolute time between LED steps in ms
static unsigned long __last_led_strip_update = 0;

// --- Manual LED Sync State ---
static bool __isManualLedInterval = false;    // Flag to override the sync table
static float __manualLedIntervalMs = 0;       // The base interval set by the user
static int __manualSpeedReference = 0;        // The logical speed at which the manual interval was set


// --- Throttled Logging --- DO NOT REMOVE THIS. It is useful to have. 
// A helper function for timestamped logs that can be throttled to prevent flooding the serial port.
// Set MIN_LOG_GAP_MS to a value like 50 or 200 to enable throttling.
static unsigned long __MIN_LOG_GAP_MS = 50; // Reduced gap to allow command + action logs to appear.
static unsigned long __lastLogTimestamp = 0;

static void log_t(const char* format, ...) {
    unsigned long now = millis();
    if (__MIN_LOG_GAP_MS == 0 || (now - __lastLogTimestamp > __MIN_LOG_GAP_MS)) {
        char buf[256];
        va_list args;
        va_start(args, format);
        vsnprintf(buf, sizeof(buf), format, args);
        va_end(args);
        Serial.printf("%lu ms: %s\n", now, buf);
        __lastLogTimestamp = now;
    }
}

/**
 * @brief Looks up the target speed in the sync table and updates the LED interval.
 * If no match is found, it falls back to the interpolated calculation method.
 */
void applySpeedSyncLookup(int speed) {
    if (__speedSyncTableSize < 2) return;

    if (speed > 0 && __isManualLedInterval) {
        // Scale the manually set interval based on the ratio of the reference speed to the new speed.
        // This maintains the user's custom sync across speed changes and reversals.
        __ledIntervalMs = __manualLedIntervalMs * ((float)__manualSpeedReference / (float)speed);
        return;
    }

    float targetRevTime = 0;

    if (speed <= __speedSyncTable[0].logicalSpeed) {
        // Linear extrapolation using the first segment
        float m = (float)(__speedSyncTable[1].revTimeMs - __speedSyncTable[0].revTimeMs) /
                  (float)(__speedSyncTable[1].logicalSpeed - __speedSyncTable[0].logicalSpeed);
        targetRevTime = __speedSyncTable[0].revTimeMs + m * (speed - __speedSyncTable[0].logicalSpeed);
    } else if (speed >= __speedSyncTable[__speedSyncTableSize - 1].logicalSpeed) {
        // Linear extrapolation using the last segment
        int last = __speedSyncTableSize - 1;
        float m = (float)(__speedSyncTable[last].revTimeMs - __speedSyncTable[last-1].revTimeMs) /
                  (float)(__speedSyncTable[last].logicalSpeed - __speedSyncTable[last-1].logicalSpeed);
        targetRevTime = __speedSyncTable[last].revTimeMs + m * (speed - __speedSyncTable[last].logicalSpeed);
    } else {
        // Piecewise linear interpolation
        for (int i = 0; i < __speedSyncTableSize - 1; i++) {
            if (speed >= __speedSyncTable[i].logicalSpeed && speed <= __speedSyncTable[i+1].logicalSpeed) {
                float fraction = (float)(speed - __speedSyncTable[i].logicalSpeed) /
                                 (float)(__speedSyncTable[i+1].logicalSpeed - __speedSyncTable[i].logicalSpeed);
                targetRevTime = __speedSyncTable[i].revTimeMs + fraction * (__speedSyncTable[i+1].revTimeMs - __speedSyncTable[i].revTimeMs);
                break;
            }
        }
    }

    __ledIntervalMs = targetRevTime / (float)__LOGICAL_NUM_LEDS;
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
        ledcWrite(__ledChannel1, 0);
        ledcWrite(__ledChannel2, duty);
    } else {
        ledcWrite(__ledChannel1, duty);
        ledcWrite(__ledChannel2, 0);
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
    return map(logicalSpeed, 1, __LOGICAL_MAX_SPEED, __PHYSICAL_MIN_SPEED, __PHYSICAL_MAX_SPEED);
}

/**
 * @brief Calculates the delay between ramp steps to achieve a specific duration.
 */
void updateRampTiming() {
    int delta = abs(__targetLogicalSpeed - __currentLogicalSpeed);
    int numSteps = (delta > 0) ? (delta / __RAMP_STEP) : 1;
    __rampStepDelay = (numSteps > 0) ? (__currentRampDuration / numSteps) : 1;
}

// --- State Change Functions (Non-Blocking) ---
void triggerStart() {
    log_t("Triggering start...");
    if (!__isMotorRunning) {
        __rampStartSpeed = __currentLogicalSpeed;
        __rampStartTime = millis();
        __led_position = 0; // Start LED cycle at the beginning
        __targetLogicalSpeed = __speedSetting;
        applySpeedSyncLookup(__targetLogicalSpeed);
        updateRampTiming();
        __motorState = __MOTOR_RAMPING_UP;
        __isMotorRunning = true;
    }
}

void triggerReverse() {
    log_t("Triggering smooth reverse...");
    if (__isMotorRunning) {
        __rampStartSpeed = __currentLogicalSpeed;
        __rampStartTime = millis();
        __reverseAfterRampDown = true;
        __targetLogicalSpeed = __LOGICAL_REVERSE_INTERMEDIATE_SPEED; // Ramp down to intermediate speed
        applySpeedSyncLookup(__targetLogicalSpeed);
        updateRampTiming();
        __motorState = __MOTOR_RAMPING_DOWN;
    } else {
        // If motor is stopped, just start it in the new direction
        __isDirectionClockwise = !__isDirectionClockwise;
        triggerStart();
    }
}

void triggerSpeedUp() {
    __speedSetting = min(__LOGICAL_MAX_SPEED, __speedSetting + __LOGICAL_SPEED_INCREMENT);
    log_t("Triggering speed up. New setting: %d", __speedSetting);
    if (__isMotorRunning)
    {
        __rampStartSpeed = __currentLogicalSpeed;
        __rampStartTime = millis();
        __targetLogicalSpeed = __speedSetting;
        applySpeedSyncLookup(__targetLogicalSpeed);
        updateRampTiming();
        __motorState = __MOTOR_RAMPING_UP;
    }
}

void triggerSpeedDown() {
    __speedSetting = max(__LOGICAL_INITIAL_SPEED, __speedSetting - __LOGICAL_SPEED_INCREMENT);
    log_t("Triggering speed down. New setting: %d", __speedSetting);
    if (__isMotorRunning)
    {
        __rampStartSpeed = __currentLogicalSpeed;
        __rampStartTime = millis();
        __targetLogicalSpeed = __speedSetting;
        applySpeedSyncLookup(__targetLogicalSpeed);
        updateRampTiming();
        __motorState = __MOTOR_RAMPING_DOWN;
    }
}

void triggerStop() {
    log_t("Triggering stop...");
    __rampStartSpeed = __currentLogicalSpeed;
    __rampStartTime = millis();
    __targetLogicalSpeed = 0;
    updateRampTiming();
    __motorState = __MOTOR_RAMPING_DOWN;
    __reverseAfterRampDown = false; // Ensure this is false for a normal stop
}

void triggerSetSpeed(int newSpeed) {
    // This function can now start the motor from a stopped state.
    __speedSetting = constrain(newSpeed, 0, __LOGICAL_MAX_SPEED); // Allow setting speed to 0 to stop.
    log_t("Triggering set speed. New setting: %d", __speedSetting);

    if (__speedSetting == 0) {
        triggerStop();
        return;
    }

    __rampStartSpeed = __currentLogicalSpeed;
    __rampStartTime = millis();

    __targetLogicalSpeed = __speedSetting;
    applySpeedSyncLookup(__targetLogicalSpeed);
    updateRampTiming();
    if (!__isMotorRunning) {
        __led_position = 0; // Start LED cycle at the beginning
        __isMotorRunning = true;
    }

    if (__targetLogicalSpeed > __currentLogicalSpeed) {
        __motorState = __MOTOR_RAMPING_UP;
    } else if (__targetLogicalSpeed < __currentLogicalSpeed) {
        __motorState = __MOTOR_RAMPING_DOWN;
    }
}

// --- BLE Callbacks ---
class MyServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
        log_t("BLE Client Connected");
    };

    void onDisconnect(BLEServer* pServer) {
        log_t("BLE Client Disconnected. Restarting advertising...");
        // Restart advertising so the device can be found again
        pServer->getAdvertising()->start();
    }
};

class CommandCallback : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
        std::string value = pCharacteristic->getValue();
        if (value.length() == 0) {
            return;
        }

        log_t("Received command: %s", value.c_str());

        size_t colon_pos = value.find(':');

        if (colon_pos != std::string::npos) {
            // --- Commands with values (Format: "cmd:value" or "cmd:val1,val2") ---
            std::string cmd = value.substr(0, colon_pos);
            int val = atoi(value.substr(colon_pos + 1).c_str());

            if (cmd == "ms") {
                // ms:XXX - Sets the motor's logical speed (0-1000).
                val = constrain(val, 0, __LOGICAL_MAX_SPEED);
                triggerSetSpeed(val);
            } else if (cmd == "ramp") {
                // ramp:XXXX - Sets the duration (in ms) for a full speed ramp (0 to 1000).
                val = constrain(val, 0, 10000);
                __currentRampDuration = val;
                log_t("Set Ramp Duration: %d", __currentRampDuration);
            } else if (cmd == "brightness") {
                // brightness:XX - Sets the global master brightness (0-100).
                int brightness_pct = constrain(val, 0, 100);
                __masterBrightness = (uint8_t)((brightness_pct * 255) / 100);
                FastLED.setBrightness(__masterBrightness);
                log_t("Master Brightness set to: %d%% (%d/255)", brightness_pct, __masterBrightness);
            } else if (cmd == "back_color") {
                // back_color:XXX,YY - Sets background Hue (0-255) and Brightness % (0-50).
                std::string params = value.substr(colon_pos + 1);
                size_t comma_pos = params.find(',');
                if (comma_pos != std::string::npos) {
                    int h = atoi(params.substr(0, comma_pos).c_str());
                    int b_pct = atoi(params.substr(comma_pos + 1).c_str());
                    __bgHue = (uint8_t)constrain(h, 0, 255);
                    __bgBrightness = (uint8_t)((constrain(b_pct, 0, 50) * 255) / 100);
                    log_t("Background set to Hue: %d, Brightness: %d%% (%d)", __bgHue, b_pct, __bgBrightness);
                }
            } else if (cmd == "tails") {
                // tails:XXX,YY,ZZ - Sets Comet Hue (0-255), Tail Length (LEDs), and Comet Count.
                // Safety: Ignored if total lit LEDs (Count * Length) exceeds 80% of the strip.
                std::string params = value.substr(colon_pos + 1);
                size_t comma1 = params.find(',');
                size_t comma2 = params.find(',', comma1 + 1);
                if (comma1 != std::string::npos && comma2 != std::string::npos) {
                    int h = atoi(params.substr(0, comma1).c_str());
                    int l = atoi(params.substr(comma1 + 1, comma2 - (comma1 + 1)).c_str());
                    int c = atoi(params.substr(comma2 + 1).c_str());
                    if (c * l <= __LOGICAL_NUM_LEDS * 0.8) {
                        __cometHue = (uint8_t)constrain(h, 0, 255);
                        __cometTailLength = max(1, l);
                        __cometCount = max(1, c);
                        log_t("Tails set: Hue %d, Length %d, Count %d", __cometHue, __cometTailLength, __cometCount);
                    } else {
                        log_t("Tails command ignored: exceeds 80%% of strip.");
                    }
                }
            } else if (cmd == "led_cycle_time") {
                // led_cycle_time:XXXX - Sets the absolute time for one full revolution of the LED cycle in ms.
                if (val > 0) {
                    __isManualLedInterval = true;
                    __manualLedIntervalMs = (float)val / (float)__LOGICAL_NUM_LEDS;
                    __manualSpeedReference = (__currentLogicalSpeed > 0) ? __currentLogicalSpeed : __speedSetting;
                    __ledIntervalMs = __manualLedIntervalMs;
                    log_t("LED Manual Sync set at speed %d. Step interval: %.2f ms", __manualSpeedReference, __ledIntervalMs);
                }
            } else if (cmd == "off") {
                // off: - Ramps down the motor and kills LED animation immediately.
                __pendingOff = true;
            } else {
                log_t("Unknown command prefix: %s", cmd.c_str());
            }

        } else if (value == "off") {
            // Handle "off" without a colon
            __pendingOff = true;
        } else if (value == "motor_start") {
            triggerStart();
        } else if (value == "motor_stop") {
            triggerStop();
        } else if (value == "start_defaults") {
            // Reset all parameters to setup() defaults
            __speedSetting = __LOGICAL_INITIAL_SPEED;
            __masterBrightness = 255;
            __bgHue = 160;
            __bgBrightness = 76;
            __cometHue = 0;
            __cometTailLength = 10;
            __cometCount = 3;
            __isManualLedInterval = false;
            __currentRampDuration = __DEFAULT_RAMP_DURATION_MS;
            FastLED.setBrightness(__masterBrightness);
            triggerSetSpeed(__speedSetting);
            log_t("System reset to defaults and started.");
        } else if (value == "reverse") {
            triggerReverse();
        } else if (value == "speed_up") {
            triggerSpeedUp();
        } else if (value == "speed_down") {
            triggerSpeedDown();
        } else if (value == "led_cycle_up") {
            __isManualLedInterval = true;
            __ledIntervalMs *= 0.92f;
            __manualLedIntervalMs = __ledIntervalMs;
            __manualSpeedReference = (__currentLogicalSpeed > 0) ? __currentLogicalSpeed : __speedSetting;
            log_t("LED Cycle speed UP 8%% (Manual). Interval: %.2f ms", __ledIntervalMs);
        } else if (value == "led_cycle_down") {
            __isManualLedInterval = true;
            __ledIntervalMs *= 1.08f;
            __manualLedIntervalMs = __ledIntervalMs;
            __manualSpeedReference = (__currentLogicalSpeed > 0) ? __currentLogicalSpeed : __speedSetting;
            log_t("LED Cycle speed DOWN 8%% (Manual). Interval: %.2f ms", __ledIntervalMs);
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

    // Force LED pin LOW immediately to prevent floating-point startup flickers
    pinMode(__LED_STRIP_PIN, OUTPUT);
    digitalWrite(__LED_STRIP_PIN, LOW);

    // Reset pins to ensure no other peripheral is holding them
    gpio_reset_pin((gpio_num_t)__IN1_PIN);
    gpio_reset_pin((gpio_num_t)__IN2_PIN);

    log_t("System Initialized");

    // Configure PWM for H-Bridge
    ledcSetup(__ledChannel1, __freq, __resolution);
    ledcSetup(__ledChannel2, __freq, __resolution);
    ledcAttachPin(__IN1_PIN, __ledChannel1);
    ledcAttachPin(__IN2_PIN, __ledChannel2);
    
    // --- FastLED Strip Setup ---
    FastLED.addLeds<WS2812B, __ONBOARD_LED_PIN, GRB>(__onboard_led, 1);
    FastLED.addLeds<WS2812B, __LED_STRIP_PIN, GRB>(__leds, __NUM_LEDS);

    // Set a safety power limit (5V, 500mA is safe for AtomS3 internal regulator)
    FastLED.setMaxPowerInVoltsAndMilliamps(5, 500);
    FastLED.setBrightness(__masterBrightness);

    // Immediate blackout to overwrite any RMT initialization glitches
    FastLED.showColor(CRGB::Black); 

    // Initial State: Stopped
    ledcWrite(__ledChannel1, 0);
    ledcWrite(__ledChannel2, 0);
    __isMotorRunning = false;

    __speedSetting = __LOGICAL_INITIAL_SPEED;

    // --- BLE Setup ---
    BLEDevice::init("Spiral Sculpture");
    BLEServer *pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());
    BLEService *pService = pServer->createService(__SERVICE_UUID);

    // Command Characteristic
    BLECharacteristic *pCommandCharacteristic = pService->createCharacteristic(
                                         __COMMAND_CHAR_UUID,
                                         BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE
                                       );
    pCommandCharacteristic->setCallbacks(new CommandCallback());
    pCommandCharacteristic->setValue(" "); // Set an initial value

    pService->start();
    pServer->getAdvertising()->start();
    log_t("BLE Server started. Waiting for a client connection...");

    __onboard_led[0] = CRGB(50, 0, 0); // Dim Red to show power is on and motor is stopped
    FastLED.show();

    // Start the motor running automatically on initialization.
    triggerStart();
}


void loop() {
    //log_t("Loop start."); // Diagnostic: Check if the main loop is running. However, this bogs down all logging.
    M5.update(); // Required for button state updates

    // --- Handle Pending Off Command ---
    if (__pendingOff) {
        log_t("Processing Off command...");
        triggerStop();         // Start motor ramp down
        __isMotorRunning = false; // Stop LED animation logic
        FastLED.clear(true);    // Blackout all LEDs immediately
        __pendingOff = false;
    }

    // --- LED Strip Animation ---
    // The LEDs only animate if the motor is logically running.
    if (__isMotorRunning && __currentLogicalSpeed > 0) {
        // Use the absolute interval set by the user.
        unsigned long dynamicInterval = (unsigned long)max(1.0f, __ledIntervalMs);

        if (millis() - __last_led_strip_update > dynamicInterval) {
            __last_led_strip_update = millis();
            
            // 1. Fade existing LEDs for the comet tail based on requested length
            uint8_t fadeAmount = 255 / __cometTailLength;
            fadeToBlackBy(__leds, __NUM_LEDS, fadeAmount);
            
            // 2. Apply dynamic background floor
            CRGB bgColor = CHSV(__bgHue, 255, __bgBrightness);
            
            // Update onboard LED to show direction: Green for CW, Blue for CCW
            __onboard_led[0] = __isDirectionClockwise ? CRGB(0, 50, 0) : CRGB(0, 0, 50);

            // Apply background using a "Lighten" blend.
            // This preserves the tail's color by taking the maximum of each channel.
            for (int i = 0; i < __NUM_LEDS; i++) {
                __leds[i].r = max(__leds[i].r, bgColor.r);
                __leds[i].g = max(__leds[i].g, bgColor.g);
                __leds[i].b = max(__leds[i].b, bgColor.b);
            }

            if (__isDirectionClockwise) {
                __led_position = (__led_position - 1 + __LOGICAL_NUM_LEDS) % __LOGICAL_NUM_LEDS;
            } else {
                __led_position = (__led_position + 1) % __LOGICAL_NUM_LEDS;
            }

            // 3. Draw the heads (spaced evenly)
            for (int j = 0; j < __cometCount; j++) {
                int pos = (__led_position + j * (__LOGICAL_NUM_LEDS / __cometCount)) % __LOGICAL_NUM_LEDS;
                // Only draw if the logical position exists on the physical strip
                if (pos < __NUM_LEDS) {
                    __leds[pos] = CHSV(__cometHue, 255, 255);
                }
            }
            FastLED.show();
        }
    }


    // --- Non-Blocking Motor State Machine ---
    if (__motorState != __MOTOR_IDLE) {
        // Use the pre-calculated rampStepDelay for the timer check.
        if (millis() - __lastRampStepTime > __rampStepDelay) {
            __lastRampStepTime = millis();

            if (__motorState == __MOTOR_RAMPING_UP) {
                __currentLogicalSpeed = min(__targetLogicalSpeed, __currentLogicalSpeed + __RAMP_STEP);
            } else { // RAMPING_DOWN
                __currentLogicalSpeed = max(__targetLogicalSpeed, __currentLogicalSpeed - __RAMP_STEP);
            }

            // log_t("Ramping... Current Speed: %d", __currentLogicalSpeed); // This line is too verbose for normal operation.

            setMotorDuty(mapSpeedToDuty(__currentLogicalSpeed), __isDirectionClockwise);

            // Update LED timing to match the current physical speed during the ramp
            applySpeedSyncLookup(__currentLogicalSpeed);

            // Check if ramp is complete
            if (__currentLogicalSpeed == __targetLogicalSpeed) {
                // If a reversal was triggered, the first ramp-down to the intermediate speed is complete.
                // Now, start the ramp-up in the other direction.
                if (__reverseAfterRampDown) {
                    __isDirectionClockwise = !__isDirectionClockwise;
                    __targetLogicalSpeed = __speedSetting; // Ramp up to the desired speed setting
                    applySpeedSyncLookup(__targetLogicalSpeed);
                    updateRampTiming();
                    __motorState = __MOTOR_RAMPING_UP;
                    __reverseAfterRampDown = false;
                    __isMotorRunning = true;
                } else {
                    __motorState = __MOTOR_IDLE;
                    if (__currentLogicalSpeed == 0) {
                        __isMotorRunning = false;
                        __speedSetting = __LOGICAL_INITIAL_SPEED; // Reset for next start
                        __onboard_led[0] = CRGB(50, 0, 0); // Red when stopped
                        FastLED.show();
                    }
                    log_t("Ramp complete. Current Speed: %d", __currentLogicalSpeed);
                    log_t("Ramp complete. %s, From %d to %d in %lu millis", 
                          __isDirectionClockwise ? "Clockwise" : "Counter-Clockwise",
                          __rampStartSpeed, __currentLogicalSpeed, 
                          millis() - __rampStartTime);
                }
            }
        }
    }

    // Priority 1: Long Press. This is the highest priority and cancels any pending clicks.
    if (M5.BtnA.pressedFor(2000)) {
        // Only trigger a stop if the motor is running and not already in the process of stopping.
        if (__isMotorRunning && !(__motorState == __MOTOR_RAMPING_DOWN && __targetLogicalSpeed == 0)) {
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