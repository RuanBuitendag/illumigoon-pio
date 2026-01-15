#ifndef LINEANIMATION_H
#define LINEANIMATION_H

#include "animation/Animation.h"
#include <FastLED.h>

class LineAnimation : public Animation {
public:
    LineAnimation(int lineLength, int spacing, CRGB colour)
        : lineLength(lineLength), spacing(spacing), colour(colour)
    {
        registerParameter("Line Length", &this->lineLength);
        registerParameter("Spacing", &this->spacing);
        registerParameter("Colour", &this->colour);
    }

    void render(uint32_t epoch, CRGB* leds, int numLeds) const override {
        for (int i = numLeds - 1; i > 0; i--) {
            leds[i] = leds[i - 1];
        }

        int cycle = lineLength + spacing;
        leds[0] = (epoch % cycle < lineLength) ? colour : CRGB::Black;
    }

private:
    int lineLength;
    int spacing;
    CRGB colour;
};

#endif