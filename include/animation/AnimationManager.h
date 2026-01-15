#ifndef ANIMATIONMANAGER_H
#define ANIMATIONMANAGER_H

#include <vector>
#include <cstdint>
#include "animation/Animation.h"
#include "system/LedController.h"

// Forward declaration if useful, but Animation is needed for vector<Animation*>
// We need Animation.h, but we DON'T need specific animations here.

class AnimationManager {
public:
    AnimationManager(LedController& ctrl);
    ~AnimationManager();

    void add(Animation* anim);
    void setCurrent(uint8_t index);
    uint8_t getCurrentIndex() const;
    void next();
    void update(uint32_t epoch);
    size_t getAnimationCount() const;
    Animation* getCurrentAnimation();

private:
    LedController& controller;
    std::vector<Animation*> animations;
    uint8_t currentIndex;

    // Resource storage
    CRGBPalette16 coolFirePalette;
    CRGBPalette16 warmFirePalette;
    std::vector<CRGB> sinusoidalColors;
    std::vector<CRGB> warmWhiteColors;

    void initResources();
    void createAnimations();
};

#endif
