#pragma once

#include <Arduino.h>
#include "system/Config.h"
#include "system/LedController.h"
#include "system/WifiManager.h"
#include "system/OtaManager.h"
#include "system/MeshNetworkManager.h"
#include "animation/AnimationManager.h"

class SystemManager {
public:
    SystemManager();
    
    void begin();
    void update();

private:
    // Static task entry points
    static void animationTaskTrampoline(void* parameter);
    static void meshTaskTrampoline(void* parameter);

    // Task implementations
    void animationTask();
    void meshTask();

    // Components - Order is critical for dependency injection!
    LedController ledController;
    WiFiManager wifi;
    AnimationManager manager;
    OtaManager ota;
    MeshNetworkManager mesh;

    // FreeRTOS Task Handles
    TaskHandle_t animationTaskHandle;
    TaskHandle_t meshTaskHandle;
};
