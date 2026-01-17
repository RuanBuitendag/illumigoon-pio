#include "animation/AnimationPresets.h"
#include "animation/AnimationManager.h"
#include <FastLED.h>

// Include all user animations
#include "animation/user_animations/LineAnimation.h"
#include "animation/user_animations/AuroraAnimation.h"
#include "animation/user_animations/FireAnimation.h"
#include "animation/user_animations/StarryNightAnimation.h"
#include "animation/user_animations/SinusoidalLinesAnimation.h"
#include "animation/user_animations/BreathingAnimation.h"
#include "animation/user_animations/AudioWaveAnimation.h"
#include "animation/user_animations/KickReactionAnimation.h"

// Define internal resources locally
namespace {
    DynamicPalette coolFirePalette = {{
        CRGB::Black,
        CRGB::Blue,
        CRGB::Cyan,
        CRGB::White
    }};

    DynamicPalette warmFirePalette = {{
        CRGB::Black,
        CRGB::Red,
        CRGB::Orange,
        CRGB::Yellow
    }};
    
    DynamicPalette standardFirePalette = {{
        CRGB::Black,
        CRGB(160, 0, 0), // Dark Red
        CRGB::Red,
        CRGB::Yellow,
        CRGB::White
    }};

    const std::vector<CRGB> sinusoidalColors = {CRGB::Red, CRGB::DarkOrange, CRGB::Blue};
    const std::vector<CRGB> warmWhiteColors = {CRGB(255, 100, 20), CRGB(255, 100, 20), CRGB(255, 100, 20)};
}

void AnimationPresets::createAnimations(AnimationManager& manager) {
    // 1. Register Base Animations
    // Note: The names passed to constructors here are less important now as they are overridden by preset names, 
    // but useful for the base type ID if getTypeName() returned name (but we implemented getTypeName separately).
    
    manager.registerBaseAnimation(new AudioWaveAnimation("AudioWave"));
    manager.registerBaseAnimation(new KickReactionAnimation("KickReaction"));
    manager.registerBaseAnimation(new LineAnimation("Line", 20, 90, CRGB(255, 30, 0), 10));
    manager.registerBaseAnimation(new BreathingAnimation("Breathing", CRGB(255, 20, 0), 2000, 1000, 0, 255, 0, 2000, 0));
    manager.registerBaseAnimation(new FireAnimation("Fire", standardFirePalette, 0.5f));
    manager.registerBaseAnimation(new AuroraAnimation("Aurora", 54321));
    manager.registerBaseAnimation(new StarryNightAnimation("StarryNight", 15));
    manager.registerBaseAnimation(new SinusoidalLinesAnimation("SinusoidalLines", sinusoidalColors, 10, 1.0f, 5.0f, CRGB(30,30,30)));

    // 2. Load existing presets
    manager.loadPresets();
}
