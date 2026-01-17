#include "system/WebManager.h"
#include <LittleFS.h>

WebManager::WebManager(AnimationManager& video, MeshNetworkManager& mesh)
    : animManager(video), meshManager(mesh), server(80), ws("/ws"), fsMounted(false) {}

void WebManager::begin() {
    if(!LittleFS.begin()){
        Serial.println("An Error has occurred while mounting LittleFS");
        fsMounted = false;
    } else {
        fsMounted = true;
    }

    setupRoutes();
    setupWebSocket();

    server.begin();
    Serial.println("Web Server started");
}

void WebManager::update() {
    ws.cleanupClients();
}

void WebManager::setupRoutes() {
    // CORS Header
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");

    // Static Files (React App)
    server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");

    // API: System Status
    server.on("/api/status", HTTP_GET, [this](AsyncWebServerRequest *request) {
        request->send(200, "application/json", getSystemStatusJson());
    });

    // API: Animations List
    server.on("/api/animations", HTTP_GET, [this](AsyncWebServerRequest *request) {
        request->send(200, "application/json", getAnimationsJson());
    });

    // API: Current Animation Params
    server.on("/api/params", HTTP_GET, [this](AsyncWebServerRequest *request) {
        request->send(200, "application/json", getParamsJson());
    });

    // API: Set Animation
    server.on("/api/animation", HTTP_POST, [this](AsyncWebServerRequest *request) {
        // Body handled in handler... requires AsyncWebServerRequest::onBody equivalent or polling params
    }, NULL, [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
        // Handle body
        StaticJsonDocument<200> doc;
        DeserializationError error = deserializeJson(doc, data, len);
        if (!error) {
            if (doc.containsKey("name")) {
                const char* name = doc["name"];
                animManager.setAnimation(name);
                request->send(200, "application/json", "{\"status\":\"ok\"}");
                
                // Broadcast new params to WS clients
                ws.textAll("{\"event\":\"params\", \"data\":" + getParamsJson() + "}");
            }
        }
    });

    // API: Mesh Peers
    server.on("/api/mesh/peers", HTTP_GET, [this](AsyncWebServerRequest *request) {
        request->send(200, "application/json", getPeersJson());
    });
    
    // CORS Preflight
    // SPA Fallback: Serve index.html for any unknown path (so React Router can handle it)
    server.onNotFound([this](AsyncWebServerRequest *request) {
        if (request->method() == HTTP_OPTIONS) {
            request->send(200);
        } else {
            // If it's an API call that failed, send 404
            if (request->url().startsWith("/api")) {
                 request->send(404, "application/json", "{\"error\":\"Not Found\"}");
            } else {
                // Otherwise serve index.html for React Router
                if (fsMounted && LittleFS.exists("/index.html")) {
                    request->send(LittleFS, "/index.html", "text/html");
                } else {
                    String msg = "404: Not Found";
                    if (!fsMounted) msg += " (LittleFS Mount Failed - Upload Filesystem Image)";
                    else msg += " (index.html missing)";
                    request->send(404, "text/plain", msg);
                }
            }
        }
    });
}

void WebManager::setupWebSocket() {
    ws.onEvent([this](AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
        onWsEvent(server, client, type, arg, data, len);
    });
    server.addHandler(&ws);
}

void WebManager::onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
    if (type == WS_EVT_CONNECT) {
        Serial.printf("WS Client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
        // Send initial state
        client->text("{\"event\":\"status\", \"data\":" + getSystemStatusJson() + "}");
        client->text("{\"event\":\"params\", \"data\":" + getParamsJson() + "}");
    } else if (type == WS_EVT_DISCONNECT) {
        Serial.printf("WS Client #%u disconnected\n", client->id());
    } else if (type == WS_EVT_DATA) {
        handleWebSocketMessage(arg, data, len);
    }
}

