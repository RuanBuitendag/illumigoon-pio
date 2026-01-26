#ifndef ANIMATION_H
#define ANIMATION_H

#include "system/LedController.h"
#include <cstdint>
#include <vector>
#include "animation/AnimationParameter.h"
#include <ArduinoJson.h>


class Animation {
public:
    Animation(const std::string& name) : name(name) {}

	virtual ~Animation() {}

	virtual void render(uint32_t epoch, CRGB* leds, int numLeds) const = 0;

    const std::string& getName() const {
        return name;
    }

    const std::vector<AnimationParameter>& getParameters() const {
        return parameters;
    }

    bool setParam(const std::string& name, int value);
    bool setParam(const std::string& name, float value);
    bool setParam(const std::string& name, uint8_t value);
    bool setParam(const std::string& name, bool value);
    bool setParam(const std::string& name, CRGB value);
    bool setParam(const std::string& name, const DynamicPalette& value);
    
    AnimationParameter* findParameter(const std::string& name);

    void resetToDefaults() {
        for (auto& p : parameters) {
            p.resetToDefault();
        }
    }

protected:
    void registerParameter(const char* name, int* value, int min = 0, int max = 255, int step = 1, const char* desc = "") {
        parameters.push_back({name, PARAM_INT, value, desc, (float)min, (float)max, (float)step});
    }

    void registerParameter(const char* name, float* value, float min = 0.0f, float max = 1.0f, float step = 0.01f, const char* desc = "") {
        parameters.push_back({name, PARAM_FLOAT, value, desc, min, max, step});
    }

    void registerParameter(const char* name, uint8_t* value, uint8_t min = 0, uint8_t max = 255, uint8_t step = 1, const char* desc = "") {
        parameters.push_back({name, PARAM_BYTE, value, desc, (float)min, (float)max, (float)step});
    }

    void registerParameter(const char* name, CRGB* value, const char* desc = "") {
        parameters.push_back({name, PARAM_COLOR, value, desc, 0, 0, 0});
    }

    void registerParameter(const char* name, bool* value, const char* desc = "") {
        parameters.push_back({name, PARAM_BOOL, value, desc, 0, 1, 1});
    }
    
    void registerParameter(const char* name, DynamicPalette* value, const char* desc = "") {
        parameters.push_back({name, PARAM_DYNAMIC_PALETTE, value, desc, 0, 0, 0});
    }

    std::vector<AnimationParameter> parameters;
    std::string name;

public:
    virtual std::string getTypeName() const = 0;

    // Serialization
    void serializeParameters(JsonObject& doc) const; // Use JsonObject to append
    bool deserializeParameters(const JsonObject& doc);
};


#endif
