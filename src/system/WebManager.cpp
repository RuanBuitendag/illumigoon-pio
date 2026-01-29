#include "system/WebManager.h"
#include <LittleFS.h>

WebManager::WebManager(AnimationManager& video, MeshNetworkManager& mesh, OtaManager& ota)
    : animManager(video), meshManager(mesh), otaManager(ota), server(80), ws("/ws"), fsMounted(false) {}

void WebManager::begin() {
    if(!LittleFS.begin(true)){
        Serial.println("An Error has occurred while mounting LittleFS");
        fsMounted = false;
    } else {
        fsMounted = true;
    }

    setupRoutes();
    setupWebSocket();

    meshManager.setAnimationManager(&animManager);
    
    // Register OTA Callback
    meshManager.setOtaCallback([this]() {
        Serial.println("WebManager: Triggering OTA check from mesh request");
        otaManager.forceCheck();
    });

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
        // Body handled in handler... 
    }, NULL, [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
        StaticJsonDocument<200> doc;
        DeserializationError error = deserializeJson(doc, data, len);
        if (!error) {
            if (doc.containsKey("name")) {
                const char* name = doc["name"];
                animManager.setAnimation(name);
                request->send(200, "application/json", "{\"status\":\"ok\"}");
                
                // Broadcast to WS
                ws.textAll("{\"event\":\"params\", \"data\":" + getParamsJson() + "}");
                ws.textAll("{\"event\":\"status\", \"data\":" + getSystemStatusJson() + "}");
                
                // Broadcast to Mesh
                meshManager.broadcastAnimationState(name, 0); 
            }
        }
    });

    // API: Save Preset
    server.on("/api/savePreset", HTTP_POST, [this](AsyncWebServerRequest *request) {}, NULL, [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
        if (total > len) { request->send(400, "application/json", "{\"error\":\"Payload too large\"}"); return; }
        
        DynamicJsonDocument doc(1200); 
        DeserializationError error = deserializeJson(doc, data, len);
        
        if (!error && doc.containsKey("name") && doc.containsKey("baseType")) {
            const char* name = doc["name"];
            const char* baseType = doc["baseType"];
            if (animManager.savePreset(name, baseType)) {
                // Mesh Broadcast
                std::string path = "/presets/" + std::string(name) + ".json";
                File f = LittleFS.open(path.c_str(), FILE_READ);
                if (f) {
                    DynamicJsonDocument pDoc(2048); 
                    deserializeJson(pDoc, f);
                    f.close();
                    String paramsJson;
                    serializeJson(pDoc["params"], paramsJson);
                    meshManager.broadcastSavePreset(name, baseType, paramsJson.c_str());
                }
                
                // WS Broadcast
                ws.textAll("{\"event\":\"animations\", \"data\":" + getAnimationsJson() + "}");

                request->send(200, "application/json", "{\"status\":\"saved\"}");
            } else {
                request->send(500, "application/json", "{\"error\":\"Save failed\"}");
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
                meshManager.broadcastRenamePreset(oldName, newName);
                ws.textAll("{\"event\":\"animations\", \"data\":" + getAnimationsJson() + "}");
                request->send(200, "application/json", "{\"status\":\"renamed\"}");
            } else {
                request->send(500, "application/json", "{\"error\":\"Rename failed\"}");
            }
        } else {
            request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
        }
    });

    // API: Delete Preset
    server.on("/api/deletePreset", HTTP_POST, [this](AsyncWebServerRequest *request) {}, NULL, [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
        StaticJsonDocument<200> doc;
        DeserializationError error = deserializeJson(doc, data, len);
        if (!error && doc.containsKey("name")) {
            const char* name = doc["name"];
            if (animManager.deletePreset(name)) {
                meshManager.broadcastDeletePreset(name);
                ws.textAll("{\"event\":\"animations\", \"data\":" + getAnimationsJson() + "}");
                request->send(200, "application/json", "{\"status\":\"deleted\"}");
            } else {
                request->send(500, "application/json", "{\"error\":\"Delete failed\"}");
            }
        } else {
            request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
        }
    });

    // API: Check Preset Exists
    server.on("/api/checkPreset", HTTP_GET, [this](AsyncWebServerRequest *request) {
        if (request->hasParam("name")) {
            String name = request->getParam("name")->value();
            bool exists = meshManager.checkPresetExists(name.c_str());
            String json = "{\"exists\":";
            json += (exists ? "true" : "false");
            json += "}";
            request->send(200, "application/json", json);
        } else {
             request->send(400, "application/json", "{\"error\":\"Missing name param\"}");
        }
    });

    // API: Export Presets
    server.on("/api/presets/export", HTTP_GET, [this](AsyncWebServerRequest *request) {
         std::string json = animManager.getAllPresetsJson();
         request->send(200, "application/json", json.c_str());
    });

    // API: Mesh Peers
    server.on("/api/mesh/peers", HTTP_GET, [this](AsyncWebServerRequest *request) {
        request->send(200, "application/json", getPeersJson());
    });

    // API: Assign Group
    server.on("/api/mesh/assign_group", HTTP_POST, [this](AsyncWebServerRequest *request) {}, NULL, [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
        StaticJsonDocument<256> doc;
        DeserializationError error = deserializeJson(doc, data, len);
        if (!error && doc.containsKey("id") && doc.containsKey("group")) {
            const char* idStr = doc["id"];
            const char* group = doc["group"];
            
            if (strcmp(idStr, "local") == 0) {
                meshManager.setGroupName(group);
                request->send(200, "application/json", "{\"status\":\"ok\"}");
            } else {
                uint64_t targetId = strtoull(idStr, NULL, 16);
                meshManager.broadcastAssignGroup(targetId, group);
                request->send(200, "application/json", "{\"status\":\"broadcast_sent\"}");
            }
        } else {
            request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
        }
    });

    // API: Set My Group (Direct)
    server.on("/api/mesh/my_group", HTTP_POST, [this](AsyncWebServerRequest *request) {}, NULL, [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
        StaticJsonDocument<256> doc;
        DeserializationError error = deserializeJson(doc, data, len);
        if (!error && doc.containsKey("group")) {
            meshManager.setGroupName(doc["group"]);
            request->send(200, "application/json", "{\"status\":\"ok\"}");
        } else {
              request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
        }
    });

    // API: Trigger OTA Check (Legacy/Backup)
    server.on("/api/ota/check", HTTP_POST, [this](AsyncWebServerRequest *request) {
        Serial.println("API: Triggering OTA check");
        otaManager.forceCheck();
        meshManager.broadcastCheckForUpdates();
        request->send(200, "application/json", "{\"status\":\"ok\"}");
    });
    
    // Static Files (React App) - Must be last to avoid capturing API routes
    server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");

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
        Serial.printf("WS Client #%u connected from %s\r\n", client->id(), client->remoteIP().toString().c_str());
        // Send initial state
        client->text("{\"event\":\"status\", \"data\":" + getSystemStatusJson() + "}");
        client->text("{\"event\":\"params\", \"data\":" + getParamsJson() + "}");
    } else if (type == WS_EVT_DISCONNECT) {
        Serial.printf("WS Client #%u disconnected\r\n", client->id());
    } else if (type == WS_EVT_DATA) {
        handleWebSocketMessage(client, arg, data, len);
    }
}

