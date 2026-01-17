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
// #include "animation/user_animations/FlowingLinesAnimation.h" // Seems exists but not used in original manager, verify if needed later?

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
    manager.add(new AudioWaveAnimation("Audio Wave"));
    manager.add(new KickReactionAnimation("Kick Reaction"));



    // Line Animation
    manager.add(new LineAnimation("Line", 20, 90, CRGB(255, 30, 0), 10));

        // Breathing Animation (AHDSR)
    // Attack=2000ms, Hold=1000ms, Decay=0, Sustain=255, SusTime=0, Release=2000ms, Rest=1000ms
    manager.add(new BreathingAnimation("Breathing", CRGB(255, 20, 0), 2000, 1000, 0, 255, 0, 2000, 0));

    // Fire Animations
    manager.add(new FireAnimation("Fire", standardFirePalette, 0.5f));           // Standard
    manager.add(new FireAnimation("CoolFire", coolFirePalette, 1.0f));    // Cool
    manager.add(new FireAnimation("WarmFire", warmFirePalette, 1.0f));    // Warm

    // Aurora
    manager.add(new AuroraAnimation("Aurora", 54321));

    // Starry Night
    manager.add(new StarryNightAnimation("StarryNight", 15));

    // Sinusoidal Lines
    manager.add(new SinusoidalLinesAnimation("Sinusoidal", sinusoidalColors, 10, 1.0f, 5.0f, CRGB(30,30,30)));
    manager.add(new SinusoidalLinesAnimation("SinusoidalDark", sinusoidalColors, 10, 1.0f, 5.0f)); // Dark variant (no bg?)
    manager.add(new SinusoidalLinesAnimation("SinusoidalWarm", warmWhiteColors, 40, 1.0f, 2.0f));
}
