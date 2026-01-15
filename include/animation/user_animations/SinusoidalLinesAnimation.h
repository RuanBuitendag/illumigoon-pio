#ifndef SINUSOIDALLINESANIMATION_H
#define SINUSOIDALLINESANIMATION_H

#include "animation/Animation.h"
#include <vector>
#include <cmath>
#include <cstdlib>
#include <FastLED.h>

class SinusoidalLinesAnimation : public Animation {
public:
    struct Line {
        CRGB colour;
        float frequency;
        float phase;
    };

    SinusoidalLinesAnimation(const std::vector<CRGB>& colours,
                             int lineLength,
                             float minFreq,
                             float maxFreq,
                             CRGB bg = CRGB::Black)
        : lineLength(lineLength),
          minFrequency(minFreq),
          maxFrequency(maxFreq),
          background(bg)
    {
        registerParameter("Line Length", &this->lineLength);
        registerParameter("Background", &this->background);
        for (const auto& c : colours) {
            Line line;
            line.colour = c;
            line.frequency = randomFloat(minFrequency, maxFrequency);
            line.phase = randomFloat(0.0f, 2.0f * M_PI);
            lines.push_back(line);
        }
    }

    void render(uint32_t epoch, CRGB* leds, int numLeds) const override {
        for (int i = 0; i < numLeds; i++) {
            leds[i] = background;
        }

        float t = epoch * 0.001f;

        for (int i = 0; i < numLeds; i++) {
            uint16_t r = 0, g = 0, b = 0;
            uint8_t count = 0;

            for (const auto& line : lines) {
                int halfLength = lineLength / 2;
                float sine = sinf(2.0f * M_PI * line.frequency * t + line.phase);

                int center = halfLength +
                             (int)((numLeds - lineLength) * 0.5f * (1.0f + sine));

                if (i >= center - halfLength && i <= center + halfLength) {
                    r += line.colour.r;
                    g += line.colour.g;
                    b += line.colour.b;
                    count++;
                }
            }

            if (count > 0) {
                leds[i] = CRGB(r / count, g / count, b / count);
            }
        }
    }

private:
    std::vector<Line> lines;
    int lineLength;
    float minFrequency, maxFrequency;
    CRGB background;

    static float randomFloat(float minVal, float maxVal) {
        return minVal + (float)rand() / (float)RAND_MAX * (maxVal - minVal);
    }
};

#endif
