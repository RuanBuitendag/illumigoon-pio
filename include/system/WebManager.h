#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include "animation/AnimationManager.h"
#include "system/MeshNetworkManager.h"

class WebManager {
public:
    WebManager(AnimationManager& video, MeshNetworkManager& mesh);
    
    void begin();
    void update(); // Call in loop for WS cleanup if needed

private:
    AnimationManager& animManager;
    MeshNetworkManager& meshManager;
    AsyncWebServer server;
    AsyncWebSocket ws;
    bool fsMounted;

    void setupRoutes();
    void setupWebSocket();
    void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len);
    void handleWebSocketMessage(void *arg, uint8_t *data, size_t len);

    // Helpers
    String getSystemStatusJson();
    String getAnimationsJson();
    String getParamsJson();
    String getPeersJson();
};
