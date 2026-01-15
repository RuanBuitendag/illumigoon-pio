#include "animation/AnimationManager.h"
#include "animation/AnimationPresets.h"

AnimationManager::AnimationManager(LedController& ctrl) : controller(ctrl), currentAnimation(nullptr) {
    AnimationPresets::createAnimations(*this);
}

AnimationManager::~AnimationManager() {
    for (Animation* anim : animations) {
        delete anim;
    }
    animations.clear();
}

void AnimationManager::add(Animation* anim) {
    animations.push_back(anim);
    if (currentAnimation == nullptr) {
        currentAnimation = anim;
    }
}

void AnimationManager::setAnimation(const std::string& name) {
    for (Animation* anim : animations) {
        if (anim->getName() == name) {
            currentAnimation = anim;
            return;
        }
    }
}

std::string AnimationManager::getCurrentAnimationName() const {
    if (currentAnimation) {
        return currentAnimation->getName();
    }
    return "";
}

std::vector<std::string> AnimationManager::getAnimationNames() const {
    std::vector<std::string> names;
    for (const auto& anim : animations) {
        names.push_back(anim->getName());
    }
    return names;
}

void AnimationManager::update(uint32_t epoch) {
    if (currentAnimation && !controller.isOtaInProgress()) {
        currentAnimation->render(epoch, controller.getLeds(), controller.getNumLeds());
        controller.render();
    }
}

Animation* AnimationManager::getCurrentAnimation() {
     return currentAnimation;
}

Animation* AnimationManager::getAnimation(const std::string& name) {
    for (Animation* anim : animations) {
        if (anim->getName() == name) {
            return anim;
        }
    }
    return nullptr;
}
