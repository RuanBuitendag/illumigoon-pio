#include "system/MeshNetworkManager.h"
#include "animation/AnimationManager.h"
#include <Arduino.h>

// Static instance pointer for callback
MeshNetworkManager* MeshNetworkManager::instance = nullptr;

MeshNetworkManager::MeshNetworkManager(LedController& ledController)
    : ledController(ledController),
      myId(0),
      currentState(NodeState::STARTUP),
      masterId(0),
      lastHeartbeatTime(0),
      sequenceNumber(0),
      electionInProgress(false),
      timeOffset(0),
      smoothedOffset(0),
      hasSyncedOnce(false) {}

void MeshNetworkManager::begin() {
    Serial.println("=== Mesh Network Manager Starting ===");
    
    // Set static instance for callbacks
    instance = this;
    
    // Get MAC address as unique ID
    uint8_t mac[6];
    WiFi.macAddress(mac);
    myId = 0;
    for (int i = 0; i < 6; i++) {
        myId = (myId << 8) | mac[i];
    }
    
    Serial.print("My ID: ");
    Serial.println(String(myId, HEX));

    // Initialize ESP-NOW
    if (esp_now_init() != ESP_OK) {
        Serial.println("ESP-NOW init failed");
        return;
    }

    // Register receive callback
    esp_now_register_recv_cb(onReceiveWrapper);

    // Set up broadcast peer - use WiFi's channel
    esp_now_peer_info_t peerInfo = {};
    memset(&peerInfo, 0, sizeof(peerInfo));
    memcpy(peerInfo.peer_addr, broadcastAddress, 6);
    peerInfo.channel = WiFi.channel();  // Match WiFi channel
    peerInfo.encrypt = false;
    
    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
        Serial.println("Failed to add broadcast peer");
        return;
    }

    Serial.print("ESP-NOW initialized on channel ");
    Serial.println(WiFi.channel());

    currentState = NodeState::IDLE;
    lastHeartbeatTime = millis();
    
    Serial.println("Mesh network initialized, listening for master...");
}

void MeshNetworkManager::update() {
    unsigned long now = millis();

    switch (currentState) {
        case NodeState::STARTUP:
            // Should transition to IDLE in begin()
            break;

        case NodeState::IDLE:
            // No master detected, start election after timeout
            if (now - lastHeartbeatTime > 2000) {
                Serial.println("No master detected, starting election");
                startElection();
            }
            break;

        case NodeState::ELECTION:
            // Wait for OK responses or timeout
            if (now - lastElectionTime > 300) {
                if (!receivedOK) {
                    // No higher priority nodes, become master
                    becomeCoordinator();
                }
                // If we received OK, wait for coordinator announcement
                else if (now - lastElectionTime > 800) {
                    // Coordinator timeout, restart election
                    Serial.println("Coordinator timeout, restarting election");
                    startElection();
                }
            }
            break;

        case NodeState::MASTER:
            // Send heartbeat every 5 seconds
            if (now - lastHeartbeatTime > 5000) {
                sendHeartbeat();
                lastHeartbeatTime = now;
            }
            // Send time sync every 10 seconds (separate from heartbeat)
            static unsigned long lastTimeSyncTime = 0;
            if (now - lastTimeSyncTime > 10000) {
                sendTimeSync();
                lastTimeSyncTime = now;
            }
            break;

        case NodeState::SLAVE:
            // Check for master heartbeat timeout - 15s to allow for 5s heartbeat interval + OTA checks
            if (now - lastHeartbeatTime > 15000) {
                Serial.println("Master heartbeat timeout, starting election");
                startElection();
            }
            break;
    }
    // Periodically announce self to mesh (every 5 seconds)
    static unsigned long lastAnnouncement = 0;
    if (millis() - lastAnnouncement > 5000) {
        sendPeerAnnouncement();
        lastAnnouncement = millis();
    }
}

