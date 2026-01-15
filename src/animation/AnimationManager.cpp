#include "animation/AnimationManager.h"

// Include all animations here, not in the header
#include "animation/user_animations/LineAnimation.h"
#include "animation/user_animations/AuroraAnimation.h"
#include "animation/user_animations/FireAnimation.h"
#include "animation/user_animations/StarryNightAnimation.h"
#include "animation/user_animations/SinusoidalLinesAnimation.h"

AnimationManager::AnimationManager(LedController& ctrl) : controller(ctrl), currentIndex(0) {
    initResources();
    createAnimations();
}

AnimationManager::~AnimationManager() {
    for (Animation* anim : animations) {
        delete anim;
    }
    animations.clear();
}

void AnimationManager::add(Animation* anim) {
    animations.push_back(anim);
}

void AnimationManager::setCurrent(uint8_t index) {
    if (index < animations.size()) {
        currentIndex = index;
    }
}

uint8_t AnimationManager::getCurrentIndex() const {
    return currentIndex;
}

void AnimationManager::next() {
    if (animations.empty()) return;
    currentIndex = (currentIndex + 1) % animations.size();
}

void AnimationManager::update(uint32_t epoch) {
    if (!animations.empty() && !controller.isOtaInProgress()) {
        animations[currentIndex]->render(epoch, controller.getLeds(), controller.getNumLeds());
        controller.render();
    }
}

size_t AnimationManager::getAnimationCount() const {
    return animations.size();
}

Animation* AnimationManager::getCurrentAnimation() {
     if (animations.empty()) return nullptr;
     return animations[currentIndex];
}

void AnimationManager::initResources() {
    // Initialize Palettes
    coolFirePalette = CRGBPalette16(
        CRGB::Black,              // cold smoke
        CRGB(10, 0, 20),          // very dark purple
        CRGB(25, 0, 40),          // deep violet
        CRGB(40, 0, 60),          // velvet purple
        CRGB(60, 0, 50),          // purple-red
        CRGB(80, 0, 40),          // deep red-violet
        CRGB(100, 0, 30),         // dark red
        CRGB(120, 0, 20),         // hotter red
        CRGB(140, 10, 10),        // ember red
        CRGB(160, 20, 10),        // hot ember
        CRGB(180, 30, 10),        // very hot ember
        CRGB(200, 40, 10),        // near-core
        CRGB(220, 60, 20),        // muted orange core
        CRGB(240, 80, 30),        // hottest (still restrained)
        CRGB(200, 40, 20),        // roll-off
        CRGB::Black               // fade back
    );

    warmFirePalette = CRGBPalette16(
        CRGB::Black, CRGB(30, 10, 0), CRGB(60, 20, 0), CRGB(100, 40, 0),
        CRGB(150, 60, 10), CRGB(180, 80, 20), CRGB(200, 100, 30), CRGB(220, 120, 40),
        CRGB(220, 120, 40), CRGB(200, 100, 30), CRGB(180, 80, 20), CRGB(150, 60, 10),
        CRGB(100, 40, 0), CRGB(60, 20, 0), CRGB(30, 10, 0), CRGB::Black
    );

    // Initialize Vectors
    sinusoidalColors = {CRGB::Red, CRGB::DarkOrange, CRGB::Blue};
    warmWhiteColors = {CRGB(255, 100, 20), CRGB(255, 100, 20), CRGB(255, 100, 20)};
}

void AnimationManager::createAnimations() {
    // Line Animation
    add(new LineAnimation(20, 90, CRGB(255, 30, 0)));

    // Fire Animations
    add(new FireAnimation(HeatColors_p, 1.0f));           // Standard
    add(new FireAnimation(coolFirePalette, 1.0f));        // Cool
    add(new FireAnimation(warmFirePalette, 1.0f));        // Warm

    // Aurora
    add(new AuroraAnimation(54321));

    // Starry Night
    add(new StarryNightAnimation(15));

    // Sinusoidal Lines
    add(new SinusoidalLinesAnimation(sinusoidalColors, 10, 1.0f, 5.0f, CRGB(30,30,30)));
    add(new SinusoidalLinesAnimation(sinusoidalColors, 10, 1.0f, 5.0f)); // Dark variant (no bg?)
    add(new SinusoidalLinesAnimation(warmWhiteColors, 40, 1.0f, 2.0f));
}
