#ifndef BREATHINGANIMATION_H
#define BREATHINGANIMATION_H

#include "animation/Animation.h"
#include <FastLED.h>

class BreathingAnimation : public Animation {
public:
    BreathingAnimation(const std::string& name, CRGB colour, 
                       int attack, int hold, int decay, uint8_t sustainLevel, int sustainTime, int release, int rest)
        : Animation(name), colour(colour), 
          attack(attack), hold(hold), decay(decay), sustainLevel(sustainLevel), 
          sustainTime(sustainTime), release(release), rest(rest)
    {
        registerParameter("Colour", &this->colour, "Main color");
        registerParameter("Attack", &this->attack, 0, 5000, 1, "Fade-in duration (ms)");
        registerParameter("Hold", &this->hold, 0, 5000, 1, "Max brightness duration (ms)");
        registerParameter("Decay", &this->decay, 0, 5000, 1, "Fade to sustain duration (ms)");
        registerParameter("Sustain Lvl", &this->sustainLevel, 0, 255, 1, "Brightness level during sustain");
        registerParameter("Sustain T", &this->sustainTime, 0, 5000, 1, "Sustain duration (ms)");
        registerParameter("Release", &this->release, 0, 5000, 1, "Fade-out duration (ms)");
        registerParameter("Rest", &this->rest, 0, 5000, 1, "Off duration (ms)");
    }

    void render(uint32_t epoch, CRGB* leds, int numLeds) const override {
        // epoch is 10ms ticks
        uint32_t timeMs = epoch * 10;
        
        int totalCycle = attack + hold + decay + sustainTime + release + rest;
        if (totalCycle == 0) totalCycle = 1;

        int cyclePos = timeMs % totalCycle;
        uint8_t brightness = 0;

        // Helper for easing: 0.0 -> 1.0 (InOutSine)
        auto easeInOut = [](float t) -> float {
            return 0.5f * (1.0f - cos(t * PI));
        };

        if (cyclePos < attack) {
            // Attack: 0 -> 255
            if (attack > 0) {
                float t = (float)cyclePos / attack;
                brightness = (uint8_t)(easeInOut(t) * 255.0f);
            } else {
                brightness = 255;
            }
        } 
        else if (cyclePos < (attack + hold)) {
            // Hold: 255
            brightness = 255;
        } 
        else if (cyclePos < (attack + hold + decay)) {
            // Decay: 255 -> SustainLevel
            int p = cyclePos - (attack + hold);
            if (decay > 0) {
                float t = (float)p / decay; // 0.0 to 1.0
                // Interpolate 255 -> sustain
                float val = 255.0f + (sustainLevel - 255.0f) * easeInOut(t);
                brightness = (uint8_t)val;
            } else {
                brightness = sustainLevel;
            }
        } 
        else if (cyclePos < (attack + hold + decay + sustainTime)) {
            // Sustain: SustainLevel
            brightness = sustainLevel;
        } 
        else if (cyclePos < (attack + hold + decay + sustainTime + release)) {
            // Release: SustainLevel -> 0
            int p = cyclePos - (attack + hold + decay + sustainTime);
            if (release > 0) {
                float t = (float)p / release;
                // Interpolate sustain -> 0
                float val = sustainLevel + (0.0f - sustainLevel) * easeInOut(t);
                brightness = (uint8_t)val;
            } else {
                brightness = 0;
            }
        } 
        else {
            // Rest: 0
            brightness = 0;
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
    int decay;          // ms
    uint8_t sustainLevel; // 0-255
    int sustainTime;    // ms
    int release;        // ms
    int rest;           // ms
};

#endif
