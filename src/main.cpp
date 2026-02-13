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
#include "auto_generator.h"
#include <vector>
#include <string>

/*
 * --- Bluetooth Command Reference ---
 * NOTE TO GEMINI: Please update these comments when commands are added, removed, or modified.
 * Commands are sent as strings via the Command Characteristic UUID.
 * 
 * motor_speed:XXX    - Set motor logical speed (0-1000). Example: "motor_speed:500"
 * motor_ramp:XXXX    - Set duration (ms) for a full speed ramp (0 to 1000). Example: "motor_ramp:4000"
 * led_brightness:XX  - Set global master brightness percentage (0-100). Example: "led_brightness:75"
 * led_background:H,B - Set background Hue (0-255) and Brightness % (0-50). Example: "led_background:160,20"
 * led_tails:H,L,C    - Set Comet Hue (0-255), Tail Length (LEDs), and Count. Example: "led_tails:0,15,3"
 * led_cycle_time:MS  - Set absolute time (ms) for one full LED revolution. Example: "led_cycle_time:5000"
 * system_off         - Ramp down motor and blackout LEDs immediately.
 * motor_start        - Start the motor using current speed settings.
 * motor_stop         - Ramp down and stop the motor.
 * system_reset       - Reset all parameters to setup() defaults and start.
 * motor_reverse      - Smoothly ramp down, change direction, and ramp up.
 * motor_speed_up     - Increase target speed setting by 50 units.
 * motor_speed_down   - Decrease target speed setting by 50 units.
 * led_cycle_up       - Increase LED cycle speed by 8% (Enables Manual Sync).
 * led_cycle_down     - Decrease LED cycle speed by 8% (Enables Manual Sync).
 * led_reverse        - Toggle the direction of LED animation cycling.
 * run_script:NAME    - Start a named script (e.g., "run_script:funky").
 * auto_mode:MMM      - Generate and run a script for MMM minutes.
 * auto_mode_debug:MMM - Generate and print a script for MMM minutes without running.
 * hold:XXXX          - (Script only) Wait XXXX ms before next command.
 * led_blink:H,B,U,D,C - Pulse Hue (0-255), Brightness % (0-100), Ramp Up (ms), Ramp Down (ms), Count (0=loop).
 * led_sine_hue:L,H   - Oscillate Comet Hue between L and H (0-255) synced to motor speed.
 * led_rainbow        - Cycle Comet Hue through full rainbow synced to motor speed.
 * led_sine_pulse:L,H - Oscillate Master Brightness between L and H % (0-100) synced to motor speed.
 * led_reset          - Clear all dynamic effects, background, and comets to black.
 */

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
static const int __LOGICAL_INITIAL_SPEED = 600; // The default logical speed
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
static bool __isLedReversed = false;
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

// --- LED Blink State ---
static bool __isBlinkActive = false;
static uint8_t __blinkHue = 0;
static uint8_t __blinkMaxBri = 255;
static unsigned long __blinkUpDuration = 1000;
static unsigned long __blinkDownDuration = 1000;
static unsigned long __blinkStartTime = 0;
static int __blinkTargetCount = 0; // 0 means loop indefinitely

// --- Dynamic LED State (Synced to Motor RPM) ---
static bool __isHueSineActive = false;
static uint8_t __hueSineLow = 0;
static uint8_t __hueSineHigh = 255;
static bool __isRainbowActive = false;
static bool __isPulseSineActive = false;
static uint8_t __pulseSineLow = 0;
static uint8_t __pulseSineHigh = 255;

// --- Scripting Engine State ---
static bool __isScriptRunning = false;
static int __scriptCommandIndex = 0;
static unsigned long __scriptLastCommandTime = 0;
static unsigned long __scriptHoldDuration = 0;
static std::vector<std::string> __activeScriptCommands;

