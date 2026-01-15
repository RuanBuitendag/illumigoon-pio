#ifndef LEDCONTROLLER_H
#define LEDCONTROLLER_H

#include <FastLED.h>
#include <algorithm>

#define PIN 4

class LedController {
public:
    LedController(int numLeds);
    
    CRGB* getLeds() const;
    void begin();
    void render();
    void clear();
    void setOtaMode(bool active);
    int getNumLeds() const;
    void showProgress(float fraction);
    bool isOtaInProgress() const;
    void flashColor(CRGB color, int count = 3, int intervalMs = 250);

private:
    void show();
    
    const int numLeds;
    int brightness;
    CRGB* leds;
    SemaphoreHandle_t mutex;
    bool otaInProgress;
};

#endif
