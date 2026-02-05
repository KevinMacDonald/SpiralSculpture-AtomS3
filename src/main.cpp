#include <M5Unified.h>
#include <Arduino.h>

#define LED_PIN 35

// Atomic H-Driver Pin Definitions
const int IN1_PIN = 5;
const int IN2_PIN = 6;

// PWM Settings
const int freq = 25000;
const int resolution = 10; // 0-1023
const int ledChannel1 = 0;
const int ledChannel2 = 1;

bool direction = true;
int counter = 0;

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
    
    neopixelWrite(LED_PIN, 255, 255, 255);
}

void loop() {
    M5.update(); // Required for button state updates

    counter++;

    if ((counter % 1000000) == 0) {
        Serial.println("My Loop:" + String(counter));
    }

    // Toggle motor on button press
    if (M5.BtnA.wasPressed()) {
        int speed = 600;
        Serial.println("Speed:" + String(speed));

        if (direction) {
            neopixelWrite(LED_PIN, 0, 0, 50); //Blue

            Serial.println("Forward");
            ledcWrite(ledChannel1, 0);
            ledcWrite(ledChannel2, speed);
            delay(7000);

        } else {
            neopixelWrite(LED_PIN, 0, 50, 0); //Green

            Serial.println("Reverse");
            ledcWrite(ledChannel2, 0);
            ledcWrite(ledChannel1, speed); 
            delay(7000);
        }
        direction = !direction;
    }

    // Stop motor on long press
    if (M5.BtnA.pressedFor(2000)) {
        Serial.println("Stopping");
        neopixelWrite(LED_PIN, 50, 0, 0); //Red
        ledcWrite(ledChannel1, 0);
        ledcWrite(ledChannel2, 0);
    }
}