void WebManager::handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
    AwsFrameInfo *info = (AwsFrameInfo*)arg;
    if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
        StaticJsonDocument<512> doc;
        DeserializationError error = deserializeJson(doc, data, len);
        if (error) return;

        const char* cmd = doc["cmd"];
        if (strcmp(cmd, "setParam") == 0) {
            const char* name = doc["name"];
            Animation* current = animManager.getCurrentAnimation();
            if (current) {
               // Type handling based on JSON value type
               if (doc["value"].is<int>()) current->setParam(name, doc["value"].as<int>());
               else if (doc["value"].is<float>()) current->setParam(name, doc["value"].as<float>());
               else if (doc["value"].is<bool>()) current->setParam(name, doc["value"].as<bool>());
               // Color handling usually needs parsing from string/object, simplified here
            }
        } else if (strcmp(cmd, "setAnimation") == 0) {
             const char* name = doc["name"];
             animManager.setAnimation(name);
             ws.textAll("{\"event\":\"params\", \"data\":" + getParamsJson() + "}");
        }
    }
}

String WebManager::getSystemStatusJson() {
    StaticJsonDocument<256> doc;
    doc["uptime"] = millis();
    doc["heap"] = ESP.getFreeHeap();
    doc["animation"] = animManager.getCurrentAnimationName();
    doc["ip"] = WiFi.localIP().toString();
    String output;
    serializeJson(doc, output);
    return output;
}

String WebManager::getAnimationsJson() {
    StaticJsonDocument<1024> doc;
    std::vector<std::string> names = animManager.getAnimationNames();
    JsonArray arr = doc.to<JsonArray>();
    for (const auto& name : names) {
        arr.add(name);
    }
    String output;
    serializeJson(doc, output);
    return output;
}

String WebManager::getParamsJson() {
    StaticJsonDocument<2048> doc;
    JsonArray arr = doc.to<JsonArray>();
    Animation* current = animManager.getCurrentAnimation();
    if (current) {
        for (const auto& param : current->getParameters()) {
            JsonObject obj = arr.createNestedObject();
            obj["name"] = param.name;
            obj["type"] = (int)param.type;
            
            // Metadata
            if (param.type == PARAM_INT || param.type == PARAM_FLOAT || param.type == PARAM_BYTE) {
                obj["min"] = param.min;
                obj["max"] = param.max;
                obj["step"] = param.step;
            }

            // Value extraction logic needs to be robust, doing simple switch for now
            switch(param.type) {
                case PARAM_INT: obj["value"] = *(int*)param.value; break;
                case PARAM_FLOAT: obj["value"] = *(float*)param.value; break;
                case PARAM_BOOL: obj["value"] = *(bool*)param.value; break;
                case PARAM_BYTE: obj["value"] = *(uint8_t*)param.value; break;
                case PARAM_COLOR: {
                    CRGB c = *(CRGB*)param.value; 
                    char hex[8]; sprintf(hex, "#%02X%02X%02X", c.r, c.g, c.b);
                    obj["value"] = hex;
                } break;
                default: break;
            }
        }
    }
    String output;
    serializeJson(doc, output);
    return output;
}

String WebManager::getPeersJson() {
    StaticJsonDocument<2048> doc;
    JsonArray arr = doc.to<JsonArray>();
    
    // Self
    JsonObject self = arr.createNestedObject();
    self["id"] = "local";
    self["ip"] = WiFi.localIP().toString();
    self["role"] = meshManager.isMaster() ? "MASTER" : "SLAVE";
    self["self"] = true;

    // Mesh Peers
    std::vector<PeerInfo> peers = meshManager.getPeers();
    for (const auto& peer : peers) {
        JsonObject obj = arr.createNestedObject();
        char idStr[17];
        sprintf(idStr, "%016llX", peer.id);
        obj["id"] = idStr;
        obj["ip"] = IPAddress(peer.ip).toString();
        obj["role"] = (peer.role == NodeState::MASTER) ? "MASTER" : "SLAVE";
        obj["lastSeen"] = peer.lastSeen;
        obj["self"] = false;
    }

    String output;
    serializeJson(doc, output);
    return output;
}
