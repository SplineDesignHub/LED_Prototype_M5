#include <M5Unified.h>
#include <Wire.h>
#include "angle8_control.h"
#include "led_control.h"

// =============== DEBUG MODE ===============
// Uncomment to enable LED debug/test mode
// #define LED_DEBUG_MODE

// =============== GLOBAL VARIABLES ===============
M5_ANGLE8 angle8;
bool angle8_found = false;
AngleInputs angleInputs;
CRGB leds[NUM_LEDS];

// =============== DISPLAY STATE ===============
bool display_needsRedraw = true;
uint8_t last_mode = 255;

// =============== FUNCTION DECLARATIONS ===============
void updateDisplay();
void runLEDDebug();

// =============== SETUP ===============
void setup() {
    auto cfg = M5.config();
    cfg.serial_baudrate = 115200;
    M5.begin(cfg);
    M5.Display.setTextSize(2);

    Serial.println("\n=============================");
    Serial.println("  NeoPixel LED Controller");
    Serial.println("=============================\n");

#ifdef LED_DEBUG_MODE
    Serial.println("*** LED DEBUG MODE ***");
    Serial.printf("LED_PIN: %d\n", LED_PIN);
    Serial.printf("NUM_LEDS: %d\n", NUM_LEDS);

    M5.Display.fillScreen(BLACK);
    M5.Display.setCursor(0, 10);
    M5.Display.setTextColor(YELLOW);
    M5.Display.println("LED DEBUG MODE");
    M5.Display.printf("PIN: %d\n", LED_PIN);
    M5.Display.printf("NUM: %d\n", NUM_LEDS);

    // Initialize FastLED
    Serial.println("Initializing FastLED...");
#ifdef USE_NEOPIXEL
    FastLED.addLeds<NEOPIXEL, LED_PIN>(leds, NUM_LEDS);
#else
    FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, NUM_LEDS);
#endif
    FastLED.setBrightness(255);
    Serial.println("FastLED initialized.");

    // Test 1: All RED
    M5.Display.setCursor(0, 80);
    M5.Display.println("Test: RED");
    Serial.println("Setting all LEDs to RED...");
    for(int i = 0; i < NUM_LEDS; i++) {
        leds[i] = CRGB::Red;
    }
    FastLED.show();
    Serial.println("RED sent. Check LEDs.");
    delay(2000);

    // Test 2: All GREEN
    M5.Display.fillRect(0, 80, 320, 30, BLACK);
    M5.Display.setCursor(0, 80);
    M5.Display.println("Test: GREEN");
    Serial.println("Setting all LEDs to GREEN...");
    for(int i = 0; i < NUM_LEDS; i++) {
        leds[i] = CRGB::Green;
    }
    FastLED.show();
    Serial.println("GREEN sent. Check LEDs.");
    delay(2000);

    // Test 3: All BLUE
    M5.Display.fillRect(0, 80, 320, 30, BLACK);
    M5.Display.setCursor(0, 80);
    M5.Display.println("Test: BLUE");
    Serial.println("Setting all LEDs to BLUE...");
    for(int i = 0; i < NUM_LEDS; i++) {
        leds[i] = CRGB::Blue;
    }
    FastLED.show();
    Serial.println("BLUE sent. Check LEDs.");
    delay(2000);

    // Test 4: All WHITE (full power)
    M5.Display.fillRect(0, 80, 320, 30, BLACK);
    M5.Display.setCursor(0, 80);
    M5.Display.println("Test: WHITE");
    Serial.println("Setting all LEDs to WHITE...");
    for(int i = 0; i < NUM_LEDS; i++) {
        leds[i] = CRGB::White;
    }
    FastLED.show();
    Serial.println("WHITE sent. Check LEDs.");
    delay(2000);

    M5.Display.fillRect(0, 80, 320, 30, BLACK);
    M5.Display.setCursor(0, 80);
    M5.Display.setTextColor(GREEN);
    M5.Display.println("Debug done!");
    M5.Display.setTextColor(WHITE);
    M5.Display.setCursor(0, 120);
    M5.Display.println("Check Serial for info");

    Serial.println("\n=== DEBUG COMPLETE ===");
    Serial.println("If no LEDs lit up, check:");
    Serial.println("1. Wiring: Data->G8, GND->GND, 5V->5V");
    Serial.println("2. LED strip direction (arrow points away from M5)");
    Serial.println("3. Power supply sufficient");
    Serial.println("4. Try different LED_TYPE in led_control.h");
    Serial.println("   (WS2812B, WS2811, NEOPIXEL, SK6812)");

    return;  // Stay in debug mode
