#ifndef LINEANIMATION_H
#define LINEANIMATION_H

#include "animation/Animation.h"
#include <FastLED.h>

class LineAnimation : public Animation {
public:
    LineAnimation(const std::string& name, int lineLength, int spacing, CRGB colour, int speed)
        : Animation(name), lineLength(lineLength), spacing(spacing), colour(colour), speed(speed)
    {
        registerParameter("Line Length", &this->lineLength, 0, 90, 1, "Length of segments");
        registerParameter("Spacing", &this->spacing, 0, 90, 1, "Distance between segments");
        registerParameter("Colour", &this->colour, "Line color");
        registerParameter("Speed", &this->speed, 0, 100, 1, "Animation speed multiplier");
    }

    std::string getTypeName() const override { return "Line"; }

    void render(uint32_t epoch, CRGB* leds, int numLeds) const override {

        // Use a clearer cycle length
        int cycle = lineLength + spacing;
        if (cycle == 0) cycle = 1; // Prevent divide by zero

        // Calculate offset based on time and speed
        // epoch is 60Hz (approx 16ms), speed is likely 0-255? or arbitrary.
        // Let's assume speed is pixels per second or similar scaler.
        // Actually, let's just make it a multiplier for the epoch.
        // If speed is 10, offset increases by 10 every tick? That's fast.
        // Let's try: offset = (epoch * speed) / 10;
        // Or if speed is just an arbitrary rate parameter.
        int offset = (epoch * speed) / 10; 

        for (int i = 0; i < numLeds; i++) {
            // Calculate effective position in the cycle
            // We want the line to move "forward" (index 0 to N) or "backward"?
            // Usually valid to have it move 0->N. 
            // Position in cycle = (pixel_index - offset) % cycle.
            // Using (pixel_index + offset) makes it move left-to-right or right-to-left depending on perspective.
            
            // Let's use (i - offset) to move 'right' (0 -> N) effectively shifting pattern 'right'.
            int pos = (i - offset) % cycle;
            if (pos < 0) pos += cycle;

            if (pos < lineLength) {
                leds[i] = colour;
            } else {
                leds[i] = CRGB::Black;
            }
        }
    }

private:
    int lineLength;
    int spacing;
    CRGB colour;
    int speed;
};

#endif