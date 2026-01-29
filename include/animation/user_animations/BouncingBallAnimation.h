#ifndef BOUNCING_BALL_ANIMATION_H
#define BOUNCING_BALL_ANIMATION_H

#include "animation/Animation.h"
#include <vector>

struct Ball {
    float position;
    float velocity;
    float bounciness;
    CRGB color;
    bool active;
};

class BouncingBallAnimation : public Animation {
public:
    BouncingBallAnimation(const std::string& name, const DynamicPalette& palette, int numBalls = 3, float speed = 1.0f, float bounciness = 0.8f)
        : Animation(name), palette(palette), speed(speed), bounciness(bounciness), numBalls(numBalls), gravity(9.8f) {
            
            // Register parameters
            registerParameter("Speed", &this->speed, 0.1f, 5.0f, 0.1f, "Simulation speed");
            registerParameter("Bounciness", &this->bounciness, 0.1f, 1.2f, 0.05f, "Bounce elasticity");
            registerParameter("Num Balls", &this->numBalls, 1, 20, 1, "Number of balls");
            registerParameter("Palette", &this->palette, "Ball colors");
            
            // Initialize balls
            resizeBalls();
        }

    std::string getTypeName() const override { return "BouncingBall"; }

    void render(uint32_t epoch, CRGB* leds, int numLeds) const override {
        // Handle resizing if parameter changed
        if (balls.size() != (size_t)numBalls) {
            const_cast<BouncingBallAnimation*>(this)->resizeBalls();
        }

        // Calculate time delta
        uint32_t dtMs = epoch - lastUpdate;
        lastUpdate = epoch;
        
        // Cap dt to prevent huge jumps if frame rate drops
        if (dtMs > 100) dtMs = 100;
        
        // Convert to seconds
        float dt = (dtMs / 1000.0f) * speed;

        // Clear LEDs
        for(int i=0; i<numLeds; i++) leds[i] = CRGB::Black;
        
        CRGBPalette16 pal = palette.toPalette16();

        for (auto& ball : balls) {
            if (!ball.active) {
                // Spawn new ball
                spawnBall(ball, numLeds, pal);
                continue;
            }

            // Update Physics
            ball.velocity += gravity * dt * 10.0f; // Scale gravity for effect
            ball.position += ball.velocity * dt;

            // Collision with bottom (end of strip)
            // Assuming 0 is top and numLeds is bottom, or vice versa?
            // "spawn balls from the top and makes them bounce on the bottom"
            // Let's assume index 0 is top, numLeds-1 is bottom.
            // Gravity pulls towards positive index.
            
            if (ball.position >= numLeds - 1) {
                ball.position = numLeds - 1;
                ball.velocity *= -ball.bounciness;
                
                // Add some damping to prevent infinite tiny bounces
                if (fabs(ball.velocity) < 0.5f) {
                     ball.velocity = 0;
                     // Respawn if it stopped bouncing? or let it sit?
                     // "spawns balls" implies a stream or cycle.
                     // Let's respawn if it stops or after some time.
                     if (fabs(ball.velocity) < 0.1f) {
                         // Mark for respawn (maybe delay?)
                         // For now, simple logic: if it settles at bottom, respawn at top
                         spawnBall(ball, numLeds, pal);
                     }
                }
            }
        }
        
        // Render
        for (const auto& ball : balls) {
            if (!ball.active) continue;
            
            int pos = (int)round(ball.position);
            if (pos >= 0 && pos < numLeds) {
                // Draw ball with some anti-aliasing or just pixel?
                // Simple pixel for now
                leds[pos] = ball.color;
            }
        }
    }

private:
    void resizeBalls() {
        balls.resize(numBalls);
        // Reset new balls
        for (auto& ball : balls) {
             ball.active = false;
        }
    }

    void spawnBall(Ball& ball, int numLeds, const CRGBPalette16& pal) const {
        ball.position = 0; // Top
        ball.velocity = 0; // Starts from rest or small push?
        
        // Random bounciness: "slightly different random bouncinesses"
        // Variation +/- 10%
        float variation = (random8() / 255.0f) * 0.2f - 0.1f;
        ball.bounciness = bounciness + variation;
        if (ball.bounciness < 0.1f) ball.bounciness = 0.1f;

        // Sample random color
        ball.color = ColorFromPalette(pal, random8(255));
        
        // Stagger starts?
        // We can just set active true immediately, relying on random render loop to space them out naturally as they die?
        // Or adding a "start delay"?
        // For simplicity:
        ball.active = true;
    }

    mutable DynamicPalette palette;
    float speed;
    float bounciness;
    int numBalls;
    float gravity;
    
    // State
    mutable std::vector<Ball> balls;
    mutable uint32_t lastUpdate = 0;
};

#endif
