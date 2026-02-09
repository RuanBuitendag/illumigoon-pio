#include "animation/AnimationManager.h"
#include "animation/AnimationPresets.h"
#include <LittleFS.h>
#include <ArduinoJson.h>

AnimationManager::AnimationManager(LedController& ctrl) : controller(ctrl), currentAnimation(nullptr), powerState(true), devicePhase(0.0f) {
    if (!LittleFS.begin(true)) {
        // Serial.println("LittleFS Mount Failed");
        // Handle error?
    }
    
    // Create directory if not exists
    if (!LittleFS.exists("/presets")) {
        LittleFS.mkdir("/presets");
    }

    AnimationPresets::createAnimations(*this);
    loadPresets();
    
    // Load Phase Persistence
    if (LittleFS.exists("/phase.json")) {
        File f = LittleFS.open("/phase.json", "r");
        if (f) {
            StaticJsonDocument<128> doc;
            deserializeJson(doc, f);
            devicePhase = doc["phase"] | 0.0f;
            f.close();
        }
    }

    // If we have presets, select the first one?
    // Or maybe restore last used?
    if (!presets.empty()) {
        setAnimation(presets[0].name);
    } else {
        // Fallback to first base animation if no presets exist
        if (!baseAnimations.empty()) {
             setAnimation(baseAnimations.begin()->first);
        }
    }
}

AnimationManager::~AnimationManager() {
    for (auto const& kv : baseAnimations) {
        delete kv.second;
    }
    baseAnimations.clear();
}


void AnimationManager::registerBaseAnimation(Animation* anim) {
    if (anim) {
        baseAnimations[anim->getTypeName()] = anim;
    }
}

void AnimationManager::loadPresets() {
    presets.clear();
    
    File dir = LittleFS.open("/presets");
    if (!dir || !dir.isDirectory()) {
        // create defaults if empty? handled in AnimationPresets maybe?
        return;
    }

    File file = dir.openNextFile();
    while (file) {
        if (!file.isDirectory()) {
            // Check extension .json
            std::string path = file.path(); // e.g. /presets/CoolFire.json (Note: LittleFS might differ on path format)
            if (String(file.name()).endsWith(".json")) {
                // Read metadata
                // We need to open it to get the name and baseType?
                // Or assume filename is the name? 
                // Better to read the file content to be robust.
                
                // For efficiency, maybe just rely on filename = name? 
                // Let's iterate and parse to get BaseType and Name.
                
                // Re-open file to read from start if needed, but file is already open handle?
                // `file` from openNextFile is a handle.
                
                // Let's read the whole file
                // size check
                if (file.size() < 4096) {
                     StaticJsonDocument<2048> doc;
                     DeserializationError error = deserializeJson(doc, file);
                     if (!error) {
                         const char* name = doc["name"];
                         const char* baseType = doc["baseType"];
                         if (name && baseType) {
                             Preset p;
                             p.name = name;
                             p.baseType = baseType;
                             p.filePath = (String("/presets/") + file.name()).c_str();
                             presets.push_back(p);
                         }
                     }
                }
            }
        }
        file = dir.openNextFile();
    }
}

bool AnimationManager::savePreset(const std::string& name, const std::string& baseType) {
    // Check if base animation exists
    if (baseAnimations.find(baseType) == baseAnimations.end()) return false;
    
    Animation* anim = baseAnimations[baseType];
    
    // Construct Path
    std::string filename = name; 
    std::string path = "/presets/" + filename + ".json";
    
    File file = LittleFS.open(path.c_str(), FILE_WRITE);
    if (!file) return false;
    
    DynamicJsonDocument doc(2048);
    doc["name"] = name;
    doc["baseType"] = baseType;
    
    JsonObject params = doc.createNestedObject("params");
    anim->serializeParameters(params);
    
    if (serializeJson(doc, file) == 0) {
        file.close();
        return false;
    }
    file.close();
    
    // Reload presets to update list
    loadPresets(); 
    return true;
}

bool AnimationManager::renamePreset(const std::string& oldName, const std::string& newName) {
    // 1. Check if new name already exists
    for (const auto& p : presets) {
        if (p.name == newName) return false;
    }

    // 2. Find old preset
    auto it = std::find_if(presets.begin(), presets.end(), [&](const Preset& p){ return p.name == oldName; });
    if (it == presets.end()) return false;

    const Preset& oldPreset = *it;

    // 3. Update file content and path
    std::string newPath = "/presets/" + newName + ".json";
    
    File fRead = LittleFS.open(oldPreset.filePath.c_str(), FILE_READ);
    if (!fRead) return false;
    
    DynamicJsonDocument doc(2048);
    DeserializationError error = deserializeJson(doc, fRead);
    fRead.close();
    
    if (error) return false;
    
    doc["name"] = newName;
    
    File fWrite = LittleFS.open(newPath.c_str(), FILE_WRITE);
    if (!fWrite) return false;
    
    if (serializeJson(doc, fWrite) == 0) {
        fWrite.close();
        LittleFS.remove(newPath.c_str()); // cleanup
        return false;
    }
    fWrite.close();
    
    // 4. Delete old file
    LittleFS.remove(oldPreset.filePath.c_str());
    
    // 5. Keep current selection if we just renamed the current preset
    bool wasCurrent = (currentPresetName == oldName);

    // 6. Reload presets to update list
    loadPresets(); 
    
    if (wasCurrent) {
        currentPresetName = newName;
    }
    
    return true;
}

bool AnimationManager::deletePreset(const std::string& name) {
    for (const auto& p : presets) {
        if (p.name == name) {
            LittleFS.remove(p.filePath.c_str());
            loadPresets();
            return true;
        }
    }
    return false;
}

