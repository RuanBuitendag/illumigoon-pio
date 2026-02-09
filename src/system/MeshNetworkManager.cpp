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
      hasSyncedOnce(false),
      myGroupName("") {}

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
    peerInfo.channel = 0;  // Use current channel (dictated by WiFi)
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
    
    // Request sync on startup (after a short delay to allow connections to stabilize)
    // We can't delay here, so just send it.
    broadcastRequestSyncPresets();
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

    // Handle pending group assignment broadcast (from main loop)
    if (pendingGroupAssignment.pending) {
        doSendAssignGroup(pendingGroupAssignment.targetId, pendingGroupAssignment.groupName);
        pendingGroupAssignment.pending = false;
    }

    // Handle pending param sync
    if (pendingParamSync.pending) {
        doSendSyncParam(pendingParamSync.paramName, pendingParamSync.jsonValue);
        pendingParamSync.pending = false;
    }

    // Handle pending power sync
    if (pendingPowerSync.pending) {
        doSendSyncPower(pendingPowerSync.powerOn);
        pendingPowerSync.pending = false;
    }


    // Periodic Sync Request (every 60 seconds)
    static unsigned long lastSyncRequest = 0;
    if (now - lastSyncRequest > 10000) {
        broadcastRequestSyncPresets();
        lastSyncRequest = now;
    }
    
    // Process manifest queue (non-blocking, one per cycle)
    if (manifestQueue.active && !manifestQueue.names.empty() && now >= manifestQueue.nextSendTime) {
        std::string name = manifestQueue.names.back();
        manifestQueue.names.pop_back();
        
        MeshMessage response;
        response.type = MessageType::PRESET_MANIFEST;
        response.senderId = myId;
        response.sequenceNumber = sequenceNumber++;
        response.totalPackets = 1;
        response.packetIndex = 0;
        
        strncpy((char*)response.data, name.c_str(), 229);
        response.data[229] = '\0';
        response.dataLength = strlen((char*)response.data) + 1;
        
        sendMessage(response);
        
        // Schedule next send with 100ms spacing
        manifestQueue.nextSendTime = now + 100;
        
        if (manifestQueue.names.empty()) {
            manifestQueue.active = false;
            Serial.println("Mesh: Manifest queue complete");
        }
    }
    
    // Process data request queue (non-blocking, one per cycle)
    if (!dataRequestQueue.requests.empty() && now >= dataRequestQueue.nextSendTime) {
        auto req = dataRequestQueue.requests.back();
        dataRequestQueue.requests.pop_back();
        
        MeshMessage msg;
        msg.type = MessageType::REQUEST_PRESET_DATA;
        msg.senderId = myId;
        msg.sequenceNumber = sequenceNumber++;
        msg.totalPackets = 1;
        msg.packetIndex = 0;
        
        memcpy(msg.data, &req.targetId, sizeof(uint64_t));
        strncpy((char*)msg.data + sizeof(uint64_t), req.name.c_str(), 229 - sizeof(uint64_t));
        ((char*)msg.data)[229] = '\0';
        msg.dataLength = sizeof(uint64_t) + strlen(req.name.c_str()) + 1;
        
        sendMessage(msg);
        Serial.printf("Mesh: Sent data request for '%s'\r\n", req.name.c_str());
        
        // Schedule next request with 500ms spacing (allow time for response)
        dataRequestQueue.nextSendTime = now + 500;
    }
}