// New: Broadcast Animation State
void MeshNetworkManager::broadcastAnimationState(const char* name, uint32_t startTime) {
    if (currentState != NodeState::MASTER) return;

    MeshMessage msg;
    msg.type = MessageType::ANIMATION_STATE;
    msg.senderId = myId;
    msg.sequenceNumber = sequenceNumber++;
    msg.totalPackets = 1;
    msg.packetIndex = 0;
    msg.dataLength = sizeof(AnimationStatePayload);
    
    AnimationStatePayload payload;
    strncpy(payload.animationName, name, 31);
    payload.animationName[31] = '\0'; // Ensure null termination
    payload.startTime = startTime;
    
    memcpy(msg.data, &payload, sizeof(AnimationStatePayload));

    sendMessage(msg);
}

// New: Get synchronized network time
uint32_t MeshNetworkManager::getNetworkTime() const {
    return millis() + timeOffset;
}



bool MeshNetworkManager::isMaster() const {
    return currentState == NodeState::MASTER;
}

bool MeshNetworkManager::isSlave() const {
    return currentState == NodeState::SLAVE;
}

void MeshNetworkManager::prepareForOTA() {
    if (currentState == NodeState::MASTER) {
        Serial.println("Master preparing for OTA, sending shutdown message");
        MeshMessage msg;
        msg.type = MessageType::SHUTDOWN;
        msg.senderId = myId;
        msg.sequenceNumber = sequenceNumber++;
        msg.totalPackets = 1;
        msg.packetIndex = 0;
        msg.dataLength = 0;
        
        sendMessage(msg);
        delay(100); // Give time for message to send
    }
    currentState = NodeState::IDLE;
}

void MeshNetworkManager::onReceiveWrapper(const uint8_t* mac, const uint8_t* data, int len) {
    if (instance) {
        instance->onReceive(mac, data, len);
    }
}

void MeshNetworkManager::onReceive(const uint8_t* mac, const uint8_t* data, int len) {
    if (len != sizeof(MeshMessage)) return;

    MeshMessage msg;
    memcpy(&msg, data, sizeof(MeshMessage));

    // Ignore our own messages
    if (msg.senderId == myId) return;

    // Only log non-periodic messages to avoid Serial spam
    if (msg.type != MessageType::FRAME_DATA && msg.type != MessageType::HEARTBEAT && msg.type != MessageType::TIME_SYNC) {
        Serial.print("RX: ");
        switch(msg.type) {
            case MessageType::HEARTBEAT: Serial.print("HEARTBEAT"); break;
            case MessageType::ELECTION: Serial.print("ELECTION"); break;
            case MessageType::OK: Serial.print("OK"); break;
            case MessageType::COORDINATOR: Serial.print("COORDINATOR"); break;
            case MessageType::SHUTDOWN: Serial.print("SHUTDOWN"); break;
            case MessageType::ANIMATION_STATE: Serial.print("ANIMATION_STATE"); break;
            case MessageType::PEER_ANNOUNCEMENT: Serial.print("PEER_ANNOUNCEMENT"); break;
            case MessageType::RENAME_PRESET: Serial.print("RENAME_PRESET"); break;
            default: Serial.print("UNKNOWN"); break;
        }
        Serial.print(" from ");
        Serial.println(String(msg.senderId, HEX));
    }

    switch (msg.type) {
        case MessageType::HEARTBEAT:
            handleHeartbeat(msg);
            break;

        case MessageType::ELECTION:
            handleElection(msg);
            break;

        case MessageType::OK:
            handleOK(msg);
            break;

        case MessageType::COORDINATOR:
            handleCoordinator(msg);
            break;

        case MessageType::FRAME_DATA:
            handleFrameData(msg);
            break;

        case MessageType::SHUTDOWN:
            handleShutdown(msg);
            break;
        
        case MessageType::TIME_SYNC:
            handleTimeSync(msg);
            break;

        case MessageType::ANIMATION_STATE:
            handleAnimationState(msg);
            break;

        case MessageType::PEER_ANNOUNCEMENT:
            handlePeerAnnouncement(msg);
            break;

        case MessageType::QUERY_PRESET:
            handleQueryPreset(msg);
            break;

        case MessageType::PRESET_EXIST_RESPONSE:
            handlePresetExistResponse(msg);
            break;

        case MessageType::SAVE_PRESET:
            handleSavePreset(msg);
            break;

        case MessageType::DELETE_PRESET:
            handleDeletePreset(msg);
            break;

        case MessageType::CHECK_FOR_UPDATES:
            handleCheckForUpdates(msg);
            break;

        case MessageType::RENAME_PRESET:
            handleRenamePreset(msg);
            break;

        default:
            break;
    }
}

