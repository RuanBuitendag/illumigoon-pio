#ifndef REFERENCEAUDIOANIMATION_H
#define REFERENCEAUDIOANIMATION_H

#include "animation/AudioReactAnimation.h"

class ReferenceAudioAnimation : public AudioReactAnimation {
public:
    enum FilterMode {
        LPF,
        HPF
    };

    ReferenceAudioAnimation()
        : AudioReactAnimation("Reference Audio"),
          currentBrightness(0.0f),
          useLPF(true), // Default to LPF
          frequencyCutoff(200.0f),
          attackTime(100),
          releaseTime(400)
    {
        // Calculate factors
        recalculateFactors();
        
        // Register parameters
        registerParameter("Use LPF", &useLPF, "Low Pass Filter if true, High Pass if false");
        registerParameter("Cutoff Freq", &frequencyCutoff, 0.0f, 4000.0f, 10.0f, "Cutoff Frequency (Hz)");
    }

    std::string getTypeName() const override { return "Reference Audio"; }

protected:
    void renderAudioAnimation(uint32_t epoch, CRGB* leds, int numLeds) const override {
        // cast away constness to update internal state
        const_cast<ReferenceAudioAnimation*>(this)->processAudio();
        const_cast<ReferenceAudioAnimation*>(this)->draw(leds, numLeds);
    }

private:
   void recalculateFactors() {
        // User code: float attackFactor  = (1000000.0 / attackTime)  / SAMPLING_FREQ;
        // Assuming attackTime is user provided int. 
        // Note: protect against div by zero?
        if (attackTime > 0) attackFactor = (1000000.0f / attackTime) / SAMPLING_FREQ;
        else attackFactor = 1.0f;
        
        if (releaseTime > 0) releaseFactor = (1000000.0f / releaseTime) / SAMPLING_FREQ;
        else releaseFactor = 1.0f;
   }

    void processAudio() {
        // User code logic:
        /*
          bandEnergy = 0.0;
          for (int i = 1; i < SAMPLES / 2; i++) {
            float freq = (i * SAMPLING_FREQ) / SAMPLES;

            if (
              (filterMode == LPF && freq <= frequencyCutoff) ||
              (filterMode == HPF && freq >= frequencyCutoff)
            ) {
              bandEnergy += vReal[i];
            }
          }
        */
        
        float bandEnergy = 0.0f;
        int halfSamples = SAMPLES / 2;
        
        // Base class renders FFT into vReal (magnitude)
        for (int i = 1; i < halfSamples; i++) {
            float freq = (i * SAMPLING_FREQ) / SAMPLES;
            
            bool pass = false;
            if (useLPF) { // LPF
                if (freq <= frequencyCutoff) pass = true;
            } else { // HPF
                if (freq >= frequencyCutoff) pass = true;
            }
            
            if (pass) {
                // vReal holds magnitude after complexToMagnitude in base class
                bandEnergy += vReal[i]; 
            }
        }

        /*
          float targetBrightness = constrain(
            map(bandEnergy, 80000, 300000, 0, 255),
            0, 255
          );

          if (targetBrightness > currentBrightness) {
            currentBrightness += attackFactor * (targetBrightness - currentBrightness);
          } else {
            currentBrightness -= releaseFactor * (currentBrightness - targetBrightness);
          }
        */

        // map() in standard Arduino is long integer math. 
        // We can do float math for smoother results or stick to int/long as requested?
        // Let's stick to float for intermediate but cast to preserve logic
        float mapped = (bandEnergy - 80000.0f) * (255.0f / (300000.0f - 80000.0f));
        float targetBrightness = constrain(mapped, 0.0f, 255.0f);
        
        // Re-calc factors if needed (parameters might change, but for now we calc in ctor. 
        // Ideally we should recalculate if params change, but Animation doesn't have an "onParamChange" hook easily accessible yet without polling).
        // Since parameters are pointers to variables, they update instantly. 
        // We really should recalc factors every frame or just use the formula directly. Constants are cheap.
        recalculateFactors(); 

        if (targetBrightness > currentBrightness) {
            currentBrightness += attackFactor * (targetBrightness - currentBrightness);
        } else {
            currentBrightness -= releaseFactor * (currentBrightness - targetBrightness);
        }
    }

    void draw(CRGB* leds, int numLeds) {
        /*
          uint8_t brightness = (uint8_t)currentBrightness;
          uint8_t hue = 200;

          fill_solid(leds, NUM_LEDS, CHSV(hue, 255, brightness));
        */
        
        uint8_t bright = (uint8_t)currentBrightness;
        uint8_t hue = 200; // Hardcoded in user request
        
        fill_solid(leds, numLeds, CHSV(hue, 255, bright));
    }

private:
    float currentBrightness;
    
    // Parameters
    bool useLPF;
    float frequencyCutoff;
    
    // Constants (could be params)
    int attackTime;
    int releaseTime;
    
    float attackFactor;
    float releaseFactor;
};

#endif
