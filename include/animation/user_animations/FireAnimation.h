#ifndef FIRE_ANIMATION_H
#define FIRE_ANIMATION_H

#include "animation/Animation.h"
#include <FastLED.h>

class FireAnimation : public Animation {
public:
    FireAnimation(const std::string& name, const DynamicPalette& palette, float speed = 1.0f, uint8_t height = 150, uint8_t sparking = 120)
        : Animation(name), palette(palette), speed(speed), height(height), cooling(55), sparking(sparking), sparkFreq(30) {
            
            // Initialize spark palette to white-ish by default
            sparkPalette = {{ CRGB::White, CRGB::Gold }};

            registerParameter("Speed", &this->speed, 0.0f, 10.0f, 0.01f, "Flame flicker speed");
            registerParameter("Height", &this->height, 0, 255, 1, "Flame height");
            registerParameter("Sparking", &this->sparking, 0, 255, 1, "Ignition intensity");
            registerParameter("Spark Speed", &this->sparkFreq, 0, 255, 1, "Spark frequency");
            registerParameter("Palette", &this->palette, "Fire colors");
            registerParameter("Spark Palette", &this->sparkPalette, "Spark colors");
        }

    std::string getTypeName() const override { return "Fire"; }

    void render(uint32_t epoch, CRGB* leds, int numLeds) const override {

        // Calculate update interval based on speed (default ~30ms at speed 1.0)
        // Max speed 10.0 -> 3ms interval
        float safeSpeed = (speed < 0.01f) ? 0.01f : speed;
        uint32_t interval = (uint32_t)(30.0f / safeSpeed);

        // Map height to cooling (Inverse relationship: Taller fire = Less cooling)
        // height 0 -> cooling 100 (short)
        // height 255 -> cooling 20 (tall)
        cooling = map(height, 0, 255, 100, 20);

        bool updated = false;

        if (epoch - lastUpdate >= interval) {
            lastUpdate = epoch;
            updated = true;
            
            // Step 1: Cool down every cell a little
            for (int i = 0; i < numLeds; i++) {
                int cooldown = random8(0, ((cooling * 10) / numLeds) + 2);
                heat[i] = qsub8(heat[i], cooldown);
            }

            // Step 2: Heat diffusion upward
            for (int k = numLeds - 1; k >= 2; k--) {
                heat[k] = (heat[k - 1] + heat[k - 2] + heat[k - 2]) / 3;
            }

            // Step 3: Random sparks at the bottom (Ignition)
            if (random8() < sparking) {
                int ignitionHeight = random8(0, numLeds / 4); 
                heat[ignitionHeight] = qadd8(heat[ignitionHeight], random8(160, 255));
            }
        }

        // Generate palette once per frame
        CRGBPalette16 p = palette.toPalette16();

        // Render Fire (Background)
        for (int j = 0; j < numLeds; j++) {
            uint8_t colourIndex = scale8(heat[j], 240);
            CRGB color = ColorFromPalette(p, colourIndex);
            
            uint8_t flicker = random8(200, 255); 
            leds[j] = color.nscale8_video(flicker);
        }

        // Step 4: Visual Sparks / Embers (Overlay)
        // We draw these AFTER the fire loop so they are visible on top.
        // We only generate them on update frames to respect the "frequency" / speed somewhat,
        // otherwise they would be extremely chaotic if drawn every render frame 
        // (independent of simulation speed).
        if (updated) {
            if (random8() < sparkFreq) {
                 int sparkPos = random8(0, numLeds / 2);
                 CRGBPalette16 sp = sparkPalette.toPalette16();
                 leds[sparkPos] = ColorFromPalette(sp, random8(255));
            }
        }
    }
    void setPalette(const DynamicPalette& newPalette) { palette = newPalette; }
    void setSparkPalette(const DynamicPalette& newPalette) { sparkPalette = newPalette; }
    void setSpeed(float newSpeed) { speed = newSpeed; }
    void setHeight(uint8_t newHeight) { height = newHeight; }
    void setSparking(uint8_t newSparking) { sparking = newSparking; }
    void setSparkFreq(uint8_t newFreq) { sparkFreq = newFreq; }

private:
    mutable DynamicPalette palette;
    mutable DynamicPalette sparkPalette;
    float speed;
    uint8_t height;
    mutable uint8_t cooling;
    uint8_t sparking;
    uint8_t sparkFreq;
    mutable uint32_t lastUpdate = 0;
    mutable uint8_t heat[90] = {0}; // make strict member instead of static local to avoid sharing across instances
};

#endif
