#include <M5Unified.h>
#include <Arduino.h>

#define LED_PIN 35

// Motor Pins (AtomS3 Bottom Header mapping for H-Driver)
// IN1 -> G6, IN2 -> G5
#define MOTOR_IN1 6
#define MOTOR_IN2 5

// PWM Settings
#define PWM_FREQ 30000 // 30kHz to reduce motor noise
#define PWM_RES 8      // 8-bit resolution (0-255)
#define PWM_CH1 0
#define PWM_CH2 1

void setup() {
    auto cfg = M5.config();
    cfg.serial_baudrate = 115200; // M5Unified will handle Serial.begin
    M5.begin(cfg);

    // Setup Motor PWM channels
    ledcSetup(PWM_CH1, PWM_FREQ, PWM_RES);
    ledcSetup(PWM_CH2, PWM_FREQ, PWM_RES);
    
    // Attach pins to channels
    ledcAttachPin(MOTOR_IN1, PWM_CH1);
    ledcAttachPin(MOTOR_IN2, PWM_CH2);
    
    Serial.println("AtomS3 Lite Motor Control Ready");
}

void loop() {
    M5.update(); // Update button states

    // 1. Rotate Clockwise
    Serial.println("Rotating Clockwise");
    neopixelWrite(LED_PIN, 0, 20, 0); // Green
    ledcWrite(PWM_CH1, 200); // Speed (0-255)
    ledcWrite(PWM_CH2, 0);
    delay(3000);

    // 2. Stop/Pause
    Serial.println("Stopping");
    neopixelWrite(LED_PIN, 20, 0, 0); // Red
    ledcWrite(PWM_CH1, 0);
    ledcWrite(PWM_CH2, 0);
    delay(2000);

    // 3. Rotate Counter-Clockwise
    Serial.println("Rotating Counter-Clockwise");
    neopixelWrite(LED_PIN, 0, 0, 20); // Blue
    ledcWrite(PWM_CH1, 0);
    ledcWrite(PWM_CH2, 200); // Speed (0-255)
    delay(3000);

    // 4. Stop/Pause
    Serial.println("Stopping");
    neopixelWrite(LED_PIN, 20, 0, 0); // Red
    ledcWrite(PWM_CH1, 0);
    ledcWrite(PWM_CH2, 0);
    delay(2000);
}
