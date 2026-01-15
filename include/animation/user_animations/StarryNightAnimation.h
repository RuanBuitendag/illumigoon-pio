#ifndef STARRYNIGHTANIMATION_H
#define STARRYNIGHTANIMATION_H

#include "animation/Animation.h"
#include <FastLED.h>

class StarryNightAnimation : public Animation {
public:
    StarryNightAnimation(uint8_t numStars = 15, uint16_t seed = 0)
        : numStars(numStars), seed(seed), stars(nullptr), initialized(false), speed(1.0f) {
        if (seed == 0) {
            this->seed = random(65535);
        }
        
        stars = new Star[numStars];
        registerParameter("Speed", &this->speed);
    }

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
            }
            initialized = true;
        }

        float skyWave = sin(epoch * 0.005f) * 0.1f + 0.9f;

        for (int i = 0; i < numLeds; i++) {
            float gradient = i / (float)numLeds;
            leds[i] = CRGB(
                0,
                (uint8_t)(5 * gradient),
                (uint8_t)((20 + 10 * gradient) * skyWave)
            );
        }

        for (int i = 0; i < numStars; i++) {
            stars[i].phase += stars[i].speed * speed;
            if (stars[i].phase > 6.28f) {
                stars[i].phase -= 6.28f;
            }

            float twinkle = (sin(stars[i].phase) + 1.0f) * 0.5f;
            twinkle = twinkle * twinkle;
            
            uint8_t starBrightness = (uint8_t)(twinkle * stars[i].brightness);

            CRGB starColor = CRGB(
                starBrightness,
                starBrightness * 0.95f,
                starBrightness * 0.85f
            );

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
    };

    mutable Star* stars;
    uint8_t numStars;
    uint16_t seed;
    mutable bool initialized;
    float speed;
};

#endif