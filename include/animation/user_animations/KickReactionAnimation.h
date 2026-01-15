#ifndef KICKREACTIONANIMATION_H
#define KICKREACTIONANIMATION_H

#include "animation/AudioReactAnimation.h"

class KickReactionAnimation : public AudioReactAnimation {
public:
    KickReactionAnimation(const std::string& name)
        : AudioReactAnimation(name),
          brightness(0.0f) {
        
        // Attack/Decay config
        // Quick attack, relatively quick release for punchy effect
        attackRate = 0.2f;  // Per frame increase
        decayRate = 0.05f;  // Per frame decrease
    }

protected:
    void renderAudioAnimation(uint32_t epoch, CRGB* leds, int numLeds) const override {
        // Update state
        const_cast<KickReactionAnimation*>(this)->calculateState();
        const_cast<KickReactionAnimation*>(this)->draw(leds, numLeds);
    }

private:
    void calculateState() {
        // Get energy in bass range (0 - 100Hz)
        float bassEnergy = getEnergy(0, 200.0f);
        
        // Threshold for triggering the kick
        // This threshold might need tuning based on mic sensitivity
        float threshold = 50000.0f * 2.0f; // Scaled for sensitivity

        if (bassEnergy > threshold) {
            // Attack phase: boost brightness based on energy intensity
            // Normalizing energy somewhat to 0.0 - 1.0 range addition
            float energyFactor = constrain((bassEnergy - threshold) / 100000.0f, 0.0f, 1.0f);
            brightness += attackRate + energyFactor;
        } else {
            // Decay phase
            brightness -= decayRate;
        }

        // Clamp brightness
        brightness = constrain(brightness, 0.0f, 1.0f);
    }

    void draw(CRGB* leds, int numLeds) {
        // Bright purple/pink: RGB(255, 0, 255) / HSV(192..255?)
        // Let's use a nice CHSV pink/purple
        uint8_t hue = 220; // Pinkish purple
        uint8_t sat = 255;
        uint8_t val = (uint8_t)(brightness * 255.0f);

        for(int i = 0; i < numLeds; i++) {
            leds[i] = CHSV(hue, sat, val);
        }
    }

private:
    float brightness;
    float attackRate;
    float decayRate;
};

#endif
