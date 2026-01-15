#ifndef AUDIOWAVEANIMATION_H
#define AUDIOWAVEANIMATION_H

#include "animation/Animation.h"
#include <FastLED.h>
#include <arduinoFFT.h>

#define MIC_PIN       34
#define SAMPLES       256
#define SAMPLING_FREQ 8000

class AudioWaveAnimation : public Animation {
public:
    AudioWaveAnimation(const std::string& name)
        : Animation(name),
          FFT(vReal, vImag, SAMPLES, SAMPLING_FREQ, false),
          lowFreqEnergy(0.0f),
          currentLedsLit(0.0f),
          waveOffset(0.0f) {
        
        // Audio smoothing factors
        attackFactor = (1000000.0f / attackTime) / SAMPLING_FREQ;
        releaseFactor = (1000000.0f / releaseTime) / SAMPLING_FREQ;

        // Note: These hardware settings are global. 
        // In a more complex system, we might want to manage this centrally,
        // but for now we set it here as per user code requirements.
        analogSetPinAttenuation(MIC_PIN, ADC_11db);
        analogReadResolution(12);
    }

    void render(uint32_t epoch, CRGB* leds, int numLeds) const override {
        // We need to cast away constness to update internal state during render loop
        // Alternatively, update() could be separated from render(), but Animation interface 
        // currently only has render taking epoch.
        // Ideally, heavy lifting like FFT should be in an update() method, not render(),
        // but following the existing pattern:
        const_cast<AudioWaveAnimation*>(this)->updateAudioData(numLeds);
        const_cast<AudioWaveAnimation*>(this)->drawWave(leds, numLeds);
    }

private:
    void updateAudioData(int numLeds) {
        unsigned long samplingPeriod = 1000000 / SAMPLING_FREQ;

        for (int i = 0; i < SAMPLES; i++) {
            unsigned long t = micros();
            vReal[i] = analogRead(MIC_PIN) - 2048;
            vImag[i] = 0;
            while (micros() - t < samplingPeriod);
        }

        FFT.windowing(vReal, SAMPLES, FFT_WIN_TYP_HAMMING, FFT_FORWARD);
        FFT.compute(vReal, vImag, SAMPLES, FFT_FORWARD);
        FFT.complexToMagnitude(vReal, vImag, SAMPLES);

        lowFreqEnergy = 0.0;
        for (int i = 1; i < SAMPLES / 2; i++) {
            float freq = (i * SAMPLING_FREQ) / SAMPLES;
            if (freq <= frequencyCutoff) {
                lowFreqEnergy += vReal[i];
            }
        }

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

            // Only light up if within the "currentLedsLit" range? 
            // The original code had logic for `currentLedsLit` but didn't explicitly use it to limit the wave 
            // in the `presetWave` function provided.
            // Wait, looking at the user request:
            // "int target = constrain(map(lowFreqEnergy...)..." 
            // "if (target > currentLedsLit)..."
            // But `presetWave` DOES NOT use `currentLedsLit` anywhere in the provided snippet!
            // It calculates it, but doesn't use it for drawing.
            // I will strictly follow `presetWave` logic which renders the wave everywhere.
            // The `currentLedsLit` might have been for a VU meter style which isn't fully implemented in the snippet provided for drawing.
            // However, to keep it faithful, I will keep the calculation in updateAudioData just in case,
            // but `drawWave` will follow `presetWave`.

            leds[i] = CHSV(hue, 255, waveIndex % 2 == 0 ? brightness : 0);
        }
    }

private:
    // FFT Members
    float vReal[SAMPLES];
    float vImag[SAMPLES];
    ArduinoFFT<float> FFT;

    // config
    const float frequencyCutoff = 200.0f;
    const int attackTime = 200;
    const int releaseTime = 500;
    
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