static const std::vector<std::string> __script_funky = {
    "led_reset",
    "hold:10000",
    "led_brightness:75",
    "led_background:0,20",
    "led_rainbow",
    "hold:20001",
    "led_tails:0,15,3",
    "motor_speed:500",
    "hold:3000",
    "led_tails:0,15,3",
    "hold:3000",
    "motor_reverse",
    "hold:4000",
    "motor_speed:700",
    "led_background:32,10",
    "hold:1000",
    "led_background:64,10",
    "hold:1000",
    "led_background:96,10",
    "hold:1000",
    "led_background:0,20",
    "hold:1000",
    "led_tails:128,10,5",
    "motor_reverse",
    "motor_speed:400",
    "hold:5000",
    "motor_speed:1000",
    "led_blink:0,70,200,400,10",
    "motor_speed:400",
    "led_tails:0,15,1",
    "led_cycle_time:8000",
    "hold:3001",
    "led_cycle_time:7000",
    "hold:3002",
    "led_cycle_time:6000",
    "hold:3003",
    "led_cycle_time:5000",
    "hold:3004",
    "led_cycle_time:4000",
    "hold:3005",
    "led_cycle_time:3000",
    "hold:3006",
    "led_cycle_time:2000",
    "hold:3007",
    "led_cycle_time:1000",
    "hold:3008",
    "led_cycle_time:500",
    "hold:7009",
    "led_tails:0,15,2",
    "led_cycle_time:5200",
    "hold:3010",
    "led_tails:0,15,3",
    "led_cycle_time:5200",
    "hold:3011",
    "led_tails:0,15,4",
    "led_cycle_time:5200",
    "hold:3012",
    "led_tails:0,15,5",
    "led_cycle_time:5200",
    "hold:3013",
    "led_reset",
    "led_tails:0,15,3",
    "led_rainbow",
    "hold:10001",
    "led_reset",
    "led_tails:0,15,3",
    "led_sine_hue:0,160",
    "hold:10002",
    "led_reset",
    "led_tails:0,15,3",
    "led_sine_pulse:0,100",
    "hold:10003",
    "led_tails:0,5,10",
    "led_cycle_time:500",
    "hold:10004",
};

// --- Throttled Logging --- DO NOT REMOVE THIS. It is useful to have. 

// --- Throttled Logging --- DO NOT REMOVE THIS. It is useful to have. 
// A helper function for timestamped logs that can be throttled to prevent flooding the serial port.
// Set MIN_LOG_GAP_MS to a value like 50 or 200 to enable throttling.
static unsigned long __MIN_LOG_GAP_MS = 100; // Throttle identical messages within this window.
static unsigned long __lastLogTimestamp = 0;
static char __lastLogBuffer[256] = "";

