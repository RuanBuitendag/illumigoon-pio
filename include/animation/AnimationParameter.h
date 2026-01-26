#ifndef ANIMATION_PARAMETER_H
#define ANIMATION_PARAMETER_H

#include <string>
#include <FastLED.h>

#include <vector>

struct DynamicPalette {
    std::vector<CRGB> colors;

    // Helper to fill a CRGBPalette16 with gradients based on the colors
    CRGBPalette16 toPalette16() const {
        if (colors.empty()) return CRGBPalette16(CRGB::Black);
        
        CRGBPalette16 pal;
        // Treat pal as array of 16 CRGBs
        CRGB* palBuf = (CRGB*)pal;

        if (colors.size() == 1) {
            fill_solid(palBuf, 16, colors[0]);
        } else {
            // Fill gradients
            // We have N colors. We have 16 slots.
            // We want to fill from slot 0 to 15.
            // Example 2 colors: fill_gradient_RGB(palBuf, 0, c1, 15, c2).
            // Example 3 colors: 0->c1, 7->c2, 15->c3.
            
            // Re-using FastLED fill_gradient_RGB. 
            // It allows chaining.
            
            // If more colors than 16, we truncate/sample? 
            // Assume < 16 usually.
            
            // Strategy: Loop through segments.
            int numSegments = colors.size() - 1;
            float segmentLength = 15.0f / numSegments;
            
            for (int i = 0; i < numSegments; i++) {
                int startPos = (int)(i * segmentLength);
                int endPos = (int)((i + 1) * segmentLength);
                if (i == numSegments - 1) endPos = 15; // Ensure we hit the end
                
                fill_gradient_RGB(palBuf, startPos, colors[i], endPos, colors[i+1]);
            }
        }
        return pal;
    }
};

enum ParameterType {
    PARAM_INT,
    PARAM_FLOAT,
    PARAM_BYTE,
    PARAM_COLOR,
    PARAM_BOOL,
    PARAM_DYNAMIC_PALETTE
};

struct AnimationParameter {
    // Default values storage union
    union DefaultValue {
        int intVal;
        float floatVal;
        uint8_t byteVal;
        bool boolVal;
        uint32_t colorVal; // CRGB as packed RGB
        // DynamicPalette is too complex for union, stored separately
    };

    AnimationParameter(const char* n, ParameterType t, void* v, const char* desc = "", float mn = 0, float mx = 100, float s = 1)
        : name(n), type(t), value(v), description(desc), min(mn), max(mx), step(s) {
        // Store the initial value as the default
        storeCurrentAsDefault();
    }

    const char* name;
    const char* description;
    ParameterType type;
    void* value; // Pointer to the actual variable
    
    // UI Metadata
    float min;
    float max;
    float step;

    // Default value storage
    DefaultValue defaultVal;
    DynamicPalette defaultPalette; // For PARAM_DYNAMIC_PALETTE type

    void storeCurrentAsDefault() {
        switch (type) {
            case PARAM_INT:
                defaultVal.intVal = *static_cast<int*>(value);
                break;
            case PARAM_FLOAT:
                defaultVal.floatVal = *static_cast<float*>(value);
                break;
            case PARAM_BYTE:
                defaultVal.byteVal = *static_cast<uint8_t*>(value);
                break;
            case PARAM_BOOL:
                defaultVal.boolVal = *static_cast<bool*>(value);
                break;
            case PARAM_COLOR: {
                CRGB c = *static_cast<CRGB*>(value);
                defaultVal.colorVal = ((uint32_t)c.r << 16) | ((uint32_t)c.g << 8) | c.b;
                break;
            }
            case PARAM_DYNAMIC_PALETTE:
                defaultPalette = *static_cast<DynamicPalette*>(value);
                break;
        }
    }

    void resetToDefault() {
        switch (type) {
            case PARAM_INT:
                *static_cast<int*>(value) = defaultVal.intVal;
                break;
            case PARAM_FLOAT:
                *static_cast<float*>(value) = defaultVal.floatVal;
                break;
            case PARAM_BYTE:
                *static_cast<uint8_t*>(value) = defaultVal.byteVal;
                break;
            case PARAM_BOOL:
                *static_cast<bool*>(value) = defaultVal.boolVal;
                break;
            case PARAM_COLOR: {
                CRGB* c = static_cast<CRGB*>(value);
                c->r = (defaultVal.colorVal >> 16) & 0xFF;
                c->g = (defaultVal.colorVal >> 8) & 0xFF;
                c->b = defaultVal.colorVal & 0xFF;
                break;
            }
            case PARAM_DYNAMIC_PALETTE:
                *static_cast<DynamicPalette*>(value) = defaultPalette;
                break;
        }
    }
};

#endif
