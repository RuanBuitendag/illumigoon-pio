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

private:
    void show();
    
    const int numLeds;
    int brightness;
    CRGB* leds;
    portMUX_TYPE mux;
    bool otaInProgress;
};

#endif
