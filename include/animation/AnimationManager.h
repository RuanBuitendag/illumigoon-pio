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
    void setAnimation(const std::string& name);
    std::string getCurrentAnimationName() const;
    void update(uint32_t epoch);
    std::vector<std::string> getAnimationNames() const;
    Animation* getCurrentAnimation();
    Animation* getAnimation(const std::string& name);

    void setPower(bool on);
    bool getPower() const;

private:
    LedController& controller;
    std::vector<Animation*> animations;
    Animation* currentAnimation;
    bool powerState;
};

#endif
