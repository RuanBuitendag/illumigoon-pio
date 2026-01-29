#ifndef AURORAANIMATION_H
#define AURORAANIMATION_H

#include "animation/Animation.h"
#include <FastLED.h>

class AuroraAnimation : public Animation {
public:
    AuroraAnimation(const std::string& name, const DynamicPalette& palette, int seed = 0, float speed = 1.0f, bool reverse = false)
        : Animation(name), palette(palette), seed(seed), speed(speed), reverse(reverse) {
        if (seed == 0) {
            this->seed = random(65535);
        }
        
        // Ensure not black
        if (this->palette.colors.empty()) {
            this->palette.colors.push_back(CRGB::DarkBlue);
            this->palette.colors.push_back(CRGB::Teal);
            this->palette.colors.push_back(CRGB::Green);
            this->palette.colors.push_back(CRGB::Purple); // Add a 4th for variety
        }

        // Seed parameter is intentionally hidden (internal usage only)
        registerParameter("Palette", &this->palette, "Aurora colors");
        registerParameter("Speed", &this->speed, 0.1f, 5.0f, 0.1f, "Animation speed");
        registerParameter("Direction", &this->reverse, "Reverse direction");
    }

    std::string getTypeName() const override { return "Aurora"; }

    void render(uint32_t epoch, CRGB* leds, int numLeds) const override {
        // Very slow time progression for calming effect
        // Apply speed scaling
        float time = epoch * 0.01f * speed;
        
        // Reverse direction if enabled
        if (reverse) {
            time = -time;
        }

        CRGBPalette16 p = palette.toPalette16();

        for (int i = 0; i < numLeds; i++) {
            // Create multiple overlapping waves at different scales
            float pos = i / (float)numLeds;
            
            // Large slow wave (primary aurora movement)
            float wave1 = sin((pos * 2.0f + time * 0.3f + seed * 0.001f) * PI);
            
            // Medium wave (secondary shimmer)
            float wave2 = sin((pos * 4.0f + time * 0.5f + seed * 0.002f) * PI);
            
            // Small fast wave (detail/sparkle)
            float wave3 = sin((pos * 8.0f + time * 0.8f + seed * 0.003f) * PI);
            
            // Combine waves with different weights
            float combined = (wave1 * 0.6f + wave2 * 0.3f + wave3 * 0.1f);
            
            // Map to 0-1 range with soft falloff
            float intensity = (combined + 1.0f) * 0.5f;
            intensity = intensity * intensity; // Square for softer gradient
            
            // Create color shift along the strip (aurora color transition)
            // Use the wave position and time to sample from the palette
            
            // Original logic used hue shift logic:
            // float hueShift = pos * 60.0f + time * 5.0f; 
            // baseHue = 140.0f + sin(...) ...
            
            // New logic: Map these dynamic factors to a palette index (0-255)
            // We want the color to drift slowly (time) and vary along the strip (pos)
            // and react to the waves.
            
            float colorIndex = (pos * 50.0f) + (time * 2.0f); // Base drift
            colorIndex += sin(time * 0.2f + pos * 2.0f) * 30.0f; // Undulation
            
            // Add subtle variation from the fast wave
            if (wave3 > 0.7f) {
                colorIndex += wave3 * 20.0f;
            }
            
            // Sample from palette
            CRGB color = ColorFromPalette(p, (uint8_t)colorIndex);
            
            // Apply intensity/brightness
            // Original used HSV value 220 - (intensity * 40), then multiplied by 0.8f
            // We can just scale the RGB color
            uint8_t brightness = (uint8_t)(intensity * 255 * 0.8f);
            leds[i] = color.nscale8_video(brightness);
            
            // Add occasional bright "peaks" in the aurora
            float peak = sin((pos * 3.0f + time * 0.4f) * PI);
            if (peak > 0.85f) {
                float peakBrightness = (peak - 0.85f) * 6.67f; // 0 to 1 range
                
                // For peaks, we can brighten the existing color or add white/gold
                // Let's bias towards the palette but brighter, or just add white for sparkle
                CRGB peakColor = ColorFromPalette(p, (uint8_t)(colorIndex + 128)); // Complementary-ish/Shifted
                // Or just white for classic shimmer
                peakColor = CRGB::White;

                leds[i] += peakColor.nscale8_video((uint8_t)(peakBrightness * 80));
            }
        }
    }

private:
    mutable DynamicPalette palette;
    int seed;
    float speed;
    bool reverse;
};

#endif