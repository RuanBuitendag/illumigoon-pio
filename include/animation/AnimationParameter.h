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
    AnimationParameter(const char* n, ParameterType t, void* v, float mn = 0, float mx = 100, float s = 1)
        : name(n), type(t), value(v), min(mn), max(mx), step(s) {}

    const char* name;
    ParameterType type;
    void* value; // Pointer to the actual variable
    
    // UI Metadata
    float min;
    float max;
    float step;
};

#endif