void WebManager::handleWebSocketMessage(AsyncWebSocketClient *client, void *arg, uint8_t *data, size_t len) {
    AwsFrameInfo *info = (AwsFrameInfo*)arg;
    if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
        StaticJsonDocument<1024> doc; // Increased for larger payloads if needed
        DeserializationError error = deserializeJson(doc, data, len);
        if (error) return;

        const char* cmd = doc["cmd"];

        // --- READ COMMANDS (Unicast to Client) ---
        if (strcmp(cmd, "getStatus") == 0) {
            client->text("{\"event\":\"status\", \"data\":" + getSystemStatusJson() + "}");
        } 
        else if (strcmp(cmd, "getAnimations") == 0) {
            client->text("{\"event\":\"animations\", \"data\":" + getAnimationsJson() + "}");
        }
        else if (strcmp(cmd, "getBaseAnimations") == 0) {
            client->text("{\"event\":\"baseAnimations\", \"data\":" + getBaseAnimationsJson() + "}");
        }
        else if (strcmp(cmd, "getParams") == 0) {
            client->text("{\"event\":\"params\", \"data\":" + getParamsJson() + "}");
        }
        else if (strcmp(cmd, "getPeers") == 0) {
            client->text("{\"event\":\"peers\", \"data\":" + getPeersJson() + "}");
        }

        // --- WRITE COMMANDS (Broadcast to All + Mesh) ---
        else if (strcmp(cmd, "setParam") == 0) {
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
                   
                   // Broadcast to Mesh Group DISABLED for Frontend Sync (as per user request previously, they might want it back?)
                   // Current logic: Frontend sends to each device individually via WS if needed.
                   // Actually, for setParam, we usually want group sync if it's a "Group" action.
                   // But "setParam" from UI is usually device specific target.
                   // Let's keep mesh broadcast DISABLED here for now as established.
               }
            }
        } 
        else if (strcmp(cmd, "setAnimation") == 0) {
             const char* name = doc["name"];
             animManager.setAnimation(name);
             ws.textAll("{\"event\":\"params\", \"data\":" + getParamsJson() + "}");
             ws.textAll("{\"event\":\"status\", \"data\":" + getSystemStatusJson() + "}");
             
             // Broadcast to Mesh Group (Now enabled!)
             meshManager.broadcastAnimationState(name, 0);
        } else if (strcmp(cmd, "reboot") == 0) {
             ESP.restart();
        } else if (strcmp(cmd, "setPower") == 0) {
             bool p = doc["value"];
             animManager.setPower(p);
             ws.textAll("{\"event\":\"status\", \"data\":" + getSystemStatusJson() + "}");
             
             // Broadcast to Mesh Group
             meshManager.broadcastSyncPower(p);
             
        } else if (strcmp(cmd, "checkOTA") == 0) {
             otaManager.forceCheck();
             meshManager.broadcastCheckForUpdates();
        } else if (strcmp(cmd, "setPhase") == 0 && doc.containsKey("value")) {
             float phase = doc["value"].as<float>();
             animManager.setDevicePhase(phase);
             // Echo back status to THIS client (and others connected to THIS device)
             // We use textAll because multiple tabs might be open to this Single device.
             ws.textAll("{\"event\":\"status\", \"data\":" + getSystemStatusJson() + "}");
        }

        // --- PRESET OPERATIONS ---
        else if (strcmp(cmd, "savePreset") == 0) {
            const char* name = doc["name"];
            const char* baseType = doc["baseType"];
            if (name && baseType) {
                if (animManager.savePreset(name, baseType)) {
                    // Success locally.
                    
                    // Mesh Broadcast
                    std::string path = "/presets/" + std::string(name) + ".json";
                    File f = LittleFS.open(path.c_str(), FILE_READ);
                    if (f) {
                        DynamicJsonDocument pDoc(2048); 
                        deserializeJson(pDoc, f);
                        f.close();
                        String paramsJson;
                        serializeJson(pDoc["params"], paramsJson);
                        meshManager.broadcastSavePreset(name, baseType, paramsJson.c_str());
                    }

                    // Broadcast updated list to UI
                    ws.textAll("{\"event\":\"animations\", \"data\":" + getAnimationsJson() + "}");
                }
            }
        }
        else if (strcmp(cmd, "renamePreset") == 0) {
            const char* oldName = doc["oldName"];
            const char* newName = doc["newName"];
            if (oldName && newName) {
                if (animManager.renamePreset(oldName, newName)) {
                    meshManager.broadcastRenamePreset(oldName, newName);
                    ws.textAll("{\"event\":\"animations\", \"data\":" + getAnimationsJson() + "}");
                }
            }
        }
        else if (strcmp(cmd, "deletePreset") == 0) {
            const char* name = doc["name"];
            if (name) {
                if (animManager.deletePreset(name)) {
                    meshManager.broadcastDeletePreset(name);
                    ws.textAll("{\"event\":\"animations\", \"data\":" + getAnimationsJson() + "}");
                }
            }
        }
        else if (strcmp(cmd, "assignGroup") == 0) {
             const char* idStr = doc["id"];
             const char* group = doc["group"];
             if (idStr && group) {
                 if (strcmp(idStr, "local") == 0) {
                     meshManager.setGroupName(group);
                 } else {
                     uint64_t targetId = strtoull(idStr, NULL, 16);
                     meshManager.broadcastAssignGroup(targetId, group);
                 }
                 // Wait a bit? Or just broadcast peers. Assigning group takes time to propagate.
                 // Let's just broadcast peers immediately, though the remote might not have updated yet.
             }
        }
    }
}

String WebManager::getSystemStatusJson() {
    StaticJsonDocument<512> doc;
    doc["uptime"] = millis();
    doc["heap"] = ESP.getFreeHeap();
    doc["animation"] = animManager.getCurrentAnimationName();
    doc["power"] = animManager.getPower();
    doc["ip"] = WiFi.localIP().toString();
    doc["version"] = otaManager.getVersion();
    doc["phase"] = animManager.getDevicePhase();
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
    self["group"] = meshManager.getGroupName();
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
        obj["group"] = peer.groupName;
        obj["lastSeen"] = peer.lastSeen;
        obj["self"] = false;
    }

    String output;
    serializeJson(doc, output);
    return output;
}
