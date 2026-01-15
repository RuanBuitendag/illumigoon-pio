#ifndef ANIMATION_H
#define ANIMATION_H

#include "system/LedController.h"
#include <cstdint>
#include <vector>
#include "animation/AnimationParameter.h"

class Animation {
public:
	Animation() {}

	virtual ~Animation() {}

	virtual void render(uint32_t epoch, CRGB* leds, int numLeds) const = 0;

    const std::vector<AnimationParameter>& getParameters() const {
        return parameters;
    }

protected:
    void registerParameter(const char* name, int* value) {
        parameters.push_back({name, PARAM_INT, value});
    }

    void registerParameter(const char* name, float* value) {
        parameters.push_back({name, PARAM_FLOAT, value});
    }

    void registerParameter(const char* name, uint8_t* value) {
        parameters.push_back({name, PARAM_BYTE, value});
    }

    void registerParameter(const char* name, CRGB* value) {
        parameters.push_back({name, PARAM_COLOR, value});
    }

    void registerParameter(const char* name, bool* value) {
        parameters.push_back({name, PARAM_BOOL, value});
    }
    
    void registerParameter(const char* name, CRGBPalette16* value) {
        parameters.push_back({name, PARAM_PALETTE, value});
    }

    std::vector<AnimationParameter> parameters;
};

#endif
