#include <Arduino.h>
#include "system/SystemManager.h"

// SystemManager handles all initialization, FreeRTOS tasks, and main loop logic
SystemManager sysManager;

void setup() {
    sysManager.begin();

    sysManager.animation.getCurrentAnimation()->setParam("Line Length", 60); // Example parameter set
    sysManager.animation.getCurrentAnimation()->setParam("Speed", 2); // Example parameter set
    sysManager.animation.getCurrentAnimation()->setParam("Spacing", 45); // Example parameter set
}

void loop() {
    sysManager.update();
}