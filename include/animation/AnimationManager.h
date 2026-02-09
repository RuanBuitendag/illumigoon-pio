#ifndef ANIMATIONMANAGER_H
#define ANIMATIONMANAGER_H

#include <vector>
#include <map>
#include <string>
#include <ArduinoJson.h>

#include <cstdint>
#include "animation/Animation.h"
#include "system/LedController.h"

// Forward declaration if useful, but Animation is needed for vector<Animation*>
// We need Animation.h, but we DON'T need specific animations here.

class AnimationManager {
public:
    AnimationManager(LedController& ctrl);
    ~AnimationManager();

    // Register a singleton base animation (e.g. "Fire", "Wave")
    void registerBaseAnimation(Animation* anim);

    // Preset Management
    void loadPresets(); // Load from LittleFS
    bool savePreset(const std::string& name, const std::string& baseType); // Save current params as new preset
    
    // New: Save preset from raw JSON data (for mesh Sync)
    bool savePresetFromData(const std::string& name, const std::string& baseType, const std::string& paramsJson);
    bool getPresetData(const std::string& name, std::string& baseType, std::string& paramsJson);
    
    bool renamePreset(const std::string& oldName, const std::string& newName);
    bool deletePreset(const std::string& name);
    
    bool exists(const std::string& name) const;
    std::string getAllPresetsJson() const;

    void setAnimation(const std::string& presetName); // Select a PRESET
    std::string getCurrentAnimationName() const; // Returns PRESET name
    
    void update(uint32_t epoch, float phase = 0.0f);
    
    std::vector<std::string> getPresetNames() const;
    std::vector<std::string> getBaseAnimationNames() const;

    Animation* getCurrentAnimation();
    
    // Helper to get access to specific base animation for configuration if needed
    Animation* getBaseAnimation(const std::string& typeName);

    void setPower(bool on);
    bool getPower() const;

    void setDevicePhase(float phase);
    float getDevicePhase() const;

private:
    LedController& controller;
    float devicePhase = 0.0f;

    // Map of BaseType -> Instance (e.g. "Fire" -> FireAnimation*)
    std::map<std::string, Animation*> baseAnimations;

    struct Preset {
        std::string name;
        std::string baseType;
        std::string filePath; // e.g. /presets/my_cool_fire.json
    };
    std::vector<Preset> presets;

    Animation* currentAnimation; // Pointer to one of the baseAnimations
    std::string currentPresetName;

    bool powerState;

    void saveLastPreset();
};


#endif
