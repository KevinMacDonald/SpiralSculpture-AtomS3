#include <M5Unified.h>
#include <Arduino.h>

#define LED_PIN 35

void setup() {
    auto cfg = M5.config();
    cfg.serial_baudrate = 115200; // M5Unified will handle Serial.begin
    M5.begin(cfg);

    // Using raw neopixelWrite because M5.Led is not responding
    
    Serial.println("AtomS3 Lite M5Unified Ready");
}

void loop() {
    M5.update(); // Update button states

    // Simple Blink Logic
    static uint32_t last_ms = 0;
    static bool state = false;

    if (millis() - last_ms > 500) {
        last_ms = millis();
        state = !state;
        
        if (state) {
            // If button is held, show Blue, otherwise Green
            if (M5.BtnA.isPressed()) {
                neopixelWrite(LED_PIN, 0, 0, 20); // Blue
            } else {
                neopixelWrite(LED_PIN, 0, 20, 0); // Green
            }
        } else {
            neopixelWrite(LED_PIN, 0, 0, 0); // Off
        }
    }

    if (M5.BtnA.wasPressed()) {
        Serial.println("Button Pressed!");
    }
}
