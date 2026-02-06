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
const int MAX_SPEED = 900;
const int MIN_SPEED = 500;
const int SPEED_INCREMENT = 25;
const int INITIAL_SPEED = 600; // The default speed when starting/after stopping.

bool isDirectionClockwise = true;
bool isMotorRunning = false;
int speed = INITIAL_SPEED; // Current motor speed. Range of 500 to 900ish works.

void stopMotor() {
    Serial.println("Stopping");
    ledcWrite(ledChannel1, 0);
    ledcWrite(ledChannel2, 0);
    isMotorRunning = false;
    speed = INITIAL_SPEED; // Reset speed to default when motor is stopped.
}

void runMotorClockwise(int speed)
{
    Serial.print("Clockwise Speed: ");
    Serial.println(speed);
    ledcWrite(ledChannel1, 0);
    ledcWrite(ledChannel2, speed);
    isMotorRunning = true;
}

void runMotorCounterClockwise(int speed)
{
    Serial.print("CounterClockwise Speed: ");
    Serial.println(speed);
    ledcWrite(ledChannel1, speed);
    ledcWrite(ledChannel2, 0);
    isMotorRunning = true;
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
    stopMotor();
    
    neopixelWrite(LED_PIN, 255, 255, 255);
}


void loop() {
    M5.update(); // Required for button state updates

    // Priority 1: Long Press. This is the highest priority and cancels any pending clicks.
    if (M5.BtnA.pressedFor(2000)) {
        if (isMotorRunning) { // Only stop if it's currently running
            neopixelWrite(LED_PIN, 50, 0, 0); //Red
            stopMotor();
        }
    }
    else if (M5.BtnA.wasSingleClicked()) {            
        // Single Click: Toggle motor direction
        Serial.println("Single Click Detected: Toggling direction.");
        isDirectionClockwise = !isDirectionClockwise; // Toggle direction state
        if (isDirectionClockwise) { // true == Clockwise
            neopixelWrite(LED_PIN, 0, 50, 0); //Green
            runMotorClockwise(speed);
        } else { // false == Counter-Clockwise
            neopixelWrite(LED_PIN, 0, 0, 50); //Blue
            runMotorCounterClockwise(speed);
        }
    }
    else if (M5.BtnA.wasDoubleClicked()) {
        // Double Click: Increase speed
        Serial.println("Double Click Detected: Increasing speed.");
        speed = min(MAX_SPEED, speed + SPEED_INCREMENT);
        Serial.print("New speed: ");
        Serial.println(speed);
        if (isMotorRunning) {
            // Re-apply the new speed to the current direction
            if (isDirectionClockwise) runMotorClockwise(speed);
            else runMotorCounterClockwise(speed);
        }
    }
}