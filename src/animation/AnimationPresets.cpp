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
// #include "animation/user_animations/FlowingLinesAnimation.h" // Seems exists but not used in original manager, verify if needed later?

// Define internal resources locally
namespace {
    CRGBPalette16 coolFirePalette = CRGBPalette16(
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

    CRGBPalette16 warmFirePalette = CRGBPalette16(
        CRGB::Black, CRGB(30, 10, 0), CRGB(60, 20, 0), CRGB(100, 40, 0),
        CRGB(150, 60, 10), CRGB(180, 80, 20), CRGB(200, 100, 30), CRGB(220, 120, 40),
        CRGB(220, 120, 40), CRGB(200, 100, 30), CRGB(180, 80, 20), CRGB(150, 60, 10),
        CRGB(100, 40, 0), CRGB(60, 20, 0), CRGB(30, 10, 0), CRGB::Black
    );

    const std::vector<CRGB> sinusoidalColors = {CRGB::Red, CRGB::DarkOrange, CRGB::Blue};
    const std::vector<CRGB> warmWhiteColors = {CRGB(255, 100, 20), CRGB(255, 100, 20), CRGB(255, 100, 20)};
}

void AnimationPresets::createAnimations(AnimationManager& manager) {
    manager.add(new AudioWaveAnimation("Audio Wave"));


    // Line Animation
    // manager.add(new LineAnimation("Line", 20, 90, CRGB(255, 30, 0), 10));

        // Breathing Animation (AHDSR)
    // Attack=2000ms, Hold=1000ms, Decay=0, Sustain=255, SusTime=0, Release=2000ms, Rest=1000ms
    manager.add(new BreathingAnimation("Breathing", CRGB(255, 20, 0), 2000, 1000, 0, 255, 0, 2000, 0));

    // Fire Animations
    manager.add(new FireAnimation("Fire", HeatColors_p, 0.5f));           // Standard
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
