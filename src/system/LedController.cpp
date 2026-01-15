#include "system/LedController.h"

LedController::LedController(int numLeds) 
    : numLeds(numLeds), brightness(255), otaInProgress(false)
{
    leds = new CRGB[numLeds];
    mux = portMUX_INITIALIZER_UNLOCKED;
}

CRGB* LedController::getLeds() const {
    return leds;
}

void LedController::begin() {
    FastLED.addLeds<WS2812B, PIN, GRB>(leds, numLeds);
    FastLED.setBrightness(brightness);
    clear();
}

void LedController::render() {
    if (otaInProgress) return; // skip rendering during OTA
    show();
}

void LedController::clear() {
    fill_solid(leds, numLeds, CRGB::Black);
    show();
}

void LedController::setOtaMode(bool active) {
    otaInProgress = active;
}

int LedController::getNumLeds() const {
    return numLeds;
}

void LedController::showProgress(float fraction) {
    taskENTER_CRITICAL(&mux);
    int ledsOn = fraction * numLeds;
    for (int i = 0; i < numLeds; i++) {
        leds[i] = i < ledsOn ? CRGB::Green : CRGB::Black;
    }
    FastLED.show();
    taskEXIT_CRITICAL(&mux);
}

bool LedController::isOtaInProgress() const {
    return otaInProgress;
}

void LedController::show() {
    taskENTER_CRITICAL(&mux);
    FastLED.show();
    taskEXIT_CRITICAL(&mux);
}
