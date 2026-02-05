#include <M5Unified.h>
#include <Arduino.h>

#define LED_PIN 35
// Motor Pins for Atom H-Driver (IN1/IN2 Mode)
// AtomS3 Lite Bottom Header: G5 and G6
#define MOTOR_IN1 5
#define MOTOR_IN2 6
// Motor Pins for Atom H-Driver (PH/EN Mode)
// Based on testing: G6 is Enable (Speed), G5 is Phase (Direction)
#define PIN_ENABLE 6
#define PIN_PHASE  5

// PWM Settings
#define PWM_FREQ 30000 // 30kHz to reduce motor noise
#define PWM_RES 8      // 8-bit resolution (0-255)
#define PWM_CH1 0
#define PWM_CH2 1
#define PWM_CH_ENABLE 0

void setup() {
    auto cfg = M5.config();
    cfg.serial_baudrate = 115200; // M5Unified will handle Serial.begin
    M5.begin(cfg);

    // Setup Motor PWM channels
    ledcSetup(PWM_CH1, PWM_FREQ, PWM_RES);
    ledcSetup(PWM_CH2, PWM_FREQ, PWM_RES);
    // Setup Enable Pin (PWM for Speed)
    ledcSetup(PWM_CH_ENABLE, PWM_FREQ, PWM_RES);
    ledcAttachPin(PIN_ENABLE, PWM_CH_ENABLE);
    
    // Attach pins to channels
    ledcAttachPin(MOTOR_IN1, PWM_CH1);
    ledcAttachPin(MOTOR_IN2, PWM_CH2);
    // Setup Phase Pin (Digital for Direction)
    pinMode(PIN_PHASE, OUTPUT);
    
    Serial.println("AtomS3 Lite Motor Control: IN1/IN2 Mode");
    Serial.println("AtomS3 Lite Motor Control: PH/EN Mode Corrected");
}

void loop() {
    M5.update(); // Update button states

    // 1. Rotate Direction A
    Serial.println("Direction A (IN1=PWM, IN2=LOW)");
    // 1. Rotate Direction A (Clockwise)
    Serial.println("Direction A (PH=HIGH, EN=PWM)");
    neopixelWrite(LED_PIN, 0, 20, 0); // Green
    
    ledcWrite(PWM_CH1, 200); // Speed
    ledcWrite(PWM_CH2, 0);   // Hold Low
    digitalWrite(PIN_PHASE, HIGH);   // Set Direction A
    ledcWrite(PWM_CH_ENABLE, 200);   // Set Speed
    delay(3000);

    // 2. Stop/Pause
    Serial.println("Stopping");
    neopixelWrite(LED_PIN, 20, 0, 0); // Red
    
    ledcWrite(PWM_CH1, 0);
    ledcWrite(PWM_CH2, 0);
    ledcWrite(PWM_CH_ENABLE, 0);     // Stop
    delay(2000);

    // 3. Rotate Direction B
    Serial.println("Direction B (IN1=LOW, IN2=PWM)");
    // 3. Rotate Direction B (Counter-Clockwise)
    Serial.println("Direction B (PH=LOW, EN=PWM)");
    neopixelWrite(LED_PIN, 0, 0, 20); // Blue
    
    ledcWrite(PWM_CH1, 0);   // Hold Low
    ledcWrite(PWM_CH2, 200); // Speed
    digitalWrite(PIN_PHASE, LOW);    // Set Direction B
    ledcWrite(PWM_CH_ENABLE, 200);   // Set Speed
    delay(3000);

    // 4. Stop/Pause
    Serial.println("Stopping");
    neopixelWrite(LED_PIN, 20, 0, 0); // Red
    
    ledcWrite(PWM_CH1, 0);
    ledcWrite(PWM_CH2, 0);
    ledcWrite(PWM_CH_ENABLE, 0);     // Stop
    delay(2000);
}