// New: Broadcast Animation State
void MeshNetworkManager::broadcastAnimationState(const char* name, uint32_t startTime) {
    // if (currentState != NodeState::MASTER) return; // Allow anyone to broadcast in a group

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
    strncpy(payload.groupName, myGroupName.c_str(), 31);
    payload.groupName[31] = '\0';
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
    // Debug: Log ALL incoming ESP-NOW packets
    Serial.printf("ESP-NOW RX: len=%d, expected=%d\r\n", len, sizeof(MeshMessage));
    
    // if (len != sizeof(MeshMessage)) {
    //     Serial.printf("ESP-NOW: Dropping packet, wrong size: got %d, expected %d\r\n", len, sizeof(MeshMessage));
    //     return;
    // }

    MeshMessage msg;
    memcpy(&msg, data, sizeof(MeshMessage));

    // Ignore our own messages
    if (msg.senderId == myId) return;

    // Only log non-periodic messages to avoid Serial spam
    // if (msg.type != MessageType::HEARTBEAT && msg.type != MessageType::TIME_SYNC) {
    if (true){
        Serial.print("RX: ");
        switch(msg.type) {
            case MessageType::ELECTION: Serial.print("ELECTION"); break;
            case MessageType::OK: Serial.print("OK"); break;
            case MessageType::COORDINATOR: Serial.print("COORDINATOR"); break;
            case MessageType::SHUTDOWN: Serial.print("SHUTDOWN"); break;
            case MessageType::ANIMATION_STATE: Serial.print("ANIMATION_STATE"); break;
            case MessageType::PEER_ANNOUNCEMENT: Serial.print("PEER_ANNOUNCEMENT"); break;
            case MessageType::RENAME_PRESET: Serial.print("RENAME_PRESET"); break;
            case MessageType::SAVE_PRESET: Serial.print("SAVE_PRESET"); break;
            case MessageType::DELETE_PRESET: Serial.print("DELETE_PRESET"); break;
            case MessageType::HEARTBEAT: Serial.print("HEARTBEAT"); break;
            case MessageType::TIME_SYNC: Serial.print("TIME_SYNC"); break;
            case MessageType::ASSIGN_GROUP: Serial.print("ASSIGN_GROUP"); break;
            case MessageType::SYNC_PARAM: Serial.print("SYNC_PARAM"); break;
            case MessageType::SYNC_POWER: Serial.print("SYNC_POWER"); break;
            case MessageType::REQUEST_SYNC_PRESETS: Serial.print("REQUEST_SYNC_PRESETS"); break;
            case MessageType::PRESET_MANIFEST: Serial.print("PRESET_MANIFEST"); break;
            case MessageType::REQUEST_PRESET_DATA: Serial.print("REQUEST_PRESET_DATA"); break;
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

        case MessageType::ASSIGN_GROUP:
            handleAssignGroup(msg);
            break;

        case MessageType::SYNC_PARAM:
            handleSyncParam(msg);
            break;

        case MessageType::SYNC_POWER:
            handleSyncPower(msg);
            break;

        case MessageType::REQUEST_SYNC_PRESETS:
            handleRequestSyncPresets(msg);
            break;

        case MessageType::PRESET_MANIFEST:
            handlePresetManifest(msg);
            break;

        case MessageType::REQUEST_PRESET_DATA:
            handleRequestPresetData(msg);
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
    
    Serial.printf("[TimeSync] Sending sync. Time: %lu\r\n", now);
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
            Serial.printf("[TimeSync] Hard sync. Master: %lu, Local: %lu, Offset: %ld\r\n", masterTime, localTime, timeOffset);
        } else {
            // Exponential Smoothing (Alpha = 0.2)
            // New = Alpha * Instant + (1 - Alpha) * Old
            smoothedOffset = (0.2 * instantaneousOffset) + (0.8 * smoothedOffset);
            timeOffset = (int32_t)smoothedOffset;
             // Only log occasionally to reduce noise, or log debug
            // Serial.printf("[TimeSync] Smooth sync. Offset: %ld (Raw: %ld)\r\n", timeOffset, instantaneousOffset);
        }
    } else {
        Serial.printf("[TimeSync] Ignored sync from non-master %llX (current master: %llX)\r\n", msg.senderId, masterId);
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
                 // Serial.printf("[Heartbeat] Received from master %llX\r\n", msg.senderId);
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
    strncpy(payload.groupName, myGroupName.c_str(), 31);
    payload.groupName[31] = '\0';

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
            peer.groupName = payload->groupName;
            peer.lastSeen = millis();
            found = true;
            break;
        }
    }
    
    if (!found) {
        knownPeers.push_back({msg.senderId, payload->ip, payload->role, payload->groupName, millis()});
        Serial.printf("New Peer Discovered: %016llX at IP %u, Group: %s\r\n", msg.senderId, payload->ip, payload->groupName);
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
    Serial.printf("Mesh: Broadcasting save preset '%s' (base: %s), JSON len: %d\r\n", name.c_str(), baseType.c_str(), paramsJson.length());
    
    // Format payload: Name\0BaseType\0JSONData
    std::vector<uint8_t> totalPayload;
    totalPayload.insert(totalPayload.end(), name.begin(), name.end());
    totalPayload.push_back('\0');
    totalPayload.insert(totalPayload.end(), baseType.begin(), baseType.end());
    totalPayload.push_back('\0');
    totalPayload.insert(totalPayload.end(), paramsJson.begin(), paramsJson.end());
    
    size_t totalLen = totalPayload.size();
    uint8_t totalPackets = (totalLen + 229) / 230;
    uint32_t seq = sequenceNumber++;
    
    // Repeat the broadcast 3 times for redundancy
    for (int r = 0; r < 3; r++) {
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
            // Slightly longer delay between packets to prevent congestion
            delay(20); 
        }
        Serial.printf("Mesh: Sent preset broadcast round %d/3\r\n", r + 1);
        delay(50); // Delay between rounds
    }
    Serial.println("Mesh: Preset broadcast complete");
}

