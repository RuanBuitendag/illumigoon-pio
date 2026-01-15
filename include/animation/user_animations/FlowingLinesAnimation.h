#ifndef FLOWINGLINESANIMATION_H
#define FLOWINGLINESANIMATION_H

#include "Animation.h"
#include <FastLED.h>
#include <cstdlib>

struct Car {
    float position;
    float speed;
    CRGB colour;
};

class FlowingLinesAnimation : public Animation {
public:
    FlowingLinesAnimation(const CRGB* colours, int numColours, int numCars, uint32_t seed)
        : numCars(numCars), numColours(numColours)
    {
        cars = new Car[numCars];
        colourArray = new CRGB[numColours];
        for (int i = 0; i < numColours; i++)
            colourArray[i] = colours[i];

        srand(seed);
    }

    ~FlowingLinesAnimation() {
        delete[] cars;
        delete[] colourArray;
    }

    void render(uint32_t epoch, CRGB* leds, int numLeds) const override {
        if (!initialized) {
            for (int i = 0; i < numCars; i++) {
                cars[i].position = rand() % numLeds;
                cars[i].speed = 0.2f + ((rand() % 100) / 500.0f);
                cars[i].colour = colourArray[rand() % numColours];
            }
            initialized = true;
        }

        for (int i = 0; i < numCars; i++) {
            cars[i].position += cars[i].speed;
            if (cars[i].position >= numLeds)
                cars[i].position -= numLeds;

            int pos = (int)cars[i].position % numLeds;
            leds[pos] = cars[i].colour;
        }
    }

private:
    mutable Car* cars;
    int numCars;
    int numColours;
    CRGB* colourArray;
    mutable bool initialized = false;
};

#endif