#endif

    // Initialize 8Angle
    init8Angle();

    // Show 8Angle status on display
    M5.Display.fillScreen(BLACK);
    M5.Display.setCursor(0, 10);
    if (angle8_found) {
        M5.Display.setTextColor(GREEN);
        M5.Display.println("8Angle: OK");
        Serial.println("8Angle connected!");
    } else {
        M5.Display.setTextColor(RED);
        M5.Display.println("8Angle: FAIL");
        Serial.println("8Angle NOT connected!");
    }
    M5.Display.setTextColor(WHITE);

    delay(500);

    // Initialize LEDs
    initLEDs();
    M5.Display.setCursor(0, 40);
    M5.Display.setTextColor(GREEN);
    M5.Display.println("LEDs: OK");
    M5.Display.setTextColor(WHITE);

    delay(500);

    // Show mode config
    M5.Display.setCursor(0, 70);
    M5.Display.setTextColor(CYAN);
#ifdef ROUND_MODE_ROTATION
    M5.Display.println("Round: Rotation");
    Serial.println("Round Mode: Rotation");
#else
    M5.Display.println("Round: Position");
    Serial.println("Round Mode: Position");
#endif

    M5.Display.setCursor(0, 95);
#ifdef GRADIENT_MODE_SHIFT
    M5.Display.println("Gradient: Shift");
    Serial.println("Gradient Mode: Shift");
#else
    M5.Display.println("Gradient: Clamp");
    Serial.println("Gradient Mode: Clamp");
#endif
    M5.Display.setTextColor(WHITE);

    delay(1000);
    M5.Display.fillScreen(BLACK);
    display_needsRedraw = true;

    Serial.println("\nStarting main loop...\n");
}

