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
        
        // Audio smoothing factors
        attackFactor = (1000000.0f / attackTime) / SAMPLING_FREQ;
        releaseFactor = (1000000.0f / releaseTime) / SAMPLING_FREQ;
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

        for (int i = 0; i < numLeds; i++) {
            float phase = waveOffset + i * waveSpacing;

            float s = sin(phase);
            uint8_t brightness = (s > 0.0f) ? (uint8_t)(s * 255.0f) : 0;

            // Determine which wave this LED belongs to
            int waveIndex = floor(phase / (2 * PI));

            // Alternate colours per wave
            uint8_t hue = (waveIndex % 2 == 0) ? 20 : 160; // red / blue

            leds[i] = CHSV(hue, 255, waveIndex % 2 == 0 ? brightness : 0);
        }
    }

private:
    // config
    const float frequencyCutoff = 200.0f;
    const int attackTime = 20;
    const int releaseTime = 250;
    
    // State
    float lowFreqEnergy;
    float currentLedsLit;
    float waveOffset;
    
    // Derived constants
    float attackFactor;
    float releaseFactor;
    const float waveSpacing = 20.0f * (PI / 180.0f); // radians per LED
};

#endif
