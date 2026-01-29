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
        : Animation(name), palette(palette), speed(speed), bounciness(bounciness), numBalls(numBalls), gravity(9.8f), directionUp(false), ballSize(1) {
            
            // Register parameters
            registerParameter("Speed", &this->speed, 0.1f, 10.0f, 0.5f, "Simulation speed");
            registerParameter("Bounciness", &this->bounciness, 0.1f, 1.2f, 0.05f, "Bounce elasticity");
            registerParameter("Num Balls", &this->numBalls, 1, 20, 1, "Number of balls");
            registerParameter("Ball Size", &this->ballSize, 1, 10, 1, "Size of the balls");
            registerParameter("Direction Up", &this->directionUp, "Fall direction (Up/Down)");
            registerParameter("Palette", &this->palette, "Ball colors");
            registerParameter("Background", &this->backgroundPalette, "Background gradient");
            
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

        // Render Background
        CRGBPalette16 bgPal = backgroundPalette.toPalette16();
        if (backgroundPalette.colors.empty()) {
             for(int i=0; i<numLeds; i++) leds[i] = CRGB::Black;
        } else {
             for(int i=0; i<numLeds; i++) {
                 // Map pixel index to gradient (0.0 to 1.0 along the strip)
                 uint8_t gradientPos = (i * 255) / (numLeds - 1);
                 leds[i] = ColorFromPalette(bgPal, gradientPos);
             }
        }
        
        CRGBPalette16 pal = palette.toPalette16();

        for (auto& ball : balls) {
            if (!ball.active) {
                // Spawn new ball
                spawnBall(ball, numLeds, pal);
                continue;
            }

            // Update Physics
            float currentGravity = directionUp ? -gravity : gravity;
            ball.velocity += currentGravity * dt * 10.0f; // Scale gravity for effect
            ball.position += ball.velocity * dt;

            // Collision logic
            if (!directionUp) {
                // Falling down (0 -> numLeds)
                if (ball.position >= numLeds - 1) {
                    ball.position = numLeds - 1;
                    ball.velocity *= -ball.bounciness;
                    
                    if (fabs(ball.velocity) < 0.5f) {
                         ball.velocity = 0;
                         if (fabs(ball.velocity) < 0.1f) {
                             spawnBall(ball, numLeds, pal);
                         }
                    }
                }
            } else {
                // Falling up (numLeds -> 0)
                if (ball.position <= 0) {
                    ball.position = 0;
                    ball.velocity *= -ball.bounciness;

                    if (fabs(ball.velocity) < 0.5f) {
                         ball.velocity = 0;
                         if (fabs(ball.velocity) < 0.1f) {
                             spawnBall(ball, numLeds, pal);
                         }
                    }
                }
            }
        }
        
        // Render Balls
        for (const auto& ball : balls) {
            if (!ball.active) continue;
            
            int pos = (int)round(ball.position);
            
            // Draw ball with size (trail)
            for (int j = 0; j < ballSize; j++) {
                int drawPos = pos;
                if (!directionUp) {
                    // Falling down, trail is above (smaller index)
                    drawPos = pos - j;
                } else {
                    // Falling up, trail is below (larger index)
                    drawPos = pos + j;
                }

                if (drawPos >= 0 && drawPos < numLeds) {
                    leds[drawPos] = ball.color;
                }
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
        if (!directionUp) {
            ball.position = 0; // Top
            ball.velocity = 0; 
        } else {
            ball.position = numLeds - 1; // Bottom
            ball.velocity = 0;
        }
        
        // Random bounciness
        float variation = (random8() / 255.0f) * 0.2f - 0.1f;
        ball.bounciness = bounciness + variation;
        if (ball.bounciness < 0.1f) ball.bounciness = 0.1f;

        // Sample random color
        ball.color = ColorFromPalette(pal, random8(255));
        
        ball.active = true;
    }

    mutable DynamicPalette palette;
    mutable DynamicPalette backgroundPalette;
    float speed;
    float bounciness;
    int numBalls;
    float gravity;
    bool directionUp;
    int ballSize;
    
    // State
    mutable std::vector<Ball> balls;
    mutable uint32_t lastUpdate = 0;
};

#endif
