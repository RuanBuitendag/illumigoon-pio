#include "system/MeshNetworkManager.h"
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
      timeOffset(0) {}

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
            // Send heartbeat every 500ms
            if (now - lastHeartbeatTime > 500) {
                sendHeartbeat();
                sendTimeSync();
                lastHeartbeatTime = now;
            }
            break;

        case NodeState::SLAVE:
            // Check for master heartbeat timeout
            if (now - lastHeartbeatTime > 1500) {
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
        
        // Simple sync: Update offset
        // Current Time (Local) = millis()
        // Master Time = masterTime
        // Offset = Master - Local
        uint32_t localTime = millis();
        timeOffset = masterTime - localTime;
        Serial.printf("[TimeSync] Synced with master %llX. Master: %lu, Local: %lu, Offset: %ld\n", msg.senderId, masterTime, localTime, timeOffset);
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
