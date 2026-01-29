#ifndef LINEANIMATION_H
#define LINEANIMATION_H

#include "animation/Animation.h"
#include <FastLED.h>

class LineAnimation : public Animation {
public:
    LineAnimation(const std::string& name, int lineLength, int spacing, CRGB colour, float speed)
        : Animation(name), lineLength(lineLength), spacing(spacing), speed(speed)
    {
        // Initialize default gradient from single color if needed, or just set a default
        if (gradientPalette.colors.empty()) {
             gradientPalette.colors.push_back(colour);
             gradientPalette.colors.push_back(colour); // Consistent solid color by default
        }

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

        for (int i = 0; i < numLeds; i++) {
            int pos = (i - offset) % cycle;
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