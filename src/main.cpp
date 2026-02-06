#include <M5Unified.h>
#include <Arduino.h>

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
const int DEFAULT_RAMP_DURATION_MS = 2000; // Default duration for a full ramp
int currentRampDuration = DEFAULT_RAMP_DURATION_MS; // Variable to change ramp duration (in milliseconds)

// Auto-reverse settings
unsigned long autoReverseDelay = 0; // Time in ms. Disabled if < 5000.
unsigned long lastButtonReleaseTime = 0;

bool isDirectionClockwise = true;
bool isMotorRunning = false;
int speed = LOGICAL_INITIAL_SPEED; // Current LOGICAL motor speed.


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
    // Map the logical speed (1-1000) to the physical PWM duty range (MIN to MAX)
    return map(logicalSpeed, 1, LOGICAL_MAX_SPEED, PHYSICAL_MIN_SPEED, PHYSICAL_MAX_SPEED);
}

/**
 * @brief Smoothly ramps the motor speed from a starting duty cycle to an ending one.
 * This is a blocking function.
 * 
 * @param fromDuty The starting PWM duty cycle.
 * @param toDuty The ending PWM duty cycle.
 * @param clockwise The direction of rotation.
 */
void rampMotor(int fromSpeed, int toSpeed, bool clockwise) {
    if (fromSpeed == toSpeed) {
        setMotorDuty(mapSpeedToDuty(toSpeed), clockwise);
        return;
    }

    int step = (fromSpeed < toSpeed) ? RAMP_STEP : -RAMP_STEP;
    int totalSpeedChange = abs(toSpeed - fromSpeed);
    int numSteps = totalSpeedChange / RAMP_STEP;

    if (numSteps == 0) {
        setMotorDuty(mapSpeedToDuty(toSpeed), clockwise);
        return;
    }

    // Calculate delay per step to achieve the desired total ramp duration.
    unsigned long delayPerStep = currentRampDuration / numSteps;
    if (delayPerStep == 0) delayPerStep = 1; // Ensure a minimum delay of 1ms

    for (int s = fromSpeed; (step > 0) ? (s <= toSpeed) : (s >= toSpeed); s += step) {
        setMotorDuty(mapSpeedToDuty(s), clockwise);
        delay(delayPerStep);
    }
    setMotorDuty(mapSpeedToDuty(toSpeed), clockwise); // Ensure final speed is set
}

/**
 * @brief Reverses the motor's direction with a smooth ramp-down and ramp-up.
 * This is a blocking function.
 */
void reverseDirection() {
    Serial.println("Reversing direction smoothly.");

    int currentLogicalSpeed = isMotorRunning ? speed : 0;

    // 1. Ramp down from current speed to zero.
    rampMotor(currentLogicalSpeed, 0, isDirectionClockwise);

    // 2. Toggle direction state.
    isDirectionClockwise = !isDirectionClockwise;

    // 3. Update LED and motor state.
    neopixelWrite(LED_PIN, 0, isDirectionClockwise ? 50 : 0, isDirectionClockwise ? 0 : 50);
    isMotorRunning = true;

    // 4. Ramp up from zero to target speed in the new direction.
    rampMotor(0, speed, isDirectionClockwise);
}

void smoothStopMotor() {
    Serial.println("Ramping down to stop...");
    rampMotor(speed, 0, isDirectionClockwise);
    setMotorDuty(0, isDirectionClockwise); // Ensure motor is fully off
    isMotorRunning = false;
    speed = LOGICAL_INITIAL_SPEED; // Reset speed to default when motor is stopped.
}

void setup() {
    auto cfg = M5.config();
    cfg.serial_baudrate = 115200;
    M5.begin(cfg);

    // Reset pins to ensure no other peripheral is holding them
    gpio_reset_pin((gpio_num_t)IN1_PIN);
    gpio_reset_pin((gpio_num_t)IN2_PIN);

    delay(1000); // Give serial monitor time to connect

    Serial.println("System Initialized");

    // Configure PWM for H-Bridge
    ledcSetup(ledChannel1, freq, resolution);
    ledcSetup(ledChannel2, freq, resolution);
    ledcAttachPin(IN1_PIN, ledChannel1);
    ledcAttachPin(IN2_PIN, ledChannel2);
    
    // Initial State: Stopped
    ledcWrite(ledChannel1, 0);
    ledcWrite(ledChannel2, 0);
    isMotorRunning = false;

    autoReverseDelay = 30000; // Example: Enable auto-reverse after 6 seconds. Set to 0 to disable.

    neopixelWrite(LED_PIN, 255, 255, 255);
}


void loop() {
    M5.update(); // Required for button state updates

    // Priority 1: Long Press. This is the highest priority and cancels any pending clicks.
    if (M5.BtnA.pressedFor(2000)) {
        if (isMotorRunning) { // Only stop if it's currently running
            neopixelWrite(LED_PIN, 50, 0, 0); // Red
            smoothStopMotor();
            // Reset the auto-reverse timer after the action is complete.
            lastButtonReleaseTime = millis();
        }
    }
    else if (M5.BtnA.wasSingleClicked()) {
        reverseDirection();
        // Reset the auto-reverse timer after the action is complete.
        lastButtonReleaseTime = millis();
    }
    else if (M5.BtnA.wasDoubleClicked()) {
        Serial.println("Double Click: Increasing speed smoothly.");
        
        int oldSpeed = speed;
        int newSpeed = min(LOGICAL_MAX_SPEED, speed + LOGICAL_SPEED_INCREMENT);
        speed = newSpeed; // Update the global target speed

        Serial.print("New speed: ");
        Serial.println(speed);

        if (isMotorRunning) {
            // Ramp smoothly from the old speed to the new speed.
            rampMotor(oldSpeed, newSpeed, isDirectionClockwise);
        }
        // Reset the auto-reverse timer after the action is complete.
        lastButtonReleaseTime = millis();
    }

    // Auto-reverse logic
    if (isMotorRunning && autoReverseDelay >= 5000) {
        if (millis() - lastButtonReleaseTime > autoReverseDelay) {
            Serial.println("Auto-reversing due to inactivity...");
            reverseDirection();
            // After auto-reversing, reset the timer to prevent it from firing again immediately.
            lastButtonReleaseTime = millis();
        }
    }
}