#ifndef FIRE_ANIMATION_H
#define FIRE_ANIMATION_H

#include "animation/Animation.h"
#include <FastLED.h>

class FireAnimation : public Animation {
public:
    FireAnimation(CRGBPalette16 palette = HeatColors_p, float speed = 1.0f, uint8_t cooling = 55, uint8_t sparking = 120)
        : palette(palette), speed(speed), cooling(cooling), sparking(sparking) {
            registerParameter("Speed", &this->speed);
            registerParameter("Cooling", &this->cooling);
            registerParameter("Sparking", &this->sparking);
            registerParameter("Palette", &this->palette);
        }

    void render(uint32_t epoch, CRGB* leds, int numLeds) const override {
        static uint8_t heat[90] = {0};

        // Step 1: Cool down every cell a little
        for (int i = 0; i < numLeds; i++) {
            int cooldown = random8(0, ((cooling * 10) / numLeds) + 2);
            heat[i] = qsub8(heat[i], cooldown);
        }

        // Step 2: Heat diffusion upward
        for (int k = numLeds - 1; k >= 2; k--) {
            heat[k] = (heat[k - 1] + heat[k - 2] + heat[k - 2]) / 3;
        }

        // Step 3: Random sparks at the bottom
        if (random8() < sparking) {
            int sparkHeight = random8(0, numLeds / 4); // variable height for sparks
            heat[sparkHeight] = qadd8(heat[sparkHeight], random8(160, 255));
        }

        // Step 4: Optional tiny timber sparks
        if (random8() < 10) {
            int sparkPos = random8(0, numLeds / 2);
            leds[sparkPos] = CRGB::White;
        }

        // Step 5: Map heat to colours and add flicker
        for (int j = 0; j < numLeds; j++) {
            uint8_t colourIndex = scale8(heat[j], 240);
            CRGB color = ColorFromPalette(palette, colourIndex);
            uint8_t flicker = random8(200, 255); // subtle flicker
            leds[j] = color.nscale8_video(flicker);
        }
    }

    void setPalette(CRGBPalette16 newPalette) { palette = newPalette; }
    void setSpeed(float newSpeed) { speed = newSpeed; }
    void setCooling(uint8_t newCooling) { cooling = newCooling; }
    void setSparking(uint8_t newSparking) { sparking = newSparking; }

private:
    mutable CRGBPalette16 palette;
    float speed;
    uint8_t cooling;
    uint8_t sparking;
};

#endif
