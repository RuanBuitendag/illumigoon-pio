#pragma once

#include <Arduino.h>
#include "system/Config.h"
#include "system/LedController.h"
#include "system/WifiManager.h"
#include "system/OtaManager.h"
#include "system/MeshNetworkManager.h"
#include "animation/AnimationManager.h"
#include "system/WebManager.h"

class SystemManager {
public:
    SystemManager();
    
    void begin();
    void update();

    // Components - Order is critical for dependency injection!
    LedController ledController;
    WiFiManager wifi;
    AnimationManager animation;
    OtaManager ota;
    MeshNetworkManager mesh;

private:
    // Static task entry points
    static void animationTaskTrampoline(void* parameter);
    static void meshTaskTrampoline(void* parameter);

    // Task implementations
    void animationTask();
    void meshTask();

    // FreeRTOS Task Handles
    TaskHandle_t animationTaskHandle;
    TaskHandle_t meshTaskHandle;

    WebManager web;

    // Config Persistence
    void loadConfig();
    void saveConfig();
    std::string lastSavedGroupName;

public:
};
