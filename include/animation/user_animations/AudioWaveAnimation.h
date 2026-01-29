#ifndef AUDIOWAVEANIMATION_H
#define AUDIOWAVEANIMATION_H

#include "animation/AudioReactAnimation.h"

class AudioWaveAnimation : public AudioReactAnimation {
public:
    AudioWaveAnimation(const std::string& name)
        : AudioReactAnimation(name),
          lowFreqEnergy(0.0f),
          currentLedsLit(0.0f),
          waveOffset(0.0f) {
        
        // Default Palette (Red/Blue)
        palette = {{ CRGB::Red, CRGB::Blue }};

        // Audio smoothing factors
        attackFactor = (1000000.0f / attackTime) / SAMPLING_FREQ;
        releaseFactor = (1000000.0f / releaseTime) / SAMPLING_FREQ;

        registerParameter("Palette", &this->palette, "Wave colors");
    }

    std::string getTypeName() const override { return "AudioWave"; }

protected:
    void renderAudioAnimation(uint32_t epoch, CRGB* leds, int numLeds) const override {
        // cast away constness to update internal state specific to this animation
        const_cast<AudioWaveAnimation*>(this)->calculateState(numLeds);
        const_cast<AudioWaveAnimation*>(this)->drawWave(leds, numLeds);
    }

private:
    void calculateState(int numLeds) {
        lowFreqEnergy = getEnergy(0, frequencyCutoff);

        // Map energy to a "target" number of LEDs or intensity
        int target = constrain(map((long)lowFreqEnergy, 50000, 250000, 0, numLeds), 0, numLeds);

        if (target > currentLedsLit) {
            currentLedsLit += attackFactor * (target - currentLedsLit);
        } else {
            currentLedsLit -= releaseFactor * (currentLedsLit - target);
        }
    }

    void drawWave(CRGB* leds, int numLeds) {
        // Audio controls speed via waveOffset
        waveOffset -= lowFreqEnergy / 160000.0f - 0.5f;

        CRGBPalette16 p = palette.toPalette16();

        for (int i = 0; i < numLeds; i++) {
            float phase = waveOffset + i * waveSpacing;

            float s = sin(phase);
            uint8_t brightness = (s > 0.0f) ? (uint8_t)(s * 255.0f) : 0;

            // Determine which wave this LED belongs to
            int waveIndex = floor(phase / (2 * PI));

            // Sample from palette based on wave index parity or continuous? 
            // Original logic: Alternate colours per wave (Red/Blue)
            // New logic: Use palette. If palette has 2 colors, it mimics original. 
            // We can map waveIndex to palette index.
            
            // Simple mapping: alternate between palette colors
            uint8_t paletteIndex = (waveIndex % 2) * 128; // 0 or 128
            // Or better, let's map it to the palette size
            // But to keep distinct waves, let's just use the palette.
            
            // Let's use the wave index to step through the palette
            // If we have a gradient, we might want to sample smoothly? 
            // The original was binary (Blue or Red). 
            // Let's sample based on (waveIndex % 16) * 16 to get some variation or just two colors.
            // Safe bet:
            CRGB color = ColorFromPalette(p, (waveIndex % 16) * 16, brightness);
            
            leds[i] = color;
        }
    }

    void setPalette(const DynamicPalette& newPalette) { palette = newPalette; }

private:
    // config
    const float frequencyCutoff = 200.0f;
    const int attackTime = 20;
    const int releaseTime = 250;
    
    // State
    float lowFreqEnergy;
    float currentLedsLit;
    float waveOffset;
    DynamicPalette palette; // User palette
    
    // Derived constants
    float attackFactor;
    float releaseFactor;
    const float waveSpacing = 20.0f * (PI / 180.0f); // radians per LED
};

#endif
