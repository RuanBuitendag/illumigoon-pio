#ifndef STARRYNIGHTANIMATION_H
#define STARRYNIGHTANIMATION_H

#include "animation/Animation.h"
#include <FastLED.h>

class StarryNightAnimation : public Animation {
public:
    StarryNightAnimation()
        : Animation("StarryNight"), numStars(15), seed(0), stars(nullptr), initialized(false), speed(1.0f) {
        
        this->seed = random(65535);
        stars = new Star[numStars];

        // Default Background: Deep Blue Gradient
        bgPalette.colors = { CRGB(0, 0, 0), CRGB(0, 0, 20), CRGB(0, 5, 30) };

        // Default Stars: White/Blueish
        starPalette.colors = { CRGB::White, CRGB(200, 200, 255) };
        
        registerParameter("Speed", &this->speed, 0.0f, 5.0f, 0.01f, "Twinkle speed");
        registerParameter("Background", &this->bgPalette, "Sky gradient");
        registerParameter("Stars", &this->starPalette, "Star colors");
    }

    std::string getTypeName() const override { return "StarryNight"; }

    ~StarryNightAnimation() {

        delete[] stars;
    }

    void render(uint32_t epoch, CRGB* leds, int numLeds) const override {
        if (!initialized) {
            for (int i = 0; i < numStars; i++) {
                stars[i].position = random(numLeds);
                stars[i].phase = random(628) / 100.0f;
                stars[i].speed = 0.02f + (random(30) / 1000.0f);
                stars[i].brightness = 128 + random(127);
                // Assign a random color index from palette? Or sample?
                // Let's store a normalized 0-1 val for color lookup
                stars[i].colorIndex = (float)random(100) / 100.0f; 
            }
            initialized = true;
        }

        float skyWave = sin(epoch * 0.005f) * 0.1f + 0.9f;

        // Render Background Gradient
        for (int i = 0; i < numLeds; i++) {
            float gradientPos = i / (float)(numLeds - 1);
            if (bgPalette.colors.empty()) {
                leds[i] = CRGB::Black;
            } else if (bgPalette.colors.size() == 1) {
                leds[i] = bgPalette.colors[0];
            } else {
                float scaled = gradientPos * (bgPalette.colors.size() - 1);
                int idx = (int)scaled;
                float frac = scaled - idx;
                
                CRGB c1 = bgPalette.colors[idx];
                CRGB c2 = (idx + 1 < bgPalette.colors.size()) ? bgPalette.colors[idx+1] : c1;
                
                CRGB bg = blend(c1, c2, (uint8_t)(frac * 255));
                leds[i] = bg; // Apply brightness modulation?
                // Scale brightness by skyWave to keep the original pulsing effect
                leds[i].nscale8((uint8_t)(skyWave * 255)); 
            }
        }

        // Render Stars
        for (int i = 0; i < numStars; i++) {
            stars[i].phase += stars[i].speed * speed;
            if (stars[i].phase > 6.28f) {
                stars[i].phase -= 6.28f;
            }

            float twinkle = (sin(stars[i].phase) + 1.0f) * 0.5f;
            twinkle = twinkle * twinkle;
            
            uint8_t starBrightness = (uint8_t)(twinkle * stars[i].brightness);

            // Fetch Star Color
            CRGB starColor = CRGB::White;
            if (!starPalette.colors.empty()) {
                if (starPalette.colors.size() == 1) {
                    starColor = starPalette.colors[0];
                } else {
                    float scaled = stars[i].colorIndex * (starPalette.colors.size() - 1);
                     int idx = (int)scaled;
                    float frac = scaled - idx;
                    CRGB c1 = starPalette.colors[idx];
                    CRGB c2 = (idx + 1 < starPalette.colors.size()) ? starPalette.colors[idx+1] : c1;
                    starColor = blend(c1, c2, (uint8_t)(frac * 255));
                }
            }
            
            starColor.nscale8(starBrightness);

            int pos = stars[i].position;
            leds[pos] += starColor;

            if (pos > 0) {
                leds[pos - 1] += starColor * 0.3f;
            }
            if (pos < numLeds - 1) {
                leds[pos + 1] += starColor * 0.3f;
            }

            if (epoch % 500 == (i * 37) % 500 && twinkle > 0.8f) {
                for (int t = 1; t <= 3 && pos + t < numLeds; t++) {
                    leds[pos + t] += CRGB(
                        starBrightness / (t * 2),
                        starBrightness / (t * 2),
                        starBrightness / (t * 2)
                    );
                }
            }
        }
    }

private:
    struct Star {
        uint8_t position;
        float phase;
        float speed;
        uint8_t brightness;
        float colorIndex;
    };

    mutable Star* stars;
    uint8_t numStars;
    uint16_t seed;
    mutable bool initialized;
    float speed;
    DynamicPalette bgPalette;
    DynamicPalette starPalette;
};

#endif