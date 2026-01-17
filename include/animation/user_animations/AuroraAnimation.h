#ifndef AURORAANIMATION_H
#define AURORAANIMATION_H

#include "animation/Animation.h"
#include <FastLED.h>

class AuroraAnimation : public Animation {
public:
    AuroraAnimation(const std::string& name, int seed = 0)
        : Animation(name), seed(seed) {
        if (seed == 0) {
            this->seed = random(65535);
        }
        registerParameter("Seed", &this->seed, 0, 65535, 1, "Pattern random seed");
    }

    void render(uint32_t epoch, CRGB* leds, int numLeds) const override {
        // Very slow time progression for calming effect
        float time = epoch * 0.01f;

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
            float hueShift = pos * 60.0f + time * 5.0f; // Slow color drift
            
            // Aurora color palette: blue-green-cyan range (120-180 on HSV)
            // With occasional violet hints (200-220)
            float baseHue = 140.0f + sin(time * 0.2f + pos * 2.0f) * 30.0f;
            
            // Add subtle purple/violet accents
            if (wave3 > 0.7f) {
                baseHue = 200.0f + wave3 * 20.0f;
            }
            
            // Create the aurora color with high saturation
            CHSV hsvColor(
                (uint8_t)(baseHue + hueShift),
                220 - (intensity * 40), // Vary saturation slightly
                (uint8_t)(intensity * 255 * 0.8f) // Keep brightness moderate for calmness
            );
            
            leds[i] = hsvColor;
            
            // Add occasional bright "peaks" in the aurora
            float peak = sin((pos * 3.0f + time * 0.4f) * PI);
            if (peak > 0.85f) {
                float peakBrightness = (peak - 0.85f) * 6.67f; // 0 to 1 range
                leds[i] += CRGB(
                    peakBrightness * 50,
                    peakBrightness * 80,
                    peakBrightness * 60
                );
            }
        }
    }

private:
    int seed;
};

#endif