void MeshNetworkManager::sendTimeSync() {
    MeshMessage msg;
    msg.type = MessageType::TIME_SYNC;
    msg.senderId = myId;
    msg.sequenceNumber = sequenceNumber++;
    msg.totalPackets = 1;
    msg.packetIndex = 0;
    msg.dataLength = sizeof(uint32_t);
    
    uint32_t now = millis();
    memcpy(msg.data, &now, sizeof(uint32_t));
    
    Serial.printf("[TimeSync] Sending sync. Time: %lu\n", now);
    sendMessage(msg);
}

void MeshNetworkManager::handleTimeSync(const MeshMessage& msg) {
    if (msg.senderId == masterId) {
        uint32_t masterTime;
        memcpy(&masterTime, msg.data, sizeof(uint32_t));
        
        // Add estimated latency
        const uint32_t LATENCY_MS = 15;
        masterTime += LATENCY_MS;

        uint32_t localTime = millis();
        int32_t instantaneousOffset = (int32_t)(masterTime - localTime);
        
        // First sync or large jump protection (>500ms)
        if (!hasSyncedOnce || abs(instantaneousOffset - (int32_t)smoothedOffset) > 500) {
            smoothedOffset = instantaneousOffset;
            timeOffset = instantaneousOffset;
            hasSyncedOnce = true;
            Serial.printf("[TimeSync] Hard sync. Master: %lu, Local: %lu, Offset: %ld\n", masterTime, localTime, timeOffset);
        } else {
            // Exponential Smoothing (Alpha = 0.2)
            // New = Alpha * Instant + (1 - Alpha) * Old
            smoothedOffset = (0.2 * instantaneousOffset) + (0.8 * smoothedOffset);
            timeOffset = (int32_t)smoothedOffset;
             // Only log occasionally to reduce noise, or log debug
            // Serial.printf("[TimeSync] Smooth sync. Offset: %ld (Raw: %ld)\n", timeOffset, instantaneousOffset);
        }
    } else {
        Serial.printf("[TimeSync] Ignored sync from non-master %llX (current master: %llX)\n", msg.senderId, masterId);
    }
}

void MeshNetworkManager::handleAnimationState(const MeshMessage& msg) {
    if (msg.senderId == masterId) {
        if (msg.dataLength == sizeof(AnimationStatePayload)) {
            AnimationStatePayload payload;
            memcpy(&payload, msg.data, sizeof(AnimationStatePayload));
            

        }
    }
}

void MeshNetworkManager::handleHeartbeat(const MeshMessage& msg) {
    if (currentState == NodeState::IDLE || currentState == NodeState::SLAVE) {
        // Accept this as master
        if (masterId == 0 || masterId == msg.senderId) {
            masterId = msg.senderId;
            lastHeartbeatTime = millis();
            
            if (currentState == NodeState::IDLE) {
                Serial.print("Master detected: ");
                Serial.println(String(masterId, HEX));
                currentState = NodeState::SLAVE;
            } else {
                 // Serial.printf("[Heartbeat] Received from master %llX\n", msg.senderId);
            }
        }
        // Split brain: higher ID wins
        else if (msg.senderId > masterId) {
            Serial.println("Higher priority master detected, switching");
            masterId = msg.senderId;
            lastHeartbeatTime = millis();
            currentState = NodeState::SLAVE;
        }
    }
    else if (currentState == NodeState::MASTER) {
        // Another master? Higher ID wins
        if (msg.senderId > myId) {
            Serial.println("Higher priority master detected, becoming slave");
            masterId = msg.senderId;
            lastHeartbeatTime = millis();
            currentState = NodeState::SLAVE;
        }
    }
}