void MeshNetworkManager::broadcastDeletePreset(const std::string& name) {
    MeshMessage msg;
    msg.type = MessageType::DELETE_PRESET;
    msg.senderId = myId;
    msg.sequenceNumber = sequenceNumber++;
    msg.totalPackets = 1;
    msg.packetIndex = 0;
    msg.dataLength = name.length() + 1;
    
    strncpy((char*)msg.data, name.c_str(), 229);
    sendMessage(msg);
    Serial.printf("Mesh: Delete preset '%s' broadcast complete\r\n", name.c_str());
}

void MeshNetworkManager::broadcastRenamePreset(const std::string& oldName, const std::string& newName) {
    MeshMessage msg;
    msg.type = MessageType::RENAME_PRESET;
    msg.senderId = myId;
    msg.sequenceNumber = sequenceNumber++;
    msg.totalPackets = 1;
    msg.packetIndex = 0;
    
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
    Serial.printf("Mesh: Rename preset broadcast complete\r\n");
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
    Serial.printf("Mesh: handleSavePreset called, packet %d/%d, seq %u\r\n", msg.packetIndex + 1, msg.totalPackets, msg.sequenceNumber);
    
    if (!animManager) {
        Serial.println("Mesh: handleSavePreset - animManager is NULL!");
        return;
    }
    
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
        
        // Resize flags and reset
        presetBuffer.receivedPacketFlags.resize(msg.totalPackets);
        std::fill(presetBuffer.receivedPacketFlags.begin(), presetBuffer.receivedPacketFlags.end(), false);
    }
    
    // Packet Handling
    if (msg.packetIndex < presetBuffer.totalPackets) {
         // Check if we already have this packet
         if (presetBuffer.receivedPacketFlags.size() > msg.packetIndex && presetBuffer.receivedPacketFlags[msg.packetIndex]) {
             // Duplicate, ignore
             return;
         }

         size_t offset = msg.packetIndex * 230;
         if (offset + msg.dataLength <= presetBuffer.data.size()) {
             memcpy(presetBuffer.data.data() + offset, msg.data, msg.dataLength);
             presetBuffer.receivedPackets++;
             
             // Mark as received
             if (presetBuffer.receivedPacketFlags.size() > msg.packetIndex) {
                 presetBuffer.receivedPacketFlags[msg.packetIndex] = true;
             }
             
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
                Serial.printf("Mesh: Saving preset '%s' (%s)\r\n", pName.c_str(), pBase.c_str());
                if (animManager->savePresetFromData(pName, pBase, pJson)) {
                    // Note: Don't call flashColor here - async callback context, blocking is bad
                    Serial.println("Mesh: Preset saved successfully!");
                } else {
                    Serial.println("Mesh: Preset save FAILED!");
                }
            }
        }
        
        // Reset

        // Important: If we just saved a preset that we requested, we might want to reload or notify?
        // simple loadPresets() is called inside savePresetFromData.
        
        presetBuffer.sequenceNumber = 0; 
    }
}

