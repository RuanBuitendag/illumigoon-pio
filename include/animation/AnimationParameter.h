#ifndef ANIMATION_PARAMETER_H
#define ANIMATION_PARAMETER_H

#include <string>
#include <FastLED.h>

enum ParameterType {
    PARAM_INT,
    PARAM_FLOAT,
    PARAM_BYTE,
    PARAM_COLOR,
    PARAM_BOOL,
    PARAM_PALETTE
};

struct AnimationParameter {
    const char* name;
    ParameterType type;
    void* value; // Pointer to the actual variable
};

#endif