void MeshNetworkManager::handleElection(const MeshMessage& msg) {
    if (msg.senderId < myId) {
        // Send OK, we have higher priority
        Serial.println("Sending OK (higher priority)");
        MeshMessage response;
        response.type = MessageType::OK;
        response.senderId = myId;
        response.sequenceNumber = sequenceNumber++;
        response.totalPackets = 1;
        response.packetIndex = 0;
        response.dataLength = 0;
        
        sendMessage(response);

        // Start our own election if not already in progress
        if (currentState != NodeState::ELECTION && currentState != NodeState::MASTER) {
            startElection();
        }
    }
}

void MeshNetworkManager::handleOK(const MeshMessage& msg) {
    if (currentState == NodeState::ELECTION) {
        Serial.println("Received OK, waiting for coordinator");
        receivedOK = true;
    }
}

void MeshNetworkManager::handleCoordinator(const MeshMessage& msg) {
    if (msg.senderId >= myId || currentState == NodeState::ELECTION) {
        Serial.print("New coordinator: ");
        Serial.println(String(msg.senderId, HEX));
        masterId = msg.senderId;
        lastHeartbeatTime = millis();
        currentState = NodeState::SLAVE;
        electionInProgress = false;
    }
}

void MeshNetworkManager::handleFrameData(const MeshMessage& msg) {
    if (currentState != NodeState::SLAVE) return;
    if (msg.senderId != masterId) return;

    unsigned long now = millis();

    // New frame started
    if (frameBuffer.sequenceNumber != msg.sequenceNumber) {
        frameBuffer.sequenceNumber = msg.sequenceNumber;
        frameBuffer.totalPackets = msg.totalPackets;
        frameBuffer.receivedPackets = 0;
        frameBuffer.lastPacketTime = now;
        
        if (frameBuffer.leds == nullptr) {
            frameBuffer.leds = new CRGB[ledController.getNumLeds()];
        }
    }

    // Check timeout (frame incomplete for 100ms)
    if (now - frameBuffer.lastPacketTime > 100) {
        frameBuffer.sequenceNumber = msg.sequenceNumber;
        frameBuffer.totalPackets = msg.totalPackets;
        frameBuffer.receivedPackets = 0;
    }

    // Copy packet data
    int offset = msg.packetIndex * 230;
    memcpy(((uint8_t*)frameBuffer.leds) + offset, msg.data, msg.dataLength);
    frameBuffer.receivedPackets++;
    frameBuffer.lastPacketTime = now;

    // All packets received, render frame
    if (frameBuffer.receivedPackets >= frameBuffer.totalPackets && !ledController.isOtaInProgress()) {
        CRGB* leds = ledController.getLeds();
        memcpy(leds, frameBuffer.leds, ledController.getNumLeds() * sizeof(CRGB));
        ledController.render();
    }
}

void MeshNetworkManager::handleShutdown(const MeshMessage& msg) {
    if (msg.senderId == masterId) {
        Serial.println("Master shutting down, starting election");
        startElection();
    }
}

void MeshNetworkManager::startElection() {
    Serial.println("Starting election");
    currentState = NodeState::ELECTION;
    lastElectionTime = millis();
    receivedOK = false;
    electionInProgress = true;

    // Add small random delay to avoid collisions
    delay(random(10, 50));

    MeshMessage msg;
    msg.type = MessageType::ELECTION;
    msg.senderId = myId;
    msg.sequenceNumber = sequenceNumber++;
    msg.totalPackets = 1;
    msg.packetIndex = 0;
    msg.dataLength = 0;

    sendMessage(msg);
}

