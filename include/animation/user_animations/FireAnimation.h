#ifndef FIRE_ANIMATION_H
#define FIRE_ANIMATION_H

#include "animation/Animation.h"
#include <FastLED.h>

class FireAnimation : public Animation {
public:
    FireAnimation(const std::string& name, const DynamicPalette& palette, float speed = 1.0f, uint8_t cooling = 55, uint8_t sparking = 120)
        : Animation(name), palette(palette), speed(speed), cooling(cooling), sparking(sparking) {
            registerParameter("Speed", &this->speed, 0.0f, 5.0f, 0.01f, "Flame flicker speed");
            registerParameter("Cooling", &this->cooling, 0, 255, 1, "Rate of cooling");
            registerParameter("Sparking", &this->sparking, 0, 255, 1, "Spark generation chance");
            registerParameter("Palette", &this->palette, "Color scheme");
        }

    std::string getTypeName() const override { return "Fire"; }

    void render(uint32_t epoch, CRGB* leds, int numLeds) const override {

        // Calculate update interval based on speed (default ~30ms at speed 1.0)
        float safeSpeed = (speed < 0.01f) ? 0.01f : speed;
        uint32_t interval = (uint32_t)(30.0f / safeSpeed);

        if (epoch - lastUpdate >= interval) {
            lastUpdate = epoch;
            
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
        }

        // Generate palette once per frame (or cache it in DynamicPalette if strict optimization needed)
        CRGBPalette16 p = palette.toPalette16();

        for (int j = 0; j < numLeds; j++) {
            uint8_t colourIndex = scale8(heat[j], 240);
            CRGB color = ColorFromPalette(p, colourIndex);
            
            uint8_t flicker = random8(200, 255); 
            leds[j] = color.nscale8_video(flicker);
        }
    }

    void setPalette(const DynamicPalette& newPalette) { palette = newPalette; }
    void setSpeed(float newSpeed) { speed = newSpeed; }
    void setCooling(uint8_t newCooling) { cooling = newCooling; }
    void setSparking(uint8_t newSparking) { sparking = newSparking; }

private:
    mutable DynamicPalette palette;
    float speed;
    uint8_t cooling;
    uint8_t sparking;
    mutable uint32_t lastUpdate = 0;
    mutable uint8_t heat[90] = {0}; // make strict member instead of static local to avoid sharing across instances
};

#endif
