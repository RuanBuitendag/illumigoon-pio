#include "system/LedController.h"

LedController::LedController(int numLeds) 
    : numLeds(numLeds), brightness(255), otaInProgress(false)
{
    leds = new CRGB[numLeds];
    mutex = xSemaphoreCreateMutex();
}

CRGB* LedController::getLeds() const {
    return leds;
}

void LedController::begin() {
    Serial.println("  > LedController::begin");
    FastLED.addLeds<WS2812B, PIN, GRB>(leds, numLeds);
    FastLED.setBrightness(brightness);
    clear();
    Serial.println("  > LedController::begin done");
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
    // Only print occasionally or it floods serial
    // Serial.printf("  > showProgress %.2f\r\n", fraction); 
    
    if (xSemaphoreTake(mutex, portMAX_DELAY)) {
        int ledsOn = fraction * numLeds;
        for (int i = 0; i < numLeds; i++) {
            leds[i] = i < ledsOn ? CRGB::Green : CRGB::Black;
        }
        FastLED.show();
        xSemaphoreGive(mutex);
    }
}

bool LedController::isOtaInProgress() const {
    return otaInProgress;
}

void LedController::show() {
    if (xSemaphoreTake(mutex, portMAX_DELAY)) {
        FastLED.show();
        xSemaphoreGive(mutex);
    }
}

void LedController::flashColor(CRGB color, int count, int intervalMs) {
    for (int i = 0; i < count; i++) {
        // ON
        fill_solid(leds, numLeds, color);
        show();
        delay(intervalMs);
        
        // OFF
        fill_solid(leds, numLeds, CRGB::Black);
        show();
        delay(intervalMs);
    }
}