void MeshNetworkManager::becomeCoordinator() {
    Serial.println("=== Becoming Master ===");
    currentState = NodeState::MASTER;
    masterId = myId;
    lastHeartbeatTime = millis();
    electionInProgress = false;

    // Announce coordinator
    MeshMessage msg;
    msg.type = MessageType::COORDINATOR;
    msg.senderId = myId;
    msg.sequenceNumber = sequenceNumber++;
    msg.totalPackets = 1;
    msg.packetIndex = 0;
    msg.dataLength = 0;

    sendMessage(msg);
}

void MeshNetworkManager::sendHeartbeat() {
    MeshMessage msg;
    msg.type = MessageType::HEARTBEAT;
    msg.senderId = myId;
    msg.sequenceNumber = sequenceNumber++;
    msg.totalPackets = 1;
    msg.packetIndex = 0;
    msg.dataLength = 0;

    sendMessage(msg);
}

void MeshNetworkManager::sendMessage(const MeshMessage& msg) {
    esp_err_t result = esp_now_send(broadcastAddress, (uint8_t*)&msg, sizeof(MeshMessage));
    if (result != ESP_OK) {
        Serial.print("Send failed: ");
        Serial.println(result);
    }
}

void MeshNetworkManager::sendPeerAnnouncement() {
    MeshMessage msg;
    msg.type = MessageType::PEER_ANNOUNCEMENT;
    msg.senderId = myId;
    msg.sequenceNumber = sequenceNumber++;
    
    PeerAnnouncementPayload payload;
    payload.ip = (uint32_t)WiFi.localIP();
    payload.role = currentState;

    memcpy(msg.data, &payload, sizeof(PeerAnnouncementPayload));
    msg.dataLength = sizeof(PeerAnnouncementPayload);
    
    sendMessage(msg);
}

void MeshNetworkManager::handlePeerAnnouncement(const MeshMessage& msg) {
    if (msg.dataLength < sizeof(PeerAnnouncementPayload)) return;
    
    PeerAnnouncementPayload* payload = (PeerAnnouncementPayload*)msg.data;
    
    // Update or Add peer
    bool found = false;
    for (auto& peer : knownPeers) {
        if (peer.id == msg.senderId) {
            peer.ip = payload->ip;
            peer.role = payload->role;
            peer.lastSeen = millis();
            found = true;
            break;
        }
    }
    
    if (!found) {
        knownPeers.push_back({msg.senderId, payload->ip, payload->role, millis()});
        Serial.printf("New Peer Discovered: %016llX at IP %u\n", msg.senderId, payload->ip);
    }
}

// ==========================================
// PRESET PROPAGATION IMPLEMENTATION
// ==========================================

bool MeshNetworkManager::checkPresetExists(const std::string& name) {
    if (!animManager) return false;
    
    // 1. Check local first
    if (animManager->exists(name)) {
        return true;
    }
    
    // 2. Query Network
    lastQueryFound = false;
    lastQueryName = name;
    
    MeshMessage msg;
    msg.type = MessageType::QUERY_PRESET;
    msg.senderId = myId;
    msg.sequenceNumber = sequenceNumber++;
    msg.totalPackets = 1;
    msg.packetIndex = 0;
    
    // Payload is just the name
    strncpy((char*)msg.data, name.c_str(), 229);
    msg.data[229] = '\0'; 
    msg.dataLength = strlen((char*)msg.data) + 1;
    
    sendMessage(msg);
    
    // 3. Wait for response (blocking, up to 500ms)
    unsigned long start = millis();
    while (millis() - start < 500) {
        // Yield to allow background tasks (esp-now callbacks) to run? 
        // On ESP32 Arduino, callbacks happen in ISR or separate task, but update() runs in loop.
        // We are blocking the loop here.
        // If receive happens in ISR/task, 'lastQueryFound' will be set.
        delay(10); 
        if (lastQueryFound) return true;
    }
    
    return false;
}

