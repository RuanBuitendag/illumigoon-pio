#include "system/WifiManager.h"
#include <HardwareSerial.h>

WiFiManager::WiFiManager(const char* ssid, const char* password)
    : ssid(ssid), password(password), connecting(false) {}

void WiFiManager::begin() {
    Serial.print("Connecting to WiFi");
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    WiFi.begin(ssid, password);
}

bool WiFiManager::update() {
    wl_status_t status = WiFi.status();

    if (status != WL_CONNECTED) {
        if (!connecting) {
            Serial.println("\nWiFi disconnected. Reconnecting...");
            WiFi.begin(ssid, password);
            connecting = true;
        }
        delay(100); // yield to FreeRTOS/watchdog
        return false;
    } else {
        if (connecting) {
            Serial.println("WiFi reconnected! IP: " + WiFi.localIP().toString());
            connecting = false;
        }
        return true;
    }
}

bool WiFiManager::connected() { return WiFi.status() == WL_CONNECTED; }

IPAddress WiFiManager::localIP() { return WiFi.localIP(); }