// ==========================================
// SYNC LOGIC
// ==========================================

void MeshNetworkManager::broadcastRequestSyncPresets() {
    MeshMessage msg;
    msg.type = MessageType::REQUEST_SYNC_PRESETS;
    msg.senderId = myId;
    msg.sequenceNumber = sequenceNumber++;
    msg.totalPackets = 1;
    msg.packetIndex = 0;
    msg.dataLength = 0;
    
    sendMessage(msg);
    Serial.println("Mesh: Requested Preset Sync");
}

void MeshNetworkManager::handleRequestSyncPresets(const MeshMessage& msg) {
    if (!animManager) return;
    
    // Queue all preset names for sending (non-blocking)
    std::vector<std::string> names = animManager->getPresetNames();
    
    if (names.empty()) return;
    
    Serial.printf("Mesh: Queueing %d presets for manifest broadcast\r\n", names.size());
    
    // Add to queue
    for (const auto& name : names) {
        manifestQueue.names.push_back(name);
    }
    
    // Schedule first send with random delay to avoid collision
    unsigned long delayTime = random(100, 800);
    manifestQueue.nextSendTime = millis() + delayTime;
    manifestQueue.active = true;
}

void MeshNetworkManager::handlePresetManifest(const MeshMessage& msg) {
    if (!animManager) return;
    
    char name[230];
    memcpy(name, msg.data, msg.dataLength);
    name[229] = '\0';
    
    // Check if we have this preset
    if (!animManager->exists(name)) {
        // Check duplication
        unsigned long now = millis();
        bool alreadyRequested = false;
        
        // Clean up old requests and check for duplicates
        for (auto it = requestedPresets.begin(); it != requestedPresets.end(); ) {
            if (now - it->requestTime > 30000) {
                it = requestedPresets.erase(it); // expire old (30s timeout)
            } else {
                if (it->name == name) {
                    alreadyRequested = true;
                }
                ++it;
            }
        }
        
        // Also check if already in the request queue
        for (const auto& req : dataRequestQueue.requests) {
            if (req.name == name) {
                alreadyRequested = true;
                break;
            }
        }

        if (!alreadyRequested) {
            Serial.printf("Mesh: Missing preset '%s', queuing request to %016llX\r\n", name, msg.senderId);
            requestedPresets.push_back({name, now});
            
            // Queue the request instead of sending immediately
            dataRequestQueue.requests.push_back({name, msg.senderId});
        }
    }
}

void MeshNetworkManager::broadcastRequestPresetData(const std::string& name, uint64_t targetId) {
    // This is now just a helper to queue a request
    dataRequestQueue.requests.push_back({name, targetId});
}

