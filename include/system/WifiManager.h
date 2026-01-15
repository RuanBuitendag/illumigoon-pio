#pragma once
#include <WiFi.h>

class WiFiManager {
public:
    WiFiManager(const char* ssid, const char* password);

    // Call once in setup()
    void begin();

    // Call periodically in loop() or a task
    bool update();

    bool connected();
    IPAddress localIP();

private:
    const char* ssid;
    const char* password;
    bool connecting;
};
