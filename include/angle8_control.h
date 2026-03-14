#ifndef ANGLE8_CONTROL_H
#define ANGLE8_CONTROL_H

#include "M5_ANGLE8.h"
#include <Arduino.h>
#include <Wire.h>
#include "pins_arduino.h"

// =============== 8ANGLE INPUT STRUCTURE ===============
// Channel mapping for LED control:
// CH1-3: Color 1 RGB
// CH4-6: Color 2 RGB
// CH7: Blend width
// CH8: Position (bar mode) / Position or Rotation (round mode, see led_control.h)
// Switch: Mode (0 = Round, 1 = Bar)

struct AngleInputs {
    bool mode_switch = 0;   // 0 = Round mode, 1 = Bar mode
    int16_t ch1 = 0;        // Color 1 Red
    int16_t ch2 = 0;        // Color 1 Green
    int16_t ch3 = 0;        // Color 1 Blue
    int16_t ch4 = 0;        // Color 2 Red
    int16_t ch5 = 0;        // Color 2 Green
    int16_t ch6 = 0;        // Color 2 Blue
    int16_t ch7 = 0;        // Blend width
    int16_t ch8 = 0;        // Position / Rotation
};

extern M5_ANGLE8 angle8;
extern bool angle8_found;
extern AngleInputs angleInputs;

// =============== FUNCTION DECLARATIONS ===============
bool init8Angle();
void read8AngleInputs();

// Mapping functions for LED control
uint8_t mapToRGB(int16_t angle_value);
float mapToNormalized(int16_t angle_value);

// =============== IMPLEMENTATION ===============

inline bool init8Angle() {
    Wire.begin(G2, G1, 400000);
    Serial.println("Initializing 8Angle...");

    if (angle8.begin()) {
        angle8_found = true;
        Serial.println("✓ 8Angle initialized!");
        return true;
    } else {
        angle8_found = false;
        Serial.println("✗ 8Angle initialization FAILED!");
        return false;
    }
}

inline void read8AngleInputs() {
    if (!angle8_found) {
        return;
    }

    // Read digital input for mode switch
    uint8_t digital_inputs = angle8.getDigitalInput();
    angleInputs.mode_switch = digital_inputs & 0x01;  // 0 = Round, 1 = Bar

    // Read all 8 analog channels
    angleInputs.ch1 = angle8.getAnalogInput(0);  // Color 1 Red
    angleInputs.ch2 = angle8.getAnalogInput(1);  // Color 1 Green
    angleInputs.ch3 = angle8.getAnalogInput(2);  // Color 1 Blue
    angleInputs.ch4 = angle8.getAnalogInput(3);  // Color 2 Red
    angleInputs.ch5 = angle8.getAnalogInput(4);  // Color 2 Green
    angleInputs.ch6 = angle8.getAnalogInput(5);  // Color 2 Blue
    angleInputs.ch7 = angle8.getAnalogInput(6);  // Blend width
    angleInputs.ch8 = angle8.getAnalogInput(7);  // Position / Rotation

    // Debug print
    static uint32_t lastPrint = 0;
    if (millis() - lastPrint > 500) {
        lastPrint = millis();
        Serial.printf("Mode:%d R1:%d G1:%d B1:%d R2:%d G2:%d B2:%d W:%.2f P:%.2f\n",
                      angleInputs.mode_switch,
                      mapToRGB(angleInputs.ch1),
                      mapToRGB(angleInputs.ch2),
                      mapToRGB(angleInputs.ch3),
                      mapToRGB(angleInputs.ch4),
                      mapToRGB(angleInputs.ch5),
                      mapToRGB(angleInputs.ch6),
                      mapToNormalized(angleInputs.ch7),
                      mapToNormalized(angleInputs.ch8));
    }
}

/**
 * Map 8Angle value (0-254, reversed) to RGB value (0-255)
 */
inline uint8_t mapToRGB(int16_t angle_value) {
    // 8Angle outputs 0-254, reversed (254 = min, 0 = max)
    float normalized = (254.0f - angle_value) / 254.0f;
    normalized = constrain(normalized, 0.0f, 1.0f);
    return (uint8_t)(normalized * 255.0f);
}

/**
 * Map 8Angle value (0-254, reversed) to normalized float (0.0 - 1.0)
 */
inline float mapToNormalized(int16_t angle_value) {
    float normalized = (254.0f - angle_value) / 254.0f;
    return constrain(normalized, 0.0f, 1.0f);
}

#endif // ANGLE8_CONTROL_H
