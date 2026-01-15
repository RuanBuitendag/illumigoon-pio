#include <Arduino.h>
#include "system/SystemManager.h"

// SystemManager handles all initialization, FreeRTOS tasks, and main loop logic
SystemManager sysManager;

void setup() {
    sysManager.begin();
}

void loop() {
    sysManager.update();
}