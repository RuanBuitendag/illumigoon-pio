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

    // 3. If no presets exist, create defaults
    if (manager.getPresetNames().empty()) {
        // --- Fire Defaults ---
        Animation* fireBase = manager.getBaseAnimation("Fire");
        if (fireBase) {
            FireAnimation* fire = (FireAnimation*)fireBase;
            
            // Standard Fire
            fire->setPalette(standardFirePalette);
            fire->setSpeed(0.5f);
            manager.savePreset("Standard Fire", "Fire");
            
            // Cool Fire
            fire->setPalette(coolFirePalette);
            fire->setSpeed(1.0f);
            manager.savePreset("Cool Fire", "Fire");
            
            // Warm Fire
            fire->setPalette(warmFirePalette);
            fire->setSpeed(1.0f);
            manager.savePreset("Warm Fire", "Fire");
        }

        // --- Line Defaults ---
        Animation* lineBase = manager.getBaseAnimation("Line");
        if (lineBase) {
            // LineAnimation params are public or accessible via setParam?
            // Existing LineAnimation doesn't have specific setters, but has registerParameter.
            // Using generic setParam/deserialize is better, but here we can just set params via setters if they exist,
            // OR use the generic setParam API.
            
            // Since LineAnimation logic uses pointers to member vars, updating members via setParam works.
            
            // Default Line
            lineBase->setParam("Line Length", 20);
            lineBase->setParam("Spacing", 90);
            lineBase->setParam("Colour", CRGB(255, 30, 0));
            lineBase->setParam("Speed", 10);
            manager.savePreset("Red Line", "Line");
        }
        
        // --- Sinusoidal Defaults ---
        Animation* sinBase = manager.getBaseAnimation("SinusoidalLines");
        if (sinBase) {
             // We need to set palette. param name "Palette".
             DynamicPalette p;
             p.colors = sinusoidalColors;
             sinBase->setParam("Palette", p);
             sinBase->setParam("Line Length", 10);
             sinBase->setParam("Background", CRGB(30,30,30));
             manager.savePreset("Sinusoidal RGB", "SinusoidalLines");
             
             p.colors = warmWhiteColors;
             sinBase->setParam("Palette", p);
             sinBase->setParam("Line Length", 40);
             sinBase->setParam("Background", CRGB::Black);
             manager.savePreset("Sinusoidal Warm", "SinusoidalLines");
        }
        
        // --- Aurora, StarryNight, Breath, etc can just save their current defaults as presets ---
        if (manager.getBaseAnimation("Aurora")) manager.savePreset("Aurora Default", "Aurora");
        if (manager.getBaseAnimation("StarryNight")) manager.savePreset("Starry Night", "StarryNight");
        
        if (manager.getBaseAnimation("Breathing")) {
            // Default Red Breathing
            manager.savePreset("Red Breathing", "Breathing");
        }
        
        if (manager.getBaseAnimation("AudioWave")) manager.savePreset("Audio Wave", "AudioWave");
        if (manager.getBaseAnimation("KickReaction")) manager.savePreset("Kick Reaction", "KickReaction");
    }
}