static void log_t(const char* format, ...) {
    unsigned long now = millis();
    char buf[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buf, sizeof(buf), format, args);
    va_end(args);

    // Log if: throttling is disabled, OR the gap has passed, OR the message is different from the last one.
    if (__MIN_LOG_GAP_MS == 0 || 
        (now - __lastLogTimestamp > __MIN_LOG_GAP_MS) || 
        strcmp(buf, __lastLogBuffer) != 0) {
        
        Serial.printf("%lu ms: %s\n", now, buf);
        __lastLogTimestamp = now;
        strncpy(__lastLogBuffer, buf, sizeof(__lastLogBuffer) - 1);
        __lastLogBuffer[sizeof(__lastLogBuffer) - 1] = '\0';
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

// --- BLE Command Handoff ---
static char __bleCommandBuffer[128];
static volatile bool __bleCommandAvailable = false;

/**
 * @brief Processes a single command string.
 */
void processCommand(std::string value) {
    if (value.length() == 0) return;

    size_t colon_pos = value.find(':');

    if (colon_pos != std::string::npos) {
        std::string cmd = value.substr(0, colon_pos);
        int val = atoi(value.substr(colon_pos + 1).c_str());

        if (cmd == "motor_speed") {
            val = constrain(val, 0, __LOGICAL_MAX_SPEED);
            triggerSetSpeed(val);
        } else if (cmd == "motor_ramp") {
            val = constrain(val, 0, 10000);
            __currentRampDuration = val;
            log_t("Set Motor Ramp Duration: %d", __currentRampDuration);
        } else if (cmd == "led_brightness") {
            __isPulseSineActive = false;
            int brightness_pct = constrain(val, 0, 100);
            __masterBrightness = (uint8_t)((brightness_pct * 255) / 100);
            FastLED.setBrightness(__masterBrightness);
            log_t("LED Master Brightness set to: %d%% (%d/255)", brightness_pct, __masterBrightness);
        } else if (cmd == "led_background") {
            __isBlinkActive = false;
            std::string params = value.substr(colon_pos + 1);
            size_t comma_pos = params.find(',');
            if (comma_pos != std::string::npos) {
                int h = atoi(params.substr(0, comma_pos).c_str());
                int b_pct = atoi(params.substr(comma_pos + 1).c_str());
                __bgHue = (uint8_t)constrain(h, 0, 255);
                __bgBrightness = (uint8_t)((constrain(b_pct, 0, 50) * 255) / 100);
                log_t("LED Background set to Hue: %d, Brightness: %d%% (%d)", __bgHue, b_pct, __bgBrightness);
            }
        } else if (cmd == "led_tails") {
            __isBlinkActive = false;
            std::string params = value.substr(colon_pos + 1);
            size_t comma1 = params.find(',');
            size_t comma2 = params.find(',', comma1 + 1);
            if (comma1 != std::string::npos && comma2 != std::string::npos) {
                int h = atoi(params.substr(0, comma1).c_str());
                int l = atoi(params.substr(comma1 + 1, comma2 - (comma1 + 1)).c_str());
                int c = atoi(params.substr(comma2 + 1).c_str());
                if (c == 0 || (c * l <= __LOGICAL_NUM_LEDS * 0.8)) {
                    __cometHue = (uint8_t)constrain(h, 0, 255);
                    __cometTailLength = max(1, l);
                    __cometCount = max(0, c);
                    log_t("LED Tails set: Hue %d, Length %d, Count %d", __cometHue, __cometTailLength, __cometCount);
                } else {
                    log_t("Tails command ignored: exceeds 80%% of strip.");
                }
            }
        } else if (cmd == "led_cycle_time") {
            __isBlinkActive = false;
            if (val > 0) {
                __isManualLedInterval = true;
                __manualLedIntervalMs = (float)val / (float)__LOGICAL_NUM_LEDS;
                __manualSpeedReference = (__currentLogicalSpeed > 0) ? __currentLogicalSpeed : __speedSetting;
                __ledIntervalMs = __manualLedIntervalMs;
                log_t("LED Manual Sync set at speed %d. Step interval: %.2f ms", __manualSpeedReference, __ledIntervalMs);
            }
        } else if (cmd == "system_off") {
            __isBlinkActive = false;
            __pendingOff = true;
        } else if (cmd == "run_script") {
            std::string scriptName = value.substr(colon_pos + 1);
            if (scriptName == "funky") {
                __activeScriptCommands = __script_funky;
                __scriptCommandIndex = 0;
                __scriptLastCommandTime = millis();
                __scriptHoldDuration = 0;
                __isScriptRunning = true;
                log_t("Script started: funky");
            }
        } else if (cmd == "auto_mode" || cmd == "auto_mode_debug") {
            int duration_minutes = constrain(val, 1, 240); // Constrain to 1min - 4hours

            // Stop any currently running script
            __isScriptRunning = false;

            __activeScriptCommands = AutoGenerator::generateScript(duration_minutes);

            if (cmd == "auto_mode" && !__activeScriptCommands.empty()) {
                __scriptCommandIndex = 0;
                __scriptLastCommandTime = millis();
                __scriptHoldDuration = 0;
                __isScriptRunning = true;
                log_t("Auto-mode script started for %d minutes.", duration_minutes);
            } else {
                log_t("Auto-mode debug script generated for %d minutes. Not executing.", duration_minutes);
            }
        } else if (cmd == "hold") {
            if (__isScriptRunning) {
                __scriptHoldDuration = val;
            }
        } else if (cmd == "led_blink") {
            // led_blink:HHH,BB,XXXX,YYYY,C
            std::string params = value.substr(colon_pos + 1);
            size_t c1 = params.find(',');
            size_t c2 = params.find(',', c1 + 1);
            size_t c3 = params.find(',', c2 + 1);
            if (c1 != std::string::npos && c2 != std::string::npos && c3 != std::string::npos) {
                int h = atoi(params.substr(0, c1).c_str());
                int b = atoi(params.substr(c1 + 1, c2 - (c1 + 1)).c_str());
                int u = atoi(params.substr(c2 + 1, c3 - (c2 + 1)).c_str());
                
                int d = 0;
                int count = 0;
                size_t c4 = params.find(',', c3 + 1);
                if (c4 != std::string::npos) {
                    d = atoi(params.substr(c3 + 1, c4 - (c3 + 1)).c_str());
                    count = atoi(params.substr(c4 + 1).c_str());
                } else {
                    d = atoi(params.substr(c3 + 1).c_str());
                    count = 0;
                }
                
                __blinkHue = (uint8_t)constrain(h, 0, 255);
                __blinkMaxBri = (uint8_t)((constrain(b, 0, 100) * 255) / 100);
                __blinkUpDuration = (unsigned long)max(1UL, (unsigned long)u);
                __blinkDownDuration = (unsigned long)max(1UL, (unsigned long)d);
                __blinkTargetCount = count;
                
                FastLED.clear(true);
                __isBlinkActive = true;
                __blinkStartTime = millis();
                log_t("LED Blink set: Hue %d, MaxBri %d, Up %lu, Down %lu, Count %d", __blinkHue, b, __blinkUpDuration, __blinkDownDuration, __blinkTargetCount);
            }
        } else if (cmd == "led_sine_hue") {
            // led_sine_hue:LOW,HIGH
            std::string params = value.substr(colon_pos + 1);
            size_t c1 = params.find(',');
            if (c1 != std::string::npos) {
                __hueSineLow = (uint8_t)atoi(params.substr(0, c1).c_str());
                __hueSineHigh = (uint8_t)atoi(params.substr(c1 + 1).c_str());
                __isHueSineActive = true;
                __isRainbowActive = false;
                if (__cometCount == 0) __cometCount = 1; // Ensure visibility
                log_t("LED Sine Hue: Range %d-%d (Sync BPM)", __hueSineLow, __hueSineHigh);
            }
        } else if (cmd == "led_sine_pulse") {
            // led_sine_pulse:LOW,HIGH
            std::string params = value.substr(colon_pos + 1);
            size_t c1 = params.find(',');
            if (c1 != std::string::npos) {
                int low_pct = atoi(params.substr(0, c1).c_str());
                int high_pct = atoi(params.substr(c1 + 1).c_str());
                
                __pulseSineLow = (uint8_t)((constrain(low_pct, 0, 100) * 255) / 100);
                __pulseSineHigh = (uint8_t)((constrain(high_pct, 0, 100) * 255) / 100);
                
                __isPulseSineActive = true;
                __isBlinkActive = false;
                // If everything is dark, enable background so the pulse is visible
                if (__bgBrightness == 0 && __cometCount == 0) {
                    __bgBrightness = 76; // Default to 30% floor
                }
                log_t("LED Sine Pulse: Range %d%%-%d%% (Sync BPM)", low_pct, high_pct);
            }
        } else {
            log_t("Unknown command prefix: %s", cmd.c_str());
        }

    } else if (value == "system_off") {
        __isBlinkActive = false;
        __pendingOff = true;
    } else if (value == "led_rainbow") {
        __isRainbowActive = true;
        __isHueSineActive = false;
        if (__cometCount == 0) __cometCount = 1; // Ensure visibility
        log_t("LED Rainbow Mode: Sync BPM");
    } else if (value == "led_reset") {
        __isBlinkActive = false;
        __isHueSineActive = false;
        __isRainbowActive = false;
        __isPulseSineActive = false;
        __bgBrightness = 0;
        __cometCount = 0;
        __isManualLedInterval = false;
        __masterBrightness = 255;
        FastLED.setBrightness(__masterBrightness);
        FastLED.clear(true);
        log_t("LEDs reset to black/static.");
    } else if (value == "motor_start") {
        triggerStart();
    } else if (value == "motor_stop") {
        triggerStop();
    } else if (value == "system_reset") {
        __isBlinkActive = false;
        __isHueSineActive = false;
        __isRainbowActive = false;
        __isPulseSineActive = false;
        __isLedReversed = false;
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
        processCommand("led_rainbow"); // Add led_rainbow after system reset
        log_t("System reset to defaults and started.");
    } else if (value == "motor_reverse") {
        triggerReverse();
    } else if (value == "motor_speed_up") {
        triggerSpeedUp();
    } else if (value == "motor_speed_down") {
        triggerSpeedDown();
    } else if (value == "led_cycle_up") {
        __isBlinkActive = false;
        __isManualLedInterval = true;
        __ledIntervalMs *= 0.92f;
        __manualLedIntervalMs = __ledIntervalMs;
        __manualSpeedReference = (__currentLogicalSpeed > 0) ? __currentLogicalSpeed : __speedSetting;
        log_t("LED Cycle speed UP 8%% (Manual). Interval: %.2f ms", __ledIntervalMs);
    } else if (value == "led_cycle_down") {
        __isBlinkActive = false;
        __isManualLedInterval = true;
        __ledIntervalMs *= 1.08f;
        __manualLedIntervalMs = __ledIntervalMs;
        __manualSpeedReference = (__currentLogicalSpeed > 0) ? __currentLogicalSpeed : __speedSetting;
        log_t("LED Cycle speed DOWN 8%% (Manual). Interval: %.2f ms", __ledIntervalMs);
    } else if (value == "led_reverse") {
        __isLedReversed = !__isLedReversed;
        log_t("LED direction reversed. New state: %s", __isLedReversed ? "Reversed" : "Normal");
    } else {
        log_t("Invalid command format: %s", value.c_str());
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
        if (value.length() == 0) return;

        log_t("BLE Received: %s", value.c_str());

        // Thread-safe handoff to loop() to avoid cross-core race conditions
        if (value.length() < sizeof(__bleCommandBuffer)) {
            strcpy(__bleCommandBuffer, value.c_str());
            __bleCommandAvailable = true;
        }

        pCharacteristic->setValue(value);
    }
};

void setup() {
    auto cfg = M5.config();
    cfg.serial_baudrate = 115200;
    cfg.internal_imu = false; // Disable IMU to prevent "not found" logging on Lite devices
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

    // --- Handle BLE Commands ---
    if (__bleCommandAvailable) {
        std::string cmd = __bleCommandBuffer;
        __bleCommandAvailable = false;

        // Only allow system_reset to interrupt a script
        if (cmd == "system_reset" || cmd == "system_off") {
            __isScriptRunning = false;
            processCommand(cmd);
        } else if (!__isScriptRunning) {
            processCommand(cmd);
        } else {
            log_t("BLE command ignored (Script running): %s", cmd.c_str());
        }
    }

    // --- Script Engine ---
    // Only advance if motor is idle AND any finite blink sequence has finished
    if (__isScriptRunning && __motorState == __MOTOR_IDLE && (__blinkTargetCount == 0 || !__isBlinkActive)) {
        if (millis() - __scriptLastCommandTime >= __scriptHoldDuration) {
            std::string cmd = __activeScriptCommands[__scriptCommandIndex];
            __scriptLastCommandTime = millis();
            __scriptHoldDuration = 0; // Reset hold for the next command
            log_t("Script Executing: %s", cmd.c_str());
            processCommand(cmd);
            __scriptCommandIndex = (__scriptCommandIndex + 1) % __activeScriptCommands.size();
        }
    }

    // --- Handle Pending Off Command ---
    if (__pendingOff) {
        log_t("Processing Off command...");
        triggerStop();         // Start motor ramp down
        __isMotorRunning = false; // Stop LED animation logic
        FastLED.clear(true);    // Blackout all LEDs immediately
        __pendingOff = false;
    }

    // --- LED Strip Animation ---
    // 1. Update dynamic parameters (Sine/Rainbow) for the comet and master brightness
    if (__isRainbowActive || __isHueSineActive || __isPulseSineActive) {
        float revTime = __ledIntervalMs * (float)__LOGICAL_NUM_LEDS;
        // Calculate BPM in Q8.8 fixed point for higher precision beat functions.
        // 60000ms * 256 = 15360000
        uint16_t bpm88 = (revTime > 0) ? (uint16_t)(15360000.0f / revTime) : 0;

        if (__isRainbowActive) {
            __cometHue = beat88(bpm88) >> 8;
        } else if (__isHueSineActive) {
            __cometHue = (uint8_t)beatsin88(bpm88, __hueSineLow, __hueSineHigh);
        }

        if (__isPulseSineActive) {
            FastLED.setBrightness((uint8_t)beatsin88(bpm88, __pulseSineLow, __pulseSineHigh));
        }
    }

    if (__isBlinkActive) {
        unsigned long elapsed = millis() - __blinkStartTime;
        unsigned long totalCycle = __blinkUpDuration + __blinkDownDuration;
        if (totalCycle > 0) {
            // Check if we have reached the target count for finite blinks
            if (__blinkTargetCount > 0 && (elapsed / totalCycle) >= (unsigned long)__blinkTargetCount) {
                __isBlinkActive = false;
                __blinkTargetCount = 0;
                FastLED.clear(true);
            } else {
                unsigned long cyclePos = elapsed % totalCycle;
                uint8_t bri = 0;
                if (cyclePos < __blinkUpDuration) {
                    bri = map(cyclePos, 0, __blinkUpDuration, 0, __blinkMaxBri);
                } else {
                    unsigned long downElapsed = cyclePos - __blinkUpDuration;
                    bri = map(downElapsed, 0, __blinkDownDuration, __blinkMaxBri, 0);
                }
                fill_solid(__leds, __NUM_LEDS, CHSV(__blinkHue, 255, bri));
                FastLED.show();
            }
        }
    } 
    // Standard comet animation runs if blink is inactive and motor is running
    else if (__isMotorRunning && __currentLogicalSpeed > 0) {
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

            // Determine LED cycling direction. Default is forward (incrementing) when motor is CCW.
            // __isLedReversed toggles this behavior.
            bool led_direction_is_forward = !__isDirectionClockwise ^ __isLedReversed;

            if (led_direction_is_forward) {
                __led_position = (__led_position + 1) % __LOGICAL_NUM_LEDS;
            } else {
                __led_position = (__led_position - 1 + __LOGICAL_NUM_LEDS) % __LOGICAL_NUM_LEDS;
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