void MeshNetworkManager::handleRequestPresetData(const MeshMessage& msg) {
    if (!animManager) return;
    
    if (msg.dataLength < sizeof(uint64_t)) return;
    
    uint64_t targetId;
    memcpy(&targetId, msg.data, sizeof(uint64_t));
    
    if (targetId != myId) {
        // Not for us
        return;
    }

    char name[230];
    strncpy(name, (char*)msg.data + sizeof(uint64_t), 229 - sizeof(uint64_t));
    name[229] = '\0';
    
    Serial.printf("Mesh: Request for preset data '%s' received (Directed)\r\n", name);
    
    std::string baseType;
    std::string paramsJson;
    
    if (animManager->getPresetData(name, baseType, paramsJson)) {
        broadcastSavePreset(name, baseType, paramsJson);
    } else {
        Serial.printf("Mesh: Requested preset '%s' not found locally!\r\n", name);
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
        
        Serial.printf("Mesh: Renaming preset from '%s' to '%s'\r\n", oldName.c_str(), newName.c_str());
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

// ==========================================
// GROUP MANAGEMENT IMPLEMENTATION
// ==========================================

void MeshNetworkManager::setGroupName(const std::string& name) {
    if (myGroupName != name) {
        Serial.printf("Mesh: Group name changed from '%s' to '%s'\r\n", myGroupName.c_str(), name.c_str());
        myGroupName = name;
        // Trigger announcement immediately so others know
        sendPeerAnnouncement();
    }
}

void MeshNetworkManager::broadcastAssignGroup(uint64_t targetId, const char* newGroupName) {
    Serial.printf("Mesh: Queuing ASSIGN_GROUP for %016llX -> '%s'\r\n", targetId, newGroupName);
    pendingGroupAssignment.targetId = targetId;
    pendingGroupAssignment.groupName = newGroupName;
    pendingGroupAssignment.pending = true;
}

void MeshNetworkManager::doSendAssignGroup(uint64_t targetId, const std::string& groupName) {
    MeshMessage msg;
    msg.type = MessageType::ASSIGN_GROUP;
    msg.senderId = myId;
    msg.sequenceNumber = sequenceNumber++;
    msg.totalPackets = 1;
    msg.packetIndex = 0;
    
    // Payload: TargetID (8) + GroupName (N)
    memcpy(msg.data, &targetId, sizeof(uint64_t));
    strncpy((char*)msg.data + sizeof(uint64_t), groupName.c_str(), 229 - sizeof(uint64_t));
    ((char*)msg.data)[229] = '\0'; // Safety
    
    msg.dataLength = sizeof(uint64_t) + strlen(groupName.c_str()) + 1;
    
    sendMessage(msg);
    Serial.printf("Mesh: ASSIGN_GROUP broadcast complete\r\n");
}

void MeshNetworkManager::handleAssignGroup(const MeshMessage& msg) {
    if (msg.dataLength < sizeof(uint64_t)) return;
    
    uint64_t targetId;
    memcpy(&targetId, msg.data, sizeof(uint64_t));
    
    if (targetId == myId) {
        char name[230];
        strncpy(name, (char*)msg.data + sizeof(uint64_t), 230 - sizeof(uint64_t));
        name[229] = '\0';
        
        Serial.printf("Mesh: Received ASSIGN_GROUP command. New Group: '%s'\r\n", name);
        setGroupName(name);
        
        // TODO: Save to LittleFS using callback (SystemManager)
        // We will implement this in SystemManager
    }
}

// ==========================================
// SYNC PARAM IMPLEMENTATION
// ==========================================

void MeshNetworkManager::broadcastSyncParam(const std::string& paramName, const std::string& jsonValue) {
    if (myGroupName.empty()) return; // Don't broadcast if not in a group
    
    pendingParamSync.paramName = paramName;
    pendingParamSync.jsonValue = jsonValue;
    pendingParamSync.pending = true;
}

void MeshNetworkManager::doSendSyncParam(const std::string& paramName, const std::string& jsonValue) {
    // Format: GroupName\0ParamName\0JsonValue
    std::string payload = myGroupName;
    payload += '\0';
    payload += paramName;
    payload += '\0';
    payload += jsonValue;
    
    if (payload.length() > 230) {
        Serial.println("Mesh: Sync Param payload too large!");
        return;
    }

    MeshMessage msg;
    msg.type = MessageType::SYNC_PARAM;
    msg.senderId = myId;
    msg.sequenceNumber = sequenceNumber++;
    msg.totalPackets = 1;
    msg.packetIndex = 0;
    
    memcpy(msg.data, payload.c_str(), payload.length());
    msg.dataLength = payload.length();
    
    sendMessage(msg);
    Serial.printf("Mesh: SYNC_PARAM broadcast complete for %s\r\n", paramName.c_str());
}

void MeshNetworkManager::handleSyncParam(const MeshMessage& msg) {
    if (!animManager) return;
    
    const char* raw = (const char*)msg.data;
    size_t len = msg.dataLength;
    
    // Parse: GroupName\0ParamName\0Value
    // Check if group matches
    std::string groupName = raw;
    if (groupName != myGroupName || myGroupName.empty()) {
        return; // Not for us
    }
    
    if (groupName.length() + 1 >= len) return;
    
    std::string paramName = raw + groupName.length() + 1;
    
    if (groupName.length() + 1 + paramName.length() + 1 >= len) return;
    
    std::string jsonValueStr = raw + groupName.length() + 1 + paramName.length() + 1;
    
    // Apply param locally
    Animation* current = animManager->getCurrentAnimation();
    if (current) {
        // We need to parse the JSON value.
        // It's a string from the web: "123", "true", "\"#FF0000\"", "[...]"
        // We can reuse logic or simple parsing.
        // Using ArduinoJson here is safest if we have heap.
        // Or passed string is raw value?
        // WebManager passes whatever `doc["value"]` was.
        
        // Since we don't want to depend heavily on ArduinoJson everywhere if avoidable, but we need it.
        // Let's assume passed string is JSON representation.
        StaticJsonDocument<512> doc;
        DeserializationError error = deserializeJson(doc, jsonValueStr);
        if (!error) {
             // Logic similar to WebManager setParam
             if (doc.is<int>()) current->setParam(paramName.c_str(), doc.as<int>());
             else if (doc.is<float>()) current->setParam(paramName.c_str(), doc.as<float>());
             else if (doc.is<bool>()) current->setParam(paramName.c_str(), doc.as<bool>());
             else if (doc.is<const char*>()) {
                  const char* val = doc.as<const char*>();
                   if (val && val[0] == '#' && strlen(val) == 7) {
                       int r, g, b;
                       if (sscanf(val + 1, "%02x%02x%02x", &r, &g, &b) == 3) {
                           current->setParam(paramName.c_str(), CRGB(r, g, b));
                       }
                   }
             }
             // Dynamic Palette array logic omitted for brevity/complexity, can add if needed
             else if (doc.is<JsonArray>()) {
                    JsonArray arr = doc.as<JsonArray>();
                    if (current->findParameter(paramName.c_str())->type == PARAM_DYNAMIC_PALETTE) {
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
                       if (newPal.colors.empty()) newPal.colors.push_back(CRGB::Black);
                       current->setParam(paramName.c_str(), newPal);
                    }
             }
        }
    }
}

// ==========================================
// SYNC POWER IMPLEMENTATION
// ==========================================

void MeshNetworkManager::broadcastSyncPower(bool powerOn) {
    if (myGroupName.empty()) return;
    
    pendingPowerSync.powerOn = powerOn;
    pendingPowerSync.pending = true;
}

void MeshNetworkManager::doSendSyncPower(bool powerOn) {
    std::string payload = myGroupName;
    payload += '\0';
    payload += (powerOn ? "1" : "0");
    
    MeshMessage msg;
    msg.type = MessageType::SYNC_POWER;
    msg.senderId = myId;
    msg.sequenceNumber = sequenceNumber++;
    msg.totalPackets = 1;
    msg.packetIndex = 0;
    
    memcpy(msg.data, payload.c_str(), payload.length());
    msg.dataLength = payload.length();
    
    sendMessage(msg);
    Serial.printf("Mesh: SYNC_POWER broadcast complete: %s\r\n", powerOn ? "ON" : "OFF");
}

void MeshNetworkManager::handleSyncPower(const MeshMessage& msg) {
    if (!animManager) return;
    
    const char* raw = (const char*)msg.data;
    size_t len = msg.dataLength;
    
    std::string groupName = raw;
    if (groupName != myGroupName || myGroupName.empty()) return;
    
    if (groupName.length() + 1 >= len) return;
    
    // Value is "1" or "0"
    char val = raw[groupName.length() + 1];
    bool powerOn = (val == '1');
    
    if (animManager->getPower() != powerOn) {
        Serial.printf("Mesh: syncing power %s\r\n", powerOn ? "ON" : "OFF");
        animManager->setPower(powerOn);
    }
}
