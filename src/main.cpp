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

bool direction = true;

void stopMotor() {
    Serial.println("Stopping");
    ledcWrite(ledChannel1, 0);
    ledcWrite(ledChannel2, 0);
}

void runMotorClockwise(int speed)
{
    Serial.println("Clockwise Speed:" + String(speed));
    ledcWrite(ledChannel1, 0);
    ledcWrite(ledChannel2, speed);
}

void runMotorCounterClockwise(int speed)
{
    Serial.println("CounterClockwise Speed:" + String(speed));
    ledcWrite(ledChannel1, speed);
    ledcWrite(ledChannel2, 0);
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



int speed = 600; //Range of 500 to 900ish works. The upper end gets noisy.

void loop() {
    M5.update(); // Required for button state updates

    // Stop motor on long press. Check this first to override short clicks.
    if (M5.BtnA.pressedFor(2000)) {
        neopixelWrite(LED_PIN, 50, 0, 0); //Red
        stopMotor();        
    }
    // Toggle motor direction on a short click.
    // wasClicked() fires on release and won't trigger if it was a long press.
    else if (M5.BtnA.wasClicked()) {
        if (direction) {
            neopixelWrite(LED_PIN, 0, 0, 50); //Blue
            runMotorCounterClockwise(speed); 
        } else {
            neopixelWrite(LED_PIN, 0, 50, 0); //Green
            runMotorClockwise(speed);
        }
        direction = !direction;
    }
}