void MeshNetworkManager::broadcastSavePreset(const std::string& name, const std::string& baseType, const std::string& paramsJson) {
    // We need to send: UpdateType (Save), Name, BaseType, JSON Data.
    // Format payload: Name\0BaseType\0JSONData
    // We'll construct a large buffer and then fragment it.
    
    std::vector<uint8_t> totalPayload;
    totalPayload.insert(totalPayload.end(), name.begin(), name.end());
    totalPayload.push_back('\0');
    totalPayload.insert(totalPayload.end(), baseType.begin(), baseType.end());
    totalPayload.push_back('\0');
    totalPayload.insert(totalPayload.end(), paramsJson.begin(), paramsJson.end());
    
    // Max data per packet: 230 bytes
    size_t totalLen = totalPayload.size();
    uint8_t totalPackets = (totalLen + 229) / 230;
    
    uint32_t seq = sequenceNumber++; // Same sequence for all fragments
    
    for (int i = 0; i < totalPackets; i++) {
        MeshMessage msg;
        msg.type = MessageType::SAVE_PRESET;
        msg.senderId = myId;
        msg.sequenceNumber = seq;
        msg.totalPackets = totalPackets;
        msg.packetIndex = i;
        
        size_t offset = i * 230;
        size_t chunkLen = totalLen - offset;
        if (chunkLen > 230) chunkLen = 230;
        
        memcpy(msg.data, totalPayload.data() + offset, chunkLen);
        msg.dataLength = chunkLen;
        
        sendMessage(msg);
        delay(10); // Small delay to prevent queue flooding
    }
}

void MeshNetworkManager::broadcastDeletePreset(const std::string& name) {
    MeshMessage msg;
    msg.type = MessageType::DELETE_PRESET;
    msg.senderId = myId;
    msg.sequenceNumber = sequenceNumber++;
    msg.totalPackets = 1;
    msg.packetIndex = 0;
    
    strncpy((char*)msg.data, name.c_str(), 229);
    sendMessage(msg);
}

void MeshNetworkManager::broadcastRenamePreset(const std::string& oldName, const std::string& newName) {
    MeshMessage msg;
    msg.type = MessageType::RENAME_PRESET;
    msg.senderId = myId;
    msg.sequenceNumber = sequenceNumber++;
    msg.totalPackets = 1;
    msg.packetIndex = 0;
    
    // Payload: OldName\0NewName\0
    std::string payload = oldName;
    payload += '\0';
    payload += newName;
    payload += '\0';
    
    if (payload.length() > 230) {
        Serial.println("Mesh: Rename payload too large!");
        return;
    }
    
    memcpy(msg.data, payload.c_str(), payload.length());
    msg.dataLength = payload.length();
    
    sendMessage(msg);
}

void MeshNetworkManager::handleQueryPreset(const MeshMessage& msg) {
    if (!animManager) return;
    
    char name[230];
    memcpy(name, msg.data, msg.dataLength);
    name[229] = '\0'; // Safety
    
    if (animManager->exists(name)) {
        // Send Response
        MeshMessage response;
        response.type = MessageType::PRESET_EXIST_RESPONSE;
        response.senderId = myId;
        response.sequenceNumber = sequenceNumber++;
        response.totalPackets = 1;
        response.packetIndex = 0;
         
        // Return name so sender knows WHICH query this answers
        memcpy(response.data, name, strlen(name) + 1);
        response.dataLength = strlen(name) + 1;
        
        sendMessage(response); 
    }
}

void MeshNetworkManager::handlePresetExistResponse(const MeshMessage& msg) {
    char name[230];
    memcpy(name, msg.data, msg.dataLength);
    name[229] = '\0';
    
    if (lastQueryName == name) {
        lastQueryFound = true;
    }
}

