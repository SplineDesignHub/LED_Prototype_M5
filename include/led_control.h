#ifndef LED_CONTROL_H
#define LED_CONTROL_H

#include <FastLED.h>

// =============== LED CONFIG ===============
#define LED_PIN         8   // Port B on CoreS3-SE
#define NUM_LEDS        46
#define LED_BRIGHTNESS  150

// LED Type options:
// - Use NEOPIXEL for WS2812/WS2812B (most common, GRB order)
// - Use WS2811 for WS2811 strips
// - Use SK6812 for SK6812 strips
#define USE_NEOPIXEL    // Comment this out to use WS2812B with custom color order

// =============== MODE CONFIG ===============
// Comment/uncomment to switch between options:
// Option A: Round mode uses CH8 as rotation (position fixed at 0.5)
// Option B: Round mode uses CH8 as position (same as bar mode)
#define ROUND_MODE_ROTATION  // Uncomment for Option A (rotation)
// Leave commented for Option B (position, same as bar mode)

// =============== GRADIENT MODE CONFIG ===============
// GRADIENT_MODE_CLAMP (default): pos controls blend zone center, width controls zone size
//   - At extremes (pos=0 or pos=1), gradient gets cut off/clamped
//   - When width=1, pos=0: [0]=Color1, gradient ends at middle, rest is Color2
//
// GRADIENT_MODE_SHIFT: pos shifts the gradient midpoint, width always spans full range
//   - When width=1, pos=0: [0]=middle color (50% blend), [45]=Color2
//   - When width=1, pos=0.5: [0]=Color1, [middle]=50%, [45]=Color2 (centered)
//   - When width=1, pos=1: [0]=Color1, [45]=middle color (50% blend)
#define GRADIENT_MODE_SHIFT  // Comment this out for CLAMP mode

// =============== LED ARRAY ===============
extern CRGB leds[NUM_LEDS];

// =============== FUNCTION DECLARATIONS ===============
void initLEDs();
void updateLEDs(CRGB color1, CRGB color2, float width, float pos, bool isRoundMode);

// =============== IMPLEMENTATION ===============

inline void initLEDs() {
#ifdef USE_NEOPIXEL
    FastLED.addLeds<NEOPIXEL, LED_PIN>(leds, NUM_LEDS);
#else
    FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, NUM_LEDS);
#endif
    FastLED.setBrightness(LED_BRIGHTNESS);
    FastLED.clear();
    FastLED.show();
    Serial.println("✓ FastLED initialized");
}

/**
 * Get virtual position for a LED pixel
 * Returns 0.0 at Color1 zone, 1.0 at Color2 zone
 *
 * Bar mode:  [0]=0.0 -----> [45]=1.0  (linear)
 * Round mode: [0]=0.0 -> [22]=1.0, [45]=0.0 -> [23]=1.0 (symmetric)
 */
inline float getVirtualPosition(int ledIndex, bool isRoundMode) {
    if (!isRoundMode) {
        // Bar mode: simple linear mapping
        return (float)ledIndex / (NUM_LEDS - 1);
    } else {
        // Round mode: symmetric from both ends toward middle
        // [0] and [45] are at position 0.0 (Color1)
        // [22] and [23] are at position 1.0 (Color2)

        int halfPoint = NUM_LEDS / 2;  // 23

        if (ledIndex <= halfPoint - 1) {
            // Left half: [0] to [22] -> 0.0 to 1.0
            return (float)ledIndex / (halfPoint - 1);  // 0/22 to 22/22
        } else {
            // Right half: [23] to [45] -> 1.0 to 0.0
            return (float)(NUM_LEDS - 1 - ledIndex) / (halfPoint - 1);  // 22/22 to 0/22
        }
    }
}

/**
 * Calculate pixel color based on virtual position and blend parameters
 *
 * @param virtualPos: 0.0 = Color1 zone, 1.0 = Color2 zone
 * @param color1: First color
 * @param color2: Second color
 * @param width: Blend width (0.0 = sharp edge, 1.0 = full gradient)
 * @param pos: Blend center position (0.0 = all Color2, 0.5 = centered, 1.0 = all Color1)
 */
