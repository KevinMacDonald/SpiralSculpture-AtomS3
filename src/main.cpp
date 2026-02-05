#include <M5Unified.h>

void setup() {
    auto cfg = M5.config();
    cfg.serial_baudrate = 115200; // M5Unified will handle Serial.begin
    M5.begin(cfg);

    // Set initial brightness (0-255)
    M5.Led.setBrightness(60);
    
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
                M5.Led.setColor(0, 0, 0, 255); // Blue
            } else {
                M5.Led.setColor(0, 0, 255, 0); // Green
            }
        } else {
            M5.Led.setColor(0, 0, 0, 0); // Off
        }
    }

    if (M5.BtnA.wasPressed()) {
        Serial.println("Button Pressed!");
    }
}
