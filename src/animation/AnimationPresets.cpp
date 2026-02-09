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
#include "animation/user_animations/BouncingBallAnimation.h"
#include "animation/user_animations/FrequencySpectrumAnimation.h"
#include "animation/user_animations/ReferenceAudioAnimation.h"

// Define internal resources locally
void AnimationPresets::createAnimations(AnimationManager& manager) {
    // 1. Register Base Animations
    // The names are now hardcoded in the animation classes
    
    manager.registerBaseAnimation(new AudioWaveAnimation());
    manager.registerBaseAnimation(new KickReactionAnimation());
    manager.registerBaseAnimation(new LineAnimation());
    manager.registerBaseAnimation(new BreathingAnimation());
    manager.registerBaseAnimation(new FireAnimation());
    manager.registerBaseAnimation(new AuroraAnimation());
    manager.registerBaseAnimation(new StarryNightAnimation());
    manager.registerBaseAnimation(new SinusoidalLinesAnimation());
    manager.registerBaseAnimation(new BouncingBallAnimation());
    manager.registerBaseAnimation(new FrequencySpectrumAnimation());
    manager.registerBaseAnimation(new ReferenceAudioAnimation());

    // 2. Load existing presets
    manager.loadPresets();
}
