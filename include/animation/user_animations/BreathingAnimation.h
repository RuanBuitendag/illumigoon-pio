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
        registerParameter("Colour", &this->colour);
        registerParameter("Attack", &this->attack);
        registerParameter("Hold", &this->hold);
        registerParameter("Decay", &this->decay);
        registerParameter("Sustain Lvl", &this->sustainLevel);
        registerParameter("Sustain T", &this->sustainTime);
        registerParameter("Release", &this->release);
        registerParameter("Rest", &this->rest);
    }

    void render(uint32_t epoch, CRGB* leds, int numLeds) const override {
        // epoch is 10ms ticks
        uint32_t timeMs = epoch * 10;
        
        int totalCycle = attack + hold + decay + sustainTime + release + rest;
        if (totalCycle == 0) totalCycle = 1;

        int cyclePos = timeMs % totalCycle;
        uint8_t brightness = 0;

        if (cyclePos < attack) {
            // Attack: 0 -> 255
            if (attack > 0) {
                brightness = (cyclePos * 255) / attack;
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
            // Interpolate from 255 down to sustainLevel
            if (decay > 0) {
                // p goes from 0 to decay
                // want map(p, 0, decay, 255, sustainLevel)
                brightness = map(p, 0, decay, 255, sustainLevel);
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
                // p goes from 0 to release
                // want map(p, 0, release, sustainLevel, 0)
                int val = map(p, 0, release, sustainLevel, 0);
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
