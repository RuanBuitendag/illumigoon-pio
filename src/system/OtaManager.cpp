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

    // Ensure previous connections are fully closed
    delay(100);

    controller.setOtaMode(true);
    // Show minimal progress (1 pixel) to indicate "Connecting" state vs "Off"
    controller.showProgress(0.01f);

    HTTPClient fwClient;
    String fwUrl = String(serverUrl) + firmwareEndpoint + version;
    Serial.println(fwUrl);

    fwClient.setConnectTimeout(10000); // 10s timeout
    fwClient.setTimeout(10000);
    
    // Explicitly begin with WiFiClient if needed, but standard begin should handle it.
    // Using begin(url) covers basic cases.
    if (!fwClient.begin(fwUrl)) {
        Serial.println("OTA: Connection failed to initialize");
        controller.flashColor(CRGB::Red, 3);
        controller.setOtaMode(false);
        return;
    }

    int httpCode = fwClient.GET();
    if (httpCode != HTTP_CODE_OK) {
        Serial.print("OTA: Firmware download failed, HTTP ");
        Serial.println(httpCode);
        controller.flashColor(CRGB::Red, 3);
        controller.setOtaMode(false);
        fwClient.end();
        return;
    }

    int totalLength = fwClient.getSize();
    Serial.print("OTA: Firmware size ");
    Serial.println(totalLength);

    if (totalLength <= 0) {
        Serial.println("OTA: Invalid content length");
        controller.flashColor(CRGB::Red, 3);
        controller.setOtaMode(false);
        fwClient.end();
        return;
    }

    if (!Update.begin(totalLength)) {
        Serial.print("OTA: Not enough flash space (");
        Serial.print(totalLength);
        Serial.println(")");
        controller.flashColor(CRGB::Red, 3);
        controller.setOtaMode(false);
        fwClient.end();
        return;
    }

    WiFiClient* stream = fwClient.getStreamPtr();
    uint8_t buff[128] = { 0 };
    size_t written = 0;
    int lastPercent = -1;

    while (fwClient.connected() && (written < totalLength)) {
        size_t size = stream->available();
        if (size) {
            int c = stream->readBytes(buff, ((size > sizeof(buff)) ? sizeof(buff) : size));
            size_t w = Update.write(buff, c);
            if (w != c) {
               Serial.println("OTA: Write failed!");
               break; 
            }
            written += c;
            
            int percent = (written * 100) / totalLength;
            if (percent > lastPercent) {
                lastPercent = percent;
                controller.showProgress((float)written / (float)totalLength);
            }
        }
        delay(1); // Give other tasks a chance
    }
    
    Serial.print("OTA: Written ");
    Serial.println(written);

    if (written != totalLength) {
        Serial.println("OTA: Download ended early");
        Update.end();
        controller.flashColor(CRGB::Red, 3);
        controller.setOtaMode(false);
        fwClient.end();
        return;
    }

    if (!Update.end(true)) {
        Serial.print("OTA: Update failed, error ");
        Serial.println(Update.getError());
        controller.flashColor(CRGB::Red, 3);
        controller.setOtaMode(false);
        fwClient.end();
        return;
    }

    prefs.begin("ota", false);
    prefs.putString("version", version);
    prefs.end();

    Serial.println("OTA: Update successful, rebooting...");
    fwClient.end();
    
    controller.flashColor(CRGB::Green, 3);
    
    delay(500);
    controller.setOtaMode(false);
    ESP.restart();
}