// =============== DISPLAY UPDATE ===============
void updateDisplay() {
    static uint32_t lastValueUpdate = 0;
    static uint32_t lastPreviewUpdate = 0;
    static int lastPosMarker = -1;  // Track last marker position
    static int lastGreenWidth = -1; // Track last width bar position
    const uint32_t VALUE_UPDATE_MS = 50;    // Update values at 20Hz
    const uint32_t PREVIEW_UPDATE_MS = 50;  // Update preview at 20Hz

    uint32_t now = millis();
    bool updateValues = (now - lastValueUpdate >= VALUE_UPDATE_MS);
    bool updatePreview = (now - lastPreviewUpdate >= PREVIEW_UPDATE_MS);

    if (!updateValues && !updatePreview) return;

    // Check if mode changed
    uint8_t current_mode = angleInputs.mode_switch;
    if (current_mode != last_mode) {
        M5.Display.fillScreen(BLACK);
        display_needsRedraw = true;
        last_mode = current_mode;
        updateValues = true;
        updatePreview = true;
    }

    // Draw static labels (only once)
    if (display_needsRedraw) {
        M5.Display.setTextSize(2);

        // Mode header
        M5.Display.setCursor(0, 0);
        M5.Display.setTextColor(CYAN);
        if (current_mode == 0) {
            M5.Display.println("ROUND MODE");
        } else {
            M5.Display.println("BAR MODE");
        }
        M5.Display.setTextColor(WHITE);

        // Labels
        M5.Display.setCursor(0, 30);
        M5.Display.print("C1:");
        M5.Display.setCursor(0, 55);
        M5.Display.print("C2:");
        M5.Display.setCursor(0, 85);
        M5.Display.print("Width:");
        M5.Display.setCursor(0, 110);

#ifdef ROUND_MODE_ROTATION
        if (current_mode == 0) {
            M5.Display.print("Rot:");
        } else {
            M5.Display.print("Pos:");
        }
#else
        M5.Display.print("Pos:");
#endif

        display_needsRedraw = false;
    }

    // Update values (clear and redraw value areas)
    if (updateValues) {
        lastValueUpdate = now;

        // Get current values
        uint8_t r1 = mapToRGB(angleInputs.ch1);
        uint8_t g1 = mapToRGB(angleInputs.ch2);
        uint8_t b1 = mapToRGB(angleInputs.ch3);
        uint8_t r2 = mapToRGB(angleInputs.ch4);
        uint8_t g2 = mapToRGB(angleInputs.ch5);
        uint8_t b2 = mapToRGB(angleInputs.ch6);
        float width = mapToNormalized(angleInputs.ch7);
        float pos = mapToNormalized(angleInputs.ch8);

        // Color 1 value (use background color to avoid flicker)
        M5.Display.setTextColor(WHITE, BLACK);
        M5.Display.setCursor(40, 30);
        M5.Display.printf("R%3d G%3d B%3d", r1, g1, b1);

        // Color 1 preview box
        M5.Display.fillRect(260, 30, 50, 20, M5.Display.color565(r1, g1, b1));

        // Color 2 value
        M5.Display.setCursor(40, 55);
        M5.Display.printf("R%3d G%3d B%3d", r2, g2, b2);

        // Color 2 preview box
        M5.Display.fillRect(260, 55, 50, 20, M5.Display.color565(r2, g2, b2));

        // Width value
        M5.Display.setCursor(80, 85);
        M5.Display.printf("%.2f", width);

        // Width bar - only redraw if width changed
        int greenWidth = (int)(150 * width);

        if (greenWidth != lastGreenWidth) {
            int greyStart = 160 + greenWidth;
            int greyWidth = 150 - greenWidth;

            M5.Display.fillRect(160, 85, greenWidth, 16, GREEN);       // Left: green
            M5.Display.fillRect(greyStart, 85, greyWidth, 16, DARKGREY); // Right: grey

            lastGreenWidth = greenWidth;
        }

        // Position/Rotation value
        M5.Display.setCursor(60, 110);
        M5.Display.printf("%.2f", pos);

        // Position bar - only redraw if marker position changed
        int posMarker = 160 + (int)(150 * pos);

        if (posMarker != lastPosMarker) {
            int markerWidth = 6;

            // Left grey (before marker)
            int leftWidth = posMarker - 3 - 160;
            if (leftWidth > 0) {
                M5.Display.fillRect(160, 110, leftWidth, 16, DARKGREY);
            }

            // Yellow marker
            M5.Display.fillRect(posMarker - 3, 110, markerWidth, 16, YELLOW);

            // Right grey (after marker)
            int rightStart = posMarker + 3;
            int rightWidth = 160 + 150 - rightStart;
            if (rightWidth > 0) {
                M5.Display.fillRect(rightStart, 110, rightWidth, 16, DARKGREY);
            }

            lastPosMarker = posMarker;
        }
    }

    // Gradient preview bar at bottom (expensive, update less frequently)
    if (updatePreview) {
        lastPreviewUpdate = now;

        // Get current values for preview
        uint8_t r1 = mapToRGB(angleInputs.ch1);
        uint8_t g1 = mapToRGB(angleInputs.ch2);
        uint8_t b1 = mapToRGB(angleInputs.ch3);
        uint8_t r2 = mapToRGB(angleInputs.ch4);
        uint8_t g2 = mapToRGB(angleInputs.ch5);
        uint8_t b2 = mapToRGB(angleInputs.ch6);
        float width = mapToNormalized(angleInputs.ch7);
        float pos = mapToNormalized(angleInputs.ch8);
        int previewY = 145;
        int previewHeight = 30;
        // M5.Display.fillRect(0, previewY, 320, previewHeight, BLACK);

        // Draw simplified gradient preview
        for (int x = 0; x < 320; x++) {
            float virtualPos;
            if (current_mode == 1) {
                // Bar mode preview
                virtualPos = (float)x / 319.0f;
            } else {
                // Round mode preview (symmetric)
                if (x < 160) {
                    virtualPos = (float)x / 159.0f;
                } else {
                    virtualPos = (float)(319 - x) / 159.0f;
                }
            }

            float blendFactor;

#ifdef GRADIENT_MODE_SHIFT
        // SHIFT mode preview
        if (width < 0.001f) {
            if (pos <= 0.001f) {
                blendFactor = 1.0f;
            } else if (pos >= 0.999f) {
                blendFactor = 0.0f;
            } else {
                blendFactor = (virtualPos < pos) ? 0.0f : 1.0f;
            }
        } else {
            float shift = (pos - 0.5f) * 2.0f;
            float shiftedPos = virtualPos - shift * (1.0f - width * 0.5f);
            float gradientStart = 0.5f - width * 0.5f;
            float gradientEnd = 0.5f + width * 0.5f;

            if (shiftedPos <= gradientStart) {
                blendFactor = 0.0f;
            } else if (shiftedPos >= gradientEnd) {
                blendFactor = 1.0f;
            } else {
                blendFactor = (shiftedPos - gradientStart) / width;
            }
            blendFactor = constrain(blendFactor, 0.0f, 1.0f);
        }
#else
        // CLAMP mode preview
        float halfWidth = width / 2.0f;
        float blendStart = constrain(pos - halfWidth, 0.0f, 1.0f);
        float blendEnd = constrain(pos + halfWidth, 0.0f, 1.0f);

        if (width < 0.001f) {
            if (pos <= 0.001f) {
                blendFactor = 1.0f;
            } else if (pos >= 0.999f) {
                blendFactor = 0.0f;
            } else {
                blendFactor = (virtualPos < pos) ? 0.0f : 1.0f;
            }
        } else if (virtualPos <= blendStart) {
            blendFactor = 0.0f;
        } else if (virtualPos >= blendEnd) {
            blendFactor = 1.0f;
        } else {
            blendFactor = (virtualPos - blendStart) / (blendEnd - blendStart);
        }
#endif

        uint8_t pr = r1 + (r2 - r1) * blendFactor;
        uint8_t pg = g1 + (g2 - g1) * blendFactor;
        uint8_t pb = b1 + (b2 - b1) * blendFactor;

            M5.Display.drawFastVLine(x, previewY, previewHeight, M5.Display.color565(pr, pg, pb));
        }

        // Mode indicators at bottom
        // M5.Display.fillRect(0, 180, 320, 40, BLACK);
        M5.Display.setCursor(0, 185);
        M5.Display.setTextSize(1);
        M5.Display.setTextColor(DARKGREY);

        // Line 1: Round mode option
#ifdef ROUND_MODE_ROTATION
        M5.Display.print("Round: Rotation");
#else
        M5.Display.print("Round: Position");
#endif

        // Line 2: Gradient mode option
        M5.Display.setCursor(0, 200);
#ifdef GRADIENT_MODE_SHIFT
        M5.Display.print("Gradient: Shift");
#else
        M5.Display.print("Gradient: Clamp");
#endif

        M5.Display.setTextSize(2);
        M5.Display.setTextColor(WHITE);
    }
}

// =============== MAIN LOOP ===============
void loop() {
#ifdef LED_DEBUG_MODE
    // Debug mode: cycle colors continuously
    static uint8_t hue = 0;
    for(int i = 0; i < NUM_LEDS; i++) {
        leds[i] = CHSV(hue + (i * 10), 255, 255);
    }
    FastLED.show();
    hue++;
    delay(50);
    return;
#endif

    M5.update();

    // Read 8Angle inputs
    read8AngleInputs();

    // Get LED parameters
    CRGB color1 = CRGB(
        mapToRGB(angleInputs.ch1),
        mapToRGB(angleInputs.ch2),
        mapToRGB(angleInputs.ch3)
    );
    CRGB color2 = CRGB(
        mapToRGB(angleInputs.ch4),
        mapToRGB(angleInputs.ch5),
        mapToRGB(angleInputs.ch6)
    );
    float width = mapToNormalized(angleInputs.ch7);
    float pos = mapToNormalized(angleInputs.ch8);
    bool isRoundMode = (angleInputs.mode_switch == 0);

    // Update LEDs
    updateLEDs(color1, color2, width, pos, isRoundMode);

    // Update display
    updateDisplay();

    // Small delay for stability
    delay(1000/120);
}
