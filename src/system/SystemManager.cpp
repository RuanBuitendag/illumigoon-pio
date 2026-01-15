#include "system/SystemManager.h"

SystemManager::SystemManager()
    : ledController(NUM_LEDS),
      wifi(WIFI_SSID, WIFI_PASSWORD),
      manager(ledController),
      ota(wifi, ledController, OTA_SERVER_URL),
      mesh(ledController),
      animationTaskHandle(NULL),
      meshTaskHandle(NULL)
{}

void SystemManager::begin() {
    Serial.begin(115200);
    Serial.println("=== Starting system (SystemManager) ===");

    wifi.begin();
    delay(1000);

    mesh.begin();
    
    // lambda captures 'this' to access 'manager'
    mesh.setOnAnimationChange([this](uint8_t index, uint32_t startTime) {
        Serial.printf("Switching to animation %d at time %u\n", index, startTime);
        manager.setCurrent(index);
    });

    ota.begin();
    ledController.begin();

    // Start Tasks
    xTaskCreatePinnedToCore(
        animationTaskTrampoline,
        "AnimationTask",
        ANIMATION_TASK_STACK_SIZE,
        this,
        ANIMATION_TASK_PRIORITY,
        &animationTaskHandle,
        ANIMATION_TASK_CORE
    );

    xTaskCreatePinnedToCore(
        meshTaskTrampoline,
        "MeshTask",
        MESH_TASK_STACK_SIZE,
        this,
        MESH_TASK_PRIORITY,
        &meshTaskHandle,
        MESH_TASK_CORE
    );
}

void SystemManager::update() {
    wifi.update();
    ota.update();
    vTaskDelay(100 / portTICK_PERIOD_MS);
}

void SystemManager::animationTaskTrampoline(void* parameter) {
    if (parameter) {
        static_cast<SystemManager*>(parameter)->animationTask();
    }
}

void SystemManager::meshTaskTrampoline(void* parameter) {
    if (parameter) {
        static_cast<SystemManager*>(parameter)->meshTask();
    }
}

void SystemManager::animationTask() {
    uint32_t lastSwitchMs = 0;

    while (true) {
        uint32_t networkTime = mesh.getNetworkTime();

        if (mesh.isMaster()) {
            uint32_t now = millis();

            if (now - lastSwitchMs >= ANIMATION_SWITCH_INTERVAL_MS) {
                lastSwitchMs = now;
                manager.next();
                
                // Broadcast new state with current network time
                mesh.broadcastAnimationState(manager.getCurrentIndex(), networkTime);
            }
        }

        // All nodes render locally using synchronized network time
        manager.update(networkTime / 10);

        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}

void SystemManager::meshTask() {
    while (true) {
        mesh.update();
        vTaskDelay(50 / portTICK_PERIOD_MS);
    }
}
