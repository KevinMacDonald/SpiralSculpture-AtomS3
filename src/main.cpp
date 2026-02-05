#include <M5Unified.h>

#define LED_PIN 35

// Atomic H-Driver Pin Definitions
const int IN1_PIN = 6;
const int IN2_PIN = 7;

// PWM Settings
const int freq = 10000;
const int resolution = 10; // 0-1023
const int ledChannel1 = 0;
const int ledChannel2 = 1;

bool direction = true;
int counter = 0;

void setup() {
    auto cfg = M5.config();
    cfg.serial_baudrate = 115200;
    M5.begin(cfg);
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
}

void loop() {
    M5.update(); // Required for button state updates

    counter++;

    Serial.println("bbbMy Loop:" + String(counter));


    // Toggle motor on button press
    if (M5.BtnA.wasPressed()) {
        if (direction) {
            neopixelWrite(LED_PIN, 0, 0, 50); 
            ledcWrite(ledChannel1, 500); // 50% duty cycle Forward
            ledcWrite(ledChannel2, 0);
        } else {
            neopixelWrite(LED_PIN, 0, 50, 0); 
            ledcWrite(ledChannel1, 0);
            ledcWrite(ledChannel2, 500); // 50% duty cycle Reverse
        }
        direction = !direction;
    }

    // Stop motor on long press
    if (M5.BtnA.pressedFor(2000)) {
        neopixelWrite(LED_PIN, 50, 0, 0); 
        ledcWrite(ledChannel1, 0);
        ledcWrite(ledChannel2, 0);
    }

    delay(100);
}