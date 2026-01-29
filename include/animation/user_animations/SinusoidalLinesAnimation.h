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

    SinusoidalLinesAnimation(const std::string& name,
                             const std::vector<CRGB>& colours,
                             int lineLength,
                             float minFreq,
                             float maxFreq,
                             CRGB bg = CRGB::Black)
        : Animation(name),
          lineLength(lineLength),
          minFrequency(minFreq),
          maxFrequency(maxFreq),
          background(bg),
          speed(1.0f)
    {
        // Initialize palette from constructor colors
        palette.colors = colours;
        
        registerParameter("Line Length", &this->lineLength, 0, 90, 1, "Wave segment length");
        registerParameter("Background", &this->background, "Background color");
        registerParameter("Palette", &this->palette, "Line colors");
        registerParameter("Speed", &this->speed, 0.1f, 5.0f, 0.1f, "Animation speed");

        // Initial population
        syncLines();
    }

    std::string getTypeName() const override { return "SinusoidalLines"; }

    void render(uint32_t epoch, CRGB* leds, int numLeds) const override {

        // Sync lines with palette state (which might have been updated by WebManager)
        syncLines();

        for (int i = 0; i < numLeds; i++) {
            leds[i] = background;
        }

        float t = epoch * 0.001f * speed;

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
    void syncLines() const {
        // Handle size mismatch (Add/Remove)
        if (lines.size() != palette.colors.size()) {
            if (lines.size() < palette.colors.size()) {
                // Add new lines
                size_t needed = palette.colors.size() - lines.size();
                // Since this method is const, use const_cast only if strictly necessary or just trust mutable.
                // lines is mutable, so we can modify it in const function.
                for (size_t i = 0; i < needed; i++) {
                    Line line;
                    // randomFloat is static, so ok.
                    line.frequency = randomFloat(minFrequency, maxFrequency);
                    line.phase = randomFloat(0.0f, 2.0f * M_PI);
                    // Color set below
                    lines.push_back(line);
                }
            } else {
                // Remove excess
                lines.resize(palette.colors.size());
            }
        }

        // Update colors from palette
        for (size_t i = 0; i < lines.size(); i++) {
            lines[i].colour = palette.colors[i];
        }
    }

    mutable std::vector<Line> lines;
    int lineLength;
    float minFrequency, maxFrequency;
    CRGB background;
    float speed;
    DynamicPalette palette;

    static float randomFloat(float minVal, float maxVal) {
        return minVal + (float)rand() / (float)RAND_MAX * (maxVal - minVal);
    }
};

#endif
