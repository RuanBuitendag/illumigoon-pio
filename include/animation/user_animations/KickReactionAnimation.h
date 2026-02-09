#ifndef KICKREACTIONANIMATION_H
#define KICKREACTIONANIMATION_H

#include "animation/AudioReactAnimation.h"

class KickReactionAnimation : public AudioReactAnimation {
public:
    KickReactionAnimation()
        : AudioReactAnimation("KickReaction"),
          brightness(0.0f),
          threshold(100000.0f),  // Default matched to previous (50000 * 2)
          decayRate(0.05f),
          minFreq(0.0f),
          maxFreq(200.0f) {
        
        // Attack/Decay config
        attackRate = 0.3f; // Initializing to a reasonable default since it wasn't before
        
        // Default Palette (Purple/Pink)
        palette = {{ CRGB(255, 0, 255), CRGB(128, 0, 128) }};

        registerParameter("Palette", &this->palette, "Kick colors");
        registerParameter("Threshold", &this->threshold, 0.0f, 200000.0f, 1000.0f, "Sensitivity");
        registerParameter("Decay", &this->decayRate, 0.001f, 0.5f, 0.001f, "Fade Speed");
        registerParameter("Min Freq", &this->minFreq, 0.0f, 4000.0f, 10.0f, "Start Hz");
        registerParameter("Max Freq", &this->maxFreq, 0.0f, 4000.0f, 10.0f, "End Hz");
    }

    std::string getTypeName() const override { return "KickReaction"; }

protected:
    void renderAudioAnimation(uint32_t epoch, CRGB* leds, int numLeds) const override {
        // Update state
        const_cast<KickReactionAnimation*>(this)->calculateState();
        const_cast<KickReactionAnimation*>(this)->draw(leds, numLeds);
    }

private:
    void calculateState() {
        // Get energy in configured frequency range
        float rangeEnergy = getEnergy(minFreq, maxFreq);
        
        if (rangeEnergy > threshold) {
            // Attack phase: boost brightness based on energy intensity
            // Normalizing energy somewhat to 0.0 - 1.0 range addition
            float energyFactor = constrain((rangeEnergy - threshold) / 100000.0f, 0.0f, 1.0f);
            brightness += attackRate + energyFactor;
        } else {
            // Decay phase
            brightness -= decayRate;
        }

        // Clamp brightness
        brightness = constrain(brightness, 0.0f, 1.0f);
    }

    void draw(CRGB* leds, int numLeds) {
        CRGBPalette16 p = palette.toPalette16();
        
        // Use index 0 color from palette
        CRGB color = ColorFromPalette(p, 0); 
        // Apply brightness
        color.nscale8_video((uint8_t)(brightness * 255.0f));

        for(int i = 0; i < numLeds; i++) {
            leds[i] = color;
        }
    }

private:
    float brightness;
    float attackRate;
    float decayRate;
    float threshold;
    float minFreq;
    float maxFreq;
    DynamicPalette palette;
};

#endif