void MeshNetworkManager::handleSavePreset(const MeshMessage& msg) {
    if (!animManager) return;
    
    unsigned long now = millis();
    
    if (presetBuffer.sequenceNumber != msg.sequenceNumber) {
        // New transfer
        presetBuffer.sequenceNumber = msg.sequenceNumber;
        presetBuffer.totalPackets = msg.totalPackets;
        presetBuffer.receivedPackets = 0;
        presetBuffer.data.clear();
        presetBuffer.lastPacketTime = now;
        
        // Reserve estimated size
        presetBuffer.data.resize(msg.totalPackets * 230);
    }
    
    // Packet Handling
    if (msg.packetIndex < presetBuffer.totalPackets) {
         size_t offset = msg.packetIndex * 230;
         if (offset + msg.dataLength <= presetBuffer.data.size()) {
             memcpy(presetBuffer.data.data() + offset, msg.data, msg.dataLength);
             presetBuffer.receivedPackets++;
             presetBuffer.lastPacketTime = now;
         }
    }
    
    // Check completion
    if (presetBuffer.receivedPackets >= presetBuffer.totalPackets) {
        // Unpack: Name\0BaseType\0JSON
        const char* raw = (const char*)presetBuffer.data.data();
        size_t totalSize = presetBuffer.data.size(); // Actually we might have resized too big, use explicit search?
        // Wait, resize was max possible. 
        // We need to trust null terminators.
        
        std::string pName = raw;
        size_t nameLen = pName.length() + 1;
        
        if (nameLen < totalSize) {
            std::string pBase = raw + nameLen;
            size_t baseLen = pBase.length() + 1;
            
            if (nameLen + baseLen < totalSize) {
                std::string pJson = raw + nameLen + baseLen;
                
                // Save it!
                Serial.printf("Mesh: Saving preset '%s' (%s)\n", pName.c_str(), pBase.c_str());
                animManager->savePresetFromData(pName, pBase, pJson);
            }
        }
        
        // Reset
        presetBuffer.sequenceNumber = 0; 
    }
}

void MeshNetworkManager::handleDeletePreset(const MeshMessage& msg) {
    if (!animManager) return;
    
    char name[230];
    memcpy(name, msg.data, msg.dataLength);
    name[229] = '\0';
    
    animManager->deletePreset(name);
}

void MeshNetworkManager::handleRenamePreset(const MeshMessage& msg) {
    if (!animManager) return;
    
    const char* raw = (const char*)msg.data;
    size_t len = msg.dataLength;
    
    // Safe extraction
    // Ensure null termination within bounds
    // Format: OldName\0NewName\0
    
    // Validation
    bool valid = false;
    for (size_t i = 0; i < len; i++) {
        if (msg.data[i] == '\0') {
             // Found first null terminator.
             // Check if there is another one after it.
             for(size_t j = i + 1; j < len; j++) {
                 if (msg.data[j] == '\0') {
                     valid = true;
                     break;
                 }
             }
             break;
        }
    }
    
    if (valid) {
        std::string oldName = raw;
        std::string newName = raw + oldName.length() + 1;
        
        Serial.printf("Mesh: Renaming preset from '%s' to '%s'\n", oldName.c_str(), newName.c_str());
        animManager->renamePreset(oldName, newName);
    } else {
        Serial.println("Mesh: Invalid Rename Payload");
    }
}

void MeshNetworkManager::broadcastCheckForUpdates() {
    MeshMessage msg;
    msg.type = MessageType::CHECK_FOR_UPDATES;
    msg.senderId = myId;
    msg.sequenceNumber = sequenceNumber++;
    msg.totalPackets = 1;
    msg.packetIndex = 0;
    msg.dataLength = 0;
    
    Serial.println("Mesh: Broadcasting check for updates");
    sendMessage(msg);
}

void MeshNetworkManager::handleCheckForUpdates(const MeshMessage& msg) {
    if (msg.senderId == myId) return; // Should be filtered already but good safety
    
    Serial.println("Mesh: Received check for updates request");
    if (otaCallback) {
        otaCallback();
    }
}
