#ifndef BREATHINGANIMATION_H
#define BREATHINGANIMATION_H

#include "animation/Animation.h"
#include <FastLED.h>

class BreathingAnimation : public Animation {
public:
    BreathingAnimation(const std::string& name, CRGB colour, 
                       int attack, int hold, int release, int rest, uint8_t minBrightness = 0)
        : Animation(name), colour(colour), 
          attack(attack), hold(hold), release(release), rest(rest), minBrightness(minBrightness)
    {
        registerParameter("Colour", &this->colour, "Main color");
        registerParameter("Attack", &this->attack, 0, 5000, 1, "Fade-in duration (ms)");
        registerParameter("Hold", &this->hold, 0, 5000, 1, "Max brightness duration (ms)");
        registerParameter("Release", &this->release, 0, 5000, 1, "Fade-out duration (ms)");
        registerParameter("Rest", &this->rest, 0, 5000, 1, "Min brightness duration (ms)");
        registerParameter("Min Brightness", &this->minBrightness, 0, 255, 1, "Base brightness level");
    }

    std::string getTypeName() const override { return "Breathing"; }

    void render(uint32_t epoch, CRGB* leds, int numLeds) const override {

        // epoch is 10ms ticks
        uint32_t timeMs = epoch * 10;
        
        int totalCycle = attack + hold + release + rest;
        if (totalCycle == 0) totalCycle = 1;

        int cyclePos = timeMs % totalCycle;
        uint8_t brightness = 0;

        // Helper for easing: 0.0 -> 1.0 (InOutSine)
        auto easeInOut = [](float t) -> float {
            return 0.5f * (1.0f - cos(t * PI));
        };

        if (cyclePos < attack) {
            // Attack: minBrightness -> 255
            if (attack > 0) {
                float t = (float)cyclePos / attack;
                float val = minBrightness + (255.0f - minBrightness) * easeInOut(t);
                brightness = (uint8_t)val;
            } else {
                brightness = 255;
            }
        } 
        else if (cyclePos < (attack + hold)) {
            // Hold: 255
            brightness = 255;
        } 
        else if (cyclePos < (attack + hold + release)) {
            // Release: 255 -> minBrightness
            int p = cyclePos - (attack + hold);
            if (release > 0) {
                float t = (float)p / release;
                // Interpolate 255 -> minBrightness
                float val = 255.0f + (minBrightness - 255.0f) * easeInOut(t);
                brightness = (uint8_t)val;
            } else {
                brightness = minBrightness;
            }
        } 
        else {
            // Rest: minBrightness
            brightness = minBrightness;
        }

        CRGB pixelColor = colour;
        pixelColor.nscale8(brightness);

        for (int i = 0; i < numLeds; i++) {
            leds[i] = pixelColor;
        }
    }

private:
    long map(long x, long in_min, long in_max, long out_min, long out_max) const {
        if (in_max == in_min) return out_min;
        return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
    }

    CRGB colour;
    int attack;         // ms
    int hold;           // ms
    int release;        // ms
    int rest;           // ms
    uint8_t minBrightness; // 0-255
};

#endif
