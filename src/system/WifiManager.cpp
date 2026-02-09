#include "system/WifiManager.h"
#include <HardwareSerial.h>
#include <ESPmDNS.h>
#include <esp_wifi.h>

WiFiManager::WiFiManager(const char* ssid, const char* password)
    : ssid(ssid), password(password), connecting(false), mdnsStarted(false) {}

void WiFiManager::begin() {
    Serial.print("Connecting to WiFi");
    WiFi.mode(WIFI_STA);
    esp_wifi_set_ps(WIFI_PS_NONE); // Disable power save for reliable ESP-NOW
    WiFi.disconnect();
    WiFi.begin(ssid, password);
}

bool WiFiManager::update() {
    wl_status_t status = WiFi.status();

    if (status != WL_CONNECTED) {
        if (!connecting) {
            Serial.println("\r\nWiFi disconnected. Reconnecting...");
            WiFi.begin(ssid, password);
            connecting = true;
            mdnsStarted = false;
        }
        delay(100); // yield to FreeRTOS/watchdog
        return false;
    } else {
        if (connecting) {
            Serial.println("WiFi reconnected! IP: " + WiFi.localIP().toString());
            connecting = false;
        }
        
        if (!mdnsStarted) {
            if (MDNS.begin("illumigoon")) {
                Serial.println("mDNS responder started: illumigoon.local");
                MDNS.addService("http", "tcp", 80);
                mdnsStarted = true;
            }
        }
        
        return true;
    }
}

bool WiFiManager::connected() { return WiFi.status() == WL_CONNECTED; }

IPAddress WiFiManager::localIP() { return WiFi.localIP(); }
