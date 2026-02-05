#include <M5Unified.h>
#include <Arduino.h>

#define LED_PIN 35

// Motor Pins (Confirmed working)
#define PIN_IN1 5
#define PIN_IN2 6

// PWM Configuration
#define PWM_FREQ 10000 // 10kHz (More reliable for drivers)
#define PWM_RES 8      // 8-bit resolution (0-255)
#define PWM_CH1 0      // Changed channel to avoid conflicts
#define PWM_CH2 1      // Changed channel to avoid conflicts

bool direction    = true;
int counter = 0;

void setup() {
    auto cfg = M5.config();
    cfg.serial_baudrate = 115200;
    M5.begin(cfg);
    
    // Reset pins to ensure no other peripheral is holding them
    gpio_reset_pin((gpio_num_t)PIN_IN1);
    gpio_reset_pin((gpio_num_t)PIN_IN2);

    // Setup PWM channels
    ledcSetup(PWM_CH1, PWM_FREQ, PWM_RES);
    ledcSetup(PWM_CH2, PWM_FREQ, PWM_RES);

    // Attach pins to channels
    ledcAttachPin(PIN_IN1, PWM_CH1);
    ledcAttachPin(PIN_IN2, PWM_CH2);

    // Initialize to 0
    ledcWrite(PWM_CH1, 0);
    ledcWrite(PWM_CH2, 0);    
}


void my_loop()
{
    M5.update();

    counter++;

    Serial.println("bbbMy Loop:" + String(counter));

/*
    if (M5.BtnA.pressedFor(2000)) {
            Serial.println("Long press detected!");
    }
*/
    if (M5.BtnA.wasPressed()) {
        if (direction) {

            Serial.println("Accelerating CW");
            neopixelWrite(LED_PIN, 0, 50, 0); 

            for (int speed = 50; speed <= 255; speed++) {
                ledcWrite(PWM_CH1, speed);
                ledcWrite(PWM_CH2, 0);
                delay(10); // Adjust this delay to change acceleration speed
            }
            delay(1000); // Hold max speed

            // --- 2. Decelerate Clockwise ---
            Serial.println("Decelerating CW");
            for (int speed = 255; speed >= 0; speed--) {
                ledcWrite(PWM_CH1, speed);
                ledcWrite(PWM_CH2, 0);
                delay(10);
            }

        } else {        
            Serial.println("Accelerating CCW");
            neopixelWrite(LED_PIN, 0, 0, 50); 

            for (int speed = 50; speed <= 255; speed++) {
                ledcWrite(PWM_CH1, 0);
                
                ledcWrite(PWM_CH2, speed);
                delay(10);
            }
            delay(500); // Hold max speed

            // --- 4. Decelerate Counter-Clockwise ---
            Serial.println("Decelerating CCW");
            for (int speed = 255; speed >= 0; speed--) {
                ledcWrite(PWM_CH1, 0);
                ledcWrite(PWM_CH2, speed);
                delay(10);
            }
        }
        direction = !direction;
    }

    delay(500);

}


void gemini_loop() {
    Serial.println("Gemini Loop");

    M5.update();

    // --- 1. Accelerate Clockwise (Green) ---
    Serial.println("Accelerating CW");
    neopixelWrite(LED_PIN, 0, 50, 0); 

    for (int speed = 50; speed <= 255; speed++) {
        ledcWrite(PWM_CH1, speed);
        ledcWrite(PWM_CH2, 0);
        delay(10); // Adjust this delay to change acceleration speed
    }
    delay(1000); // Hold max speed

    // --- 2. Decelerate Clockwise ---
    Serial.println("Decelerating CW");
    for (int speed = 255; speed >= 0; speed--) {
        ledcWrite(PWM_CH1, speed);
        ledcWrite(PWM_CH2, 0);
        delay(10);
    }
    
    // Stop (Red)
    neopixelWrite(LED_PIN, 50, 0, 0); 
    ledcWrite(PWM_CH1, 0);
    ledcWrite(PWM_CH2, 0);
    delay(1000);

    // --- 3. Accelerate Counter-Clockwise (Blue) ---
    Serial.println("Accelerating CCW");
    neopixelWrite(LED_PIN, 0, 0, 50); 

    for (int speed = 50; speed <= 255; speed++) {
        ledcWrite(PWM_CH1, 0);
        ledcWrite(PWM_CH2, speed);
        delay(10);
    }
    delay(500); // Hold max speed

    // --- 4. Decelerate Counter-Clockwise ---
    Serial.println("Decelerating CCW");
    for (int speed = 255; speed >= 0; speed--) {
        ledcWrite(PWM_CH1, 0);
        ledcWrite(PWM_CH2, speed);
        delay(10);
    }
    
    // Stop (Red)
    neopixelWrite(LED_PIN, 50, 0, 0); 
    ledcWrite(PWM_CH1, 0);
    ledcWrite(PWM_CH2, 0);
    delay(1000);
}

void loop() 
{
    my_loop();
    //gemini_loop();
}
