#include <Arduino.h>

// AtomS3 Lite Internal LED is on GPIO 35
#define LED_PIN 35

void setup() {
    // No M5.begin() or complex init. Just raw hardware check.
    // The USB Serial is handled automatically by the build flags.
    Serial.begin(115200);
    delay(2000); // Give USB time to enumerate
}

void loop() {
    // Blink Green using built-in ESP32-S3 helper
    // neopixelWrite(pin, red, green, blue);
    neopixelWrite(LED_PIN, 0, 20, 0); 
    delay(500);
    
    // Off
    neopixelWrite(LED_PIN, 0, 0, 0);
    delay(500);

    // Print heartbeat to USB Serial
    Serial.println("AtomS3 Lite is ALIVE");
}
