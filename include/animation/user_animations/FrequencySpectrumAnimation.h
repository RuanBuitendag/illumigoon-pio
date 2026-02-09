#ifndef FREQUENCYSPECTRUMANIMATION_H
#define FREQUENCYSPECTRUMANIMATION_H

#include "animation/AudioReactAnimation.h"

class FrequencySpectrumAnimation : public AudioReactAnimation {
public:
    FrequencySpectrumAnimation()
        : AudioReactAnimation("FrequencySpectrum"),
          sensitivity(0.0001f), // Lowered default sensitivity
          smoothing(0.5f),
          threshold(1500.0f) { // Noise floor threshold
        
        // Default Palette (Heatmap style)
        palette = {{ CRGB::Red, CRGB::Yellow, CRGB::Green, CRGB::Blue }}; // Low(Red) to High(Blue)

        // Initialize smoothed bins
        for(int i=0; i<64; i++) smoothedBins[i] = 0.0f;

        registerParameter("Sensitivity", &sensitivity, 0.00001f, 0.01f, 0.00001f, "Gain");
        registerParameter("Threshold", &threshold, 0.0f, 10000.0f, 100.0f, "Squelch");
        registerParameter("Palette", &palette, "Colors");
        registerParameter("Smoothing", &smoothing, 0.0f, 0.99f, 0.01f, "Decay");
    }

    std::string getTypeName() const override { return "FrequencySpectrum"; }

protected:
    void renderAudioAnimation(uint32_t epoch, CRGB* leds, int numLeds) const override {
        // cast away constness to update internal state
        const_cast<FrequencySpectrumAnimation*>(this)->renderInternal(leds, numLeds);
    }

private:
    void renderInternal(CRGB* leds, int numLeds) {
        // We will map 0..numLeds to 0..64 bins (approx 0-2kHz)
        // Update bins first? 
        // Or update them as we access them? To smooth them properly, we need to update all 64 frames 
        // even if numLeds < 64, but typically numLeds > 64. 
        // Actually, let's just update the bins we use to save cycles, 
        // but if we want consistent smoothing, we should strictly update all bins or mapping might jitter if numLeds changes (unlikely).
        // Let's just update based on the mapping.

        CRGBPalette16 p = palette.toPalette16();

        for (int i = 0; i < numLeds; i++) {
            // Map LED index to frequency bin (0 to 63)
            // Using a log mapping could be better but linear is fine for now.
            // 0 -> 0, numLeds-1 -> 63
            int bin = map(i, 0, numLeds, 0, 63);
            bin = constrain(bin, 0, 63);

            // Get magnitude
            float raw = getMagnitude(bin);
            
            // Apply smoothing
            // If smoothing is 0.9, we keep 90% old, 10% new. 
            // If smoothing is 0.1, we keep 10% old, 90% new.
            // Let's interpret 'smoothing' as "amount of smoothness" -> high val = slow change.
            smoothedBins[bin] = smoothedBins[bin] * smoothing + raw * (1.0f - smoothing);

            // Calculate brightness
            // Subtract threshold from smoothed value
            float val = 0.0f;
            if (smoothedBins[bin] > threshold) {
                val = (smoothedBins[bin] - threshold) * sensitivity;
            }
            val = constrain(val, 0.0f, 1.0f);
            uint8_t brightness = (uint8_t)(val * 255.0f);

            // Calculate color
            // Map LED position to palette
            uint8_t hueIndex = map(i, 0, numLeds, 0, 255);
            CRGB color = ColorFromPalette(p, hueIndex, brightness);

            leds[i] = color;
        }
    }

private:
    float sensitivity;
    float smoothing;
    float threshold;
    DynamicPalette palette;
    
    // Internal state for smoothing
    // AudioReactAnimation defines SAMPLES=256 -> 128 bins. 
    // We only use first 64 (up to 2khz approx).
    float smoothedBins[64];
};

#endif