bool AnimationManager::exists(const std::string& name) const {
    for (const auto& p : presets) {
        if (p.name == name) return true;
    }
    return false;
}

bool AnimationManager::savePresetFromData(const std::string& name, const std::string& baseType, const std::string& paramsJson) {
     // Check if base animation exists
    if (baseAnimations.find(baseType) == baseAnimations.end()) return false;
    
    std::string filename = name; 
    std::string path = "/presets/" + filename + ".json";
    
    File file = LittleFS.open(path.c_str(), FILE_WRITE);
    if (!file) return false;
    
    DynamicJsonDocument doc(4096);
    doc["name"] = name;
    doc["baseType"] = baseType;
    
    // Parse paramsJson and put into doc
    DynamicJsonDocument paramsDoc(2048);
    deserializeJson(paramsDoc, paramsJson);
    doc["params"] = paramsDoc.as<JsonObject>();
    
    if (serializeJson(doc, file) == 0) {
        file.close();
        return false;
    }
    file.close();
    
    loadPresets(); 
    return true;
}



bool AnimationManager::getPresetData(const std::string& name, std::string& baseType, std::string& paramsJson) {
     const Preset* targetPreset = nullptr;
    for (const auto& p : presets) {
        if (p.name == name) {
            targetPreset = &p;
            break;
        }
    }
    
    if (!targetPreset) return false;
    
    File file = LittleFS.open(targetPreset->filePath.c_str(), FILE_READ);
    if (!file) return false;
    
    DynamicJsonDocument doc(4096);
    DeserializationError error = deserializeJson(doc, file);
    file.close();
    
    if (error) return false;
    
    baseType = doc["baseType"].as<std::string>();
    
    if (doc.containsKey("params")) {
        // Serialize just the params object back to JSON string
        serializeJson(doc["params"], paramsJson);
    } else {
        paramsJson = "{}";
    }
    
    return true;
}


std::string AnimationManager::getAllPresetsJson() const {
    DynamicJsonDocument doc(4096);
    JsonArray arr = doc.to<JsonArray>();
    
    for (const auto& p : presets) {
        // Read file content and add to array
        File file = LittleFS.open(p.filePath.c_str(), FILE_READ);
        if (file) {
            DynamicJsonDocument pDoc(1024);
            deserializeJson(pDoc, file);
            arr.add(pDoc.as<JsonObject>());
            file.close();
        }
    }
    
    std::string output;
    serializeJson(doc, output);
    return output;
}

void AnimationManager::setAnimation(const std::string& name) {
    // 1. Try to find as Preset
    const Preset* targetPreset = nullptr;
    for (const auto& p : presets) {
        if (p.name == name) {
            targetPreset = &p;
            break;
        }
    }
    
    if (targetPreset) {
        // Find base animation
        auto it = baseAnimations.find(targetPreset->baseType);
        if (it == baseAnimations.end()) return;
        
        Animation* anim = it->second;
        
        // Load parameters from file
        File file = LittleFS.open(targetPreset->filePath.c_str(), FILE_READ);
        if (file) {
            DynamicJsonDocument doc(2048);
            deserializeJson(doc, file);
            file.close();
            
            if (doc.containsKey("params")) {
                JsonObject params = doc["params"];
                anim->deserializeParameters(params);
            }
        }
        
        currentAnimation = anim;
        currentPresetName = name;
        return;
    }
    
    // 2. Try to find as Base Animation
    auto it = baseAnimations.find(name);
    if (it != baseAnimations.end()) {
         // Reset to defaults when selecting a base animation directly
         it->second->resetToDefaults();
         currentAnimation = it->second;
         currentPresetName = name; // Treat base name as the current "preset" name
         return;
    }
}


std::string AnimationManager::getCurrentAnimationName() const {
    return currentPresetName;
}

std::vector<std::string> AnimationManager::getPresetNames() const {
    std::vector<std::string> names;
    for (const auto& p : presets) {
        names.push_back(p.name);
    }
    return names;
}

std::vector<std::string> AnimationManager::getBaseAnimationNames() const {
    std::vector<std::string> names;
    for (auto const& kv : baseAnimations) {
        names.push_back(kv.first);
    }
    return names;
}


void AnimationManager::update(uint32_t epoch, float phase) {
    if (currentAnimation && !controller.isOtaInProgress()) {
        if (powerState) {
            currentAnimation->setDevicePhase(devicePhase);
            currentAnimation->render(epoch, controller.getLeds(), controller.getNumLeds());
            
            // Apply Animation Brightness
            uint8_t animBrightness = currentAnimation->getBrightness();
            if (animBrightness < 255) {
                 nscale8_video(controller.getLeds(), controller.getNumLeds(), animBrightness);
            }

            controller.render();
        } else {
            controller.clear();
        }
    }
}

void AnimationManager::setPower(bool on) {
    powerState = on;
}

bool AnimationManager::getPower() const {
    return powerState;
}

Animation* AnimationManager::getCurrentAnimation() {
     return currentAnimation;
}

Animation* AnimationManager::getBaseAnimation(const std::string& typeName) {
    auto it = baseAnimations.find(typeName);
    if (it != baseAnimations.end()) {
        return it->second;
    }
    return nullptr;
}

void AnimationManager::setDevicePhase(float phase) {
    if (phase < 0.0f) phase = 0.0f;
    if (phase > 1.0f) phase = 1.0f;
    devicePhase = phase;

    // Persist
    File f = LittleFS.open("/phase.json", "w");
    if (f) {
        StaticJsonDocument<128> doc;
        doc["phase"] = devicePhase;
        serializeJson(doc, f);
        f.close();
    }
}

float AnimationManager::getDevicePhase() const {
    return devicePhase;
}
