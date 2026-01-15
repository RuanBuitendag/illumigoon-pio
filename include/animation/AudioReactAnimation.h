#ifndef AUDIOREACTANIMATION_H
#define AUDIOREACTANIMATION_H

#include "animation/Animation.h"
#include <FastLED.h>
#include <arduinoFFT.h>

#define MIC_PIN       34
#define SAMPLES       256
#define SAMPLING_FREQ 8000

class AudioReactAnimation : public Animation {
public:
    AudioReactAnimation(const std::string& name)
        : Animation(name),
          FFT(vReal, vImag, SAMPLES, SAMPLING_FREQ, false) {
        
        // Initialize hardware settings
        // Note: These hardware settings are global. 
        analogSetPinAttenuation(MIC_PIN, ADC_11db);
        analogReadResolution(12);
    }

    virtual void render(uint32_t epoch, CRGB* leds, int numLeds) const override {
        // Cast away constness to update internal state
        const_cast<AudioReactAnimation*>(this)->updateAudioData();
        renderAudioAnimation(epoch, leds, numLeds);
    }

protected:
    // Pure virtual method for subclasses to implement their specific rendering logic
    virtual void renderAudioAnimation(uint32_t epoch, CRGB* leds, int numLeds) const = 0;

    void updateAudioData() {
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
    }

    // Helper to get total energy in a frequency range
    float getEnergy(float minFreq, float maxFreq) const {
        float energy = 0.0f;
        for (int i = 1; i < SAMPLES / 2; i++) {
            float freq = (i * SAMPLING_FREQ) / SAMPLES;
            if (freq >= minFreq && freq <= maxFreq) {
                energy += vReal[i];
            }
        }
        return energy;
    }

    // Access to raw FFT data if needed
    float getMagnitude(int bin) const {
        if (bin >= 0 && bin < SAMPLES / 2) {
            return vReal[bin];
        }
        return 0.0f;
    }

    int getNumBins() const {
        return SAMPLES / 2;
    }

    float getBinFrequency(int bin) const {
        return (bin * SAMPLING_FREQ) / SAMPLES;
    }

protected:
    float vReal[SAMPLES];
    float vImag[SAMPLES];
    ArduinoFFT<float> FFT;
};

#endif
