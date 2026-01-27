#pragma once
#include <HTTPClient.h>
#include <Update.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include "system/WifiManager.h"
#include "system/LedController.h"

class OtaManager {
public:
    OtaManager(WiFiManager& wifi, LedController& controller, const char* serverUrl,
               const char* versionEndpoint = "/api/version",
               const char* firmwareEndpoint = "/api/firmware/",
               unsigned long checkInterval = 10000);

    void begin();
    void update();
    void forceCheck();
    String getVersion() const { return currentVersion; }

private:
    WiFiManager& wifi;
    LedController& controller;
    const char* serverUrl;
    const char* versionEndpoint;
    const char* firmwareEndpoint;
    unsigned long checkInterval;
    unsigned long lastCheck = 0;
    bool wasDisconnected = false;

    Preferences prefs;
    String currentVersion;

    int compareVersions(const char* v1, const char* v2);
    void checkForUpdates();
    void performOTA(const char* version);
};
