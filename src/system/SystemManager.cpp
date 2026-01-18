#include "system/SystemManager.h"

SystemManager::SystemManager()
    : ledController(NUM_LEDS),
      wifi(WIFI_SSID, WIFI_PASSWORD),
      animation(ledController),
      ota(wifi, ledController, OTA_SERVER_URL, "/api/version", "/api/firmware/", 60000), // 1 minute check interval
      mesh(ledController),
      web(animation, mesh, ota),
      animationTaskHandle(NULL),
      meshTaskHandle(NULL)
{}

void SystemManager::begin() {
    Serial.begin(115200);
    Serial.println("=== Starting system (SystemManager) ===");

    Serial.println("Init: LEDs...");
    ledController.begin();
    Serial.println("Init: LEDs done.");

    Serial.println("Init: WiFi...");
    wifi.begin();
    delay(1000);

    Serial.println("Init: Mesh...");
    mesh.begin();
    
    Serial.println("Init: Web...");
    web.begin();

    Serial.println("Init: OTA...");
    ota.begin();
    
    Serial.println("Init: Tasks...");
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
    Serial.println("Init: Tasks done.");
}

void SystemManager::update() {
    wifi.update();
    ota.update();
    web.update();
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
            // Logic to switch animations based on external input or schedule would go here.
            // For now, we stay on the current animation.
            
            // Periodically broadcast current state to sync new nodes? 
            // Or just rely on state changes.
        }

        // All nodes render locally using synchronized network time
        animation.update(networkTime / 10);

        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}

void SystemManager::meshTask() {
    while (true) {
        mesh.update();
        vTaskDelay(50 / portTICK_PERIOD_MS);
    }
}
