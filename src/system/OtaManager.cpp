#include "system/OtaManager.h"

OtaManager::OtaManager(WiFiManager& wifi, LedController& controller, const char* serverUrl,
            const char* versionEndpoint,
            const char* firmwareEndpoint,
            unsigned long checkInterval)
        : wifi(wifi), controller(controller), 
          serverUrl(serverUrl),
          versionEndpoint(versionEndpoint),
          firmwareEndpoint(firmwareEndpoint),
          checkInterval(checkInterval) {}

void OtaManager::begin() {
    Serial.println("=== OTA Manager Starting ===");

    prefs.begin("ota", true);
    currentVersion = prefs.getString("version", "0.0.0");
    prefs.end();

    Serial.print("Current firmware version: ");
    Serial.println(currentVersion);

    lastCheck = millis();
}

void OtaManager::update() {
    if (!wifi.connected()) {
        if (!wasDisconnected) {
            Serial.println("OTA: WiFi disconnected, waiting...");
            wasDisconnected = true;
        }
        return;
    }

    if (wasDisconnected) {
        Serial.println("OTA: WiFi connected, resuming checks");
        wasDisconnected = false;
        lastCheck = 0; // force immediate check
    }

    unsigned long elapsed = millis() - lastCheck;
    if (elapsed < checkInterval) return;

    Serial.println("OTA: Checking for updates...");
    lastCheck = millis();
    checkForUpdates();
}

int OtaManager::compareVersions(const char* v1, const char* v2) {
    int a1, b1, c1;
    int a2, b2, c2;
    sscanf(v1, "%d.%d.%d", &a1, &b1, &c1);
    sscanf(v2, "%d.%d.%d", &a2, &b2, &c2);
    if (a1 != a2) return a1 > a2 ? 1 : -1;
    if (b1 != b2) return b1 > b2 ? 1 : -1;
    if (c1 != c2) return c1 > c2 ? 1 : -1;
    return 0;
}

void OtaManager::checkForUpdates() {
    Serial.println("OTA: Fetching latest version...");

    HTTPClient http;
    String versionUrl = String(serverUrl) + versionEndpoint;
    Serial.println(versionUrl);

    http.begin(versionUrl);
    int httpCode = http.GET();

    if (httpCode != HTTP_CODE_OK) {
        Serial.print("OTA: Version check failed, HTTP ");
        Serial.println(httpCode);
        http.end();
        return;
    }

    String payload = http.getString();
    http.end();

    Serial.print("OTA: Server response: ");
    Serial.println(payload);

    DynamicJsonDocument doc(1024);
    if (deserializeJson(doc, payload)) {
        Serial.println("OTA: JSON parse failed");
        return;
    }

    const char* latestVersion = doc["version"];
    Serial.print("OTA: Latest version: ");
    Serial.println(latestVersion);

    if (compareVersions(currentVersion.c_str(), latestVersion) < 0) {
        Serial.println("OTA: Update available");
        performOTA(latestVersion);
    } else {
        Serial.println("OTA: Already up to date");
    }
}

void OtaManager::performOTA(const char* version) {
    Serial.print("OTA: Downloading firmware ");
    Serial.println(version);

    controller.setOtaMode(true);
    controller.showProgress(1);

    HTTPClient fw;
    String fwUrl = String(serverUrl) + firmwareEndpoint + version;
    Serial.println(fwUrl);

    fw.begin(fwUrl);
    int httpCode = fw.GET();
    if (httpCode != HTTP_CODE_OK) {
        Serial.print("OTA: Firmware download failed, HTTP ");
        Serial.println(httpCode);
        fw.end();
        return;
    }

    int size = fw.getSize();
    Serial.print("OTA: Firmware size ");
    Serial.println(size);

    if (!Update.begin(size)) {
        Serial.println("OTA: Not enough flash space");
        fw.end();
        return;
    }

    size_t written = Update.writeStream(*fw.getStreamPtr());
    Serial.print("OTA: Written ");
    Serial.println(written);

    if (!Update.end(true)) {
        Serial.print("OTA: Update failed, error ");
        Serial.println(Update.getError());
        fw.end();
        return;
    }

    prefs.begin("ota", false);
    prefs.putString("version", version);
    prefs.end();

    Serial.println("OTA: Update successful, rebooting...");
    fw.end();
    delay(500);
    controller.setOtaMode(false);
    ESP.restart();
}
