#include "system/WebManager.h"
#include <LittleFS.h>

WebManager::WebManager(AnimationManager& video, MeshNetworkManager& mesh)
    : animManager(video), meshManager(mesh), server(80), ws("/ws"), fsMounted(false) {}

void WebManager::begin() {
    if(!LittleFS.begin(true)){
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

    // API: Animations List (Presets)
    server.on("/api/animations", HTTP_GET, [this](AsyncWebServerRequest *request) {
        request->send(200, "application/json", getAnimationsJson());
    });

    // API: Base Animations List
    server.on("/api/baseAnimations", HTTP_GET, [this](AsyncWebServerRequest *request) {
        request->send(200, "application/json", getBaseAnimationsJson());
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

    // API: Save Preset
    server.on("/api/savePreset", HTTP_POST, [this](AsyncWebServerRequest *request) {}, NULL, [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
        StaticJsonDocument<512> doc;
        DeserializationError error = deserializeJson(doc, data, len);
        if (!error) {
            const char* name = doc["name"];
            const char* baseType = doc["baseType"];
            if (name && baseType) {
                if (animManager.savePreset(name, baseType)) {
                    request->send(200, "application/json", "{\"status\":\"saved\"}");
                } else {
                    request->send(500, "application/json", "{\"error\":\"Save failed\"}");
                }
            } else {
                request->send(400, "application/json", "{\"error\":\"Missing name or baseType\"}");
            }
        } else {
            request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
        }
    });

    // API: Rename Preset
    server.on("/api/renamePreset", HTTP_POST, [this](AsyncWebServerRequest *request) {}, NULL, [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
        StaticJsonDocument<256> doc;
        DeserializationError error = deserializeJson(doc, data, len);
        if (!error && doc.containsKey("oldName") && doc.containsKey("newName")) {
            const char* oldName = doc["oldName"];
            const char* newName = doc["newName"];
            if (animManager.renamePreset(oldName, newName)) {
                request->send(200, "application/json", "{\"status\":\"renamed\"}");
            } else {
                request->send(500, "application/json", "{\"error\":\"Rename failed\"}");
            }
        } else {
            request->send(400, "application/json", "{\"error\":\"Invalid JSON or missing fields\"}");
        }
    });

    // API: Delete Preset
    server.on("/api/deletePreset", HTTP_POST, [this](AsyncWebServerRequest *request) {}, NULL, [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
        StaticJsonDocument<200> doc;
        DeserializationError error = deserializeJson(doc, data, len);
        if (!error && doc.containsKey("name")) {
            const char* name = doc["name"];
            if (animManager.deletePreset(name)) {
                request->send(200, "application/json", "{\"status\":\"deleted\"}");
            } else {
                request->send(500, "application/json", "{\"error\":\"Delete failed\"}");
            }
        } else {
            request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
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
            bool changed = false;

            if (current) {
               // Type handling based on JSON value type
               if (doc["value"].is<int>()) {
                   current->setParam(name, doc["value"].as<int>());
                   changed = true;
               }
               else if (doc["value"].is<float>()) {
                   current->setParam(name, doc["value"].as<float>());
                   changed = true;
               }
               else if (doc["value"].is<bool>()) {
                   current->setParam(name, doc["value"].as<bool>());
                   changed = true;
               }
               else if (doc["value"].is<const char*>()) {
                   const char* val = doc["value"];
                   // Simple hex parser #RRGGBB
                   if (val[0] == '#' && strlen(val) == 7) {
                       int r, g, b;
                       if (sscanf(val + 1, "%02x%02x%02x", &r, &g, &b) == 3) {
                           current->setParam(name, CRGB(r, g, b));
                           changed = true;
                       }
                   }
               }
                else if (doc["value"].is<JsonArray>()) {
                   // Handle Dynamic Palette
                   JsonArray arr = doc["value"];
                   if (current->findParameter(name)->type == PARAM_DYNAMIC_PALETTE) {
                       DynamicPalette newPal;
                       for (JsonVariant v : arr) {
                           const char* val = v.as<const char*>();
                           if (val && val[0] == '#' && strlen(val) == 7) {
                               int r, g, b;
                               if (sscanf(val + 1, "%02x%02x%02x", &r, &g, &b) == 3) {
                                   newPal.colors.push_back(CRGB(r, g, b));
                               }
                           }
                       }
                       // Ensure at least one color exists?
                       if (newPal.colors.empty()) newPal.colors.push_back(CRGB::Black);

                       current->setParam(name, newPal);
                       changed = true;
                   }
               }
               
               if (changed) {
                   // Broadcast new params to all connected clients
                   ws.textAll("{\"event\":\"params\", \"data\":" + getParamsJson() + "}");
               }
            }
        } else if (strcmp(cmd, "setAnimation") == 0) {
             const char* name = doc["name"];
             animManager.setAnimation(name);
             ws.textAll("{\"event\":\"params\", \"data\":" + getParamsJson() + "}");
        } else if (strcmp(cmd, "reboot") == 0) {
             ESP.restart();
        } else if (strcmp(cmd, "setPower") == 0) {
             bool p = doc["value"];
             animManager.setPower(p);
             ws.textAll("{\"event\":\"status\", \"data\":" + getSystemStatusJson() + "}");
        }
    }
}

String WebManager::getSystemStatusJson() {
    StaticJsonDocument<256> doc;
    doc["uptime"] = millis();
    doc["heap"] = ESP.getFreeHeap();
    doc["animation"] = animManager.getCurrentAnimationName();
    doc["power"] = animManager.getPower();
    doc["ip"] = WiFi.localIP().toString();
    String output;
    serializeJson(doc, output);
    return output;
}

String WebManager::getAnimationsJson() {
    StaticJsonDocument<2048> doc; // Increased size for potentially many presets
    std::vector<std::string> names = animManager.getPresetNames();
    JsonArray arr = doc.to<JsonArray>();
    for (const auto& name : names) {
        arr.add(name);
    }
    String output;
    serializeJson(doc, output);
    return output;
}

String WebManager::getBaseAnimationsJson() {
    StaticJsonDocument<1024> doc;
    std::vector<std::string> names = animManager.getBaseAnimationNames();
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
    // Root is object
    Animation* current = animManager.getCurrentAnimation();
    if (current) {
        doc["baseType"] = current->getTypeName();
        JsonArray arr = doc.createNestedArray("params");
        
        for (const auto& param : current->getParameters()) {
            JsonObject obj = arr.createNestedObject();
            obj["name"] = param.name;
            obj["description"] = param.description;
            obj["type"] = (int)param.type;
            
            if (param.type == PARAM_INT || param.type == PARAM_FLOAT || param.type == PARAM_BYTE) {
                obj["min"] = param.min;
                obj["max"] = param.max;
                obj["step"] = param.step;
            }

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
                case PARAM_DYNAMIC_PALETTE: {
                    DynamicPalette* pal = (DynamicPalette*)param.value;
                    JsonArray palArr = obj.createNestedArray("value");
                    for (const auto& c : pal->colors) {
                        char hex[8]; sprintf(hex, "#%02X%02X%02X", c.r, c.g, c.b);
                        palArr.add(hex);
                    }
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
