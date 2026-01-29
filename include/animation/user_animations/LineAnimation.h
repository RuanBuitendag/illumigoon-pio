#ifndef LINEANIMATION_H
#define LINEANIMATION_H

#include "animation/Animation.h"
#include <FastLED.h>

class LineAnimation : public Animation {
public:
    LineAnimation()
        : Animation("Line"), lineLength(60), spacing(30), speed(5.0f)
    {
        // Default gradient
        gradientPalette.colors = { CRGB(255, 30, 0), CRGB(255, 30, 0) };

        registerParameter("Line Length", &this->lineLength, 0, 90, 1, "Length of segments");
        registerParameter("Spacing", &this->spacing, 0, 90, 1, "Distance between segments");
        registerParameter("Gradient", &this->gradientPalette, "Color gradient");
        registerParameter("Speed", &this->speed, 0.0f, 10.0f, 1.0f, "Animation speed multiplier");
    }

    std::string getTypeName() const override { return "Line"; }

    void render(uint32_t epoch, CRGB* leds, int numLeds) const override {

        // Use a clearer cycle length
        int cycle = lineLength + spacing;
        if (cycle == 0) cycle = 1; // Prevent divide by zero

        int offset = (epoch * speed) / 10; 
        
        // Apply phase offset
        // devicePhase is 0.0-1.0, map to 0-cycle
        int phaseOffset = (int)(cycle * devicePhase);
        
        // Invert phase addition if needed, or just add. 
        // Adding phase effectively shifts the pattern "backwards" relative to movement if strictly added to 'pos' calculation in a certain way?
        // Let's standard: pos determines "where in the cycle is this pixel". 
        // If we want "Phase 0.5" to mean "Halfway through cycle ahead of Phase 0.0"
        
        for (int i = 0; i < numLeds; i++) {
            // (i - offset) shifts the pattern along the strip.
            // Adding phaseOffset shifts the pattern locally.
            int pos = (i - offset + phaseOffset) % cycle;
            if (pos < 0) pos += cycle;

            if (pos < lineLength) {
                // Map pixel index to gradient (0.0 to 1.0 along the strip)
                float gradientPos = (float)i / (float)(numLeds - 1);
                
                // Get color from palette
                if (gradientPalette.colors.empty()) {
                    leds[i] = CRGB::White;
                } else if (gradientPalette.colors.size() == 1) {
                    leds[i] = gradientPalette.colors[0];
                } else {
                    // Linear interpolation
                    float scaled = gradientPos * (gradientPalette.colors.size() - 1);
                    int idx = (int)scaled;
                    float frac = scaled - idx;
                    
                    if (idx >= gradientPalette.colors.size() - 1) {
                        leds[i] = gradientPalette.colors.back();
                    } else {
                        CRGB c1 = gradientPalette.colors[idx];
                        CRGB c2 = gradientPalette.colors[idx + 1];
                        leds[i] = blend(c1, c2, (uint8_t)(frac * 255));
                    }
                }
            } else {
                leds[i] = CRGB::Black;
            }
        }
    }

private:
    int lineLength;
    int spacing;
    DynamicPalette gradientPalette;
    int speed;
};

#endif