inline CRGB calculatePixelColor(float virtualPos, CRGB color1, CRGB color2, float width, float pos) {
    float blendFactor;

#ifdef GRADIENT_MODE_SHIFT
    // SHIFT MODE: pos shifts the gradient midpoint
    // width controls how much of the gradient is visible (1.0 = full gradient always)
    // pos shifts where the "50% point" falls

    if (width < 0.001f) {
        // Sharp edge (no blending)
        if (pos <= 0.001f) {
            blendFactor = 1.0f;  // All Color2
        } else if (pos >= 0.999f) {
            blendFactor = 0.0f;  // All Color1
        } else {
            blendFactor = (virtualPos < pos) ? 0.0f : 1.0f;
        }
    } else {
        // Shift the virtual position based on pos
        // When pos=0.5, no shift (centered)
        // When pos=0, shift so midpoint is at start (virtualPos 0 -> blendFactor 0.5)
        // When pos=1, shift so midpoint is at end (virtualPos 1 -> blendFactor 0.5)

        // Calculate shifted position: remap virtualPos through the gradient
        // The gradient "window" is determined by width, shifted by pos
        float shift = (pos - 0.5f) * 2.0f;  // -1.0 to 1.0
        float shiftedPos = virtualPos - shift * (1.0f - width * 0.5f);

        // Map through gradient based on width
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
    // CLAMP MODE (original): pos controls blend zone center, width controls zone size
    float halfWidth = width / 2.0f;
    float blendStart = pos - halfWidth;
    float blendEnd = pos + halfWidth;

    blendStart = constrain(blendStart, 0.0f, 1.0f);
    blendEnd = constrain(blendEnd, 0.0f, 1.0f);

    if (width < 0.001f) {
        // Sharp edge (no blending)
        if (pos <= 0.001f) {
            blendFactor = 1.0f;  // All Color2
        } else if (pos >= 0.999f) {
            blendFactor = 0.0f;  // All Color1
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

    // Interpolate between colors
    return CRGB(
        color1.r + (color2.r - color1.r) * blendFactor,
        color1.g + (color2.g - color1.g) * blendFactor,
        color1.b + (color2.b - color1.b) * blendFactor
    );
}

/**
 * Apply rotation offset to LED index (for Round mode Option A)
 */
inline int applyRotation(int ledIndex, float rotation) {
    int offset = (int)(rotation * NUM_LEDS);
    return (ledIndex + offset) % NUM_LEDS;
}

/**
 * Main LED update function
 *
 * @param color1: First color (RGB)
 * @param color2: Second color (RGB)
 * @param width: Blend width (0.0 - 1.0)
 * @param pos: Position/rotation value from CH8 (0.0 - 1.0)
 * @param isRoundMode: true = round/circle mode, false = bar mode
 */
inline void updateLEDs(CRGB color1, CRGB color2, float width, float pos, bool isRoundMode) {

#ifdef ROUND_MODE_ROTATION
    // Option A: In round mode, CH8 controls rotation, position fixed at 0.5
    float actualPos = isRoundMode ? 0.5f : pos;
    float rotation = isRoundMode ? pos : 0.0f;
#else
    // Option B: CH8 controls position in both modes (no rotation)
    float actualPos = pos;
    float rotation = 0.0f;
#endif

    for (int i = 0; i < NUM_LEDS; i++) {
        int effectiveIndex = i;

        // Apply rotation offset if in round mode with rotation enabled
        if (isRoundMode && rotation > 0.001f) {
            effectiveIndex = applyRotation(i, rotation);
        }

        float virtualPos = getVirtualPosition(effectiveIndex, isRoundMode);
        leds[i] = calculatePixelColor(virtualPos, color1, color2, width, actualPos);
    }

    FastLED.show();
}

#endif // LED_CONTROL_H
