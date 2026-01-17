#pragma once
#include <esp_now.h>
#include <WiFi.h>
#include <vector>
#include <functional> // Added for std::function
#include "system/LedController.h"

enum class NodeState {
    STARTUP,
    ELECTION,
    MASTER,
    SLAVE,
    IDLE
};

enum class MessageType : uint8_t {
    HEARTBEAT = 0,
    ELECTION = 1,
    OK = 2,
    COORDINATOR = 3,
    FRAME_DATA = 4,
    PEER_ANNOUNCEMENT = 5,
    SHUTDOWN = 6,
    TIME_SYNC = 7, // New 
    ANIMATION_STATE = 8 // New
};

struct __attribute__((packed)) AnimationStatePayload {
    char animationName[32];
    uint32_t startTime;
};

struct __attribute__((packed)) MeshMessage {
    MessageType type;
    uint64_t senderId;
    uint32_t sequenceNumber;
    uint8_t totalPackets;
    uint8_t packetIndex;
    uint8_t dataLength;
    uint8_t data[230]; // 246 total - 16 bytes header = 230 for data
};

struct __attribute__((packed)) PeerAnnouncementPayload {
    uint32_t ip;
    NodeState role;
};

struct PeerInfo {
    uint64_t id;
    uint32_t ip;
    NodeState role;
    unsigned long lastSeen;
};

class MeshNetworkManager {
public:

    MeshNetworkManager(LedController& ledController);

    void begin();
    void update();

    // New: Broadcast Animation State
    void broadcastAnimationState(const char* name, uint32_t startTime);

    // New: Get synchronized network time
    uint32_t getNetworkTime() const;



    bool isMaster() const;
    bool isSlave() const;

    void prepareForOTA();

private:
    LedController& ledController;
    uint64_t myId;
    NodeState currentState;
    uint64_t masterId;
    unsigned long lastHeartbeatTime;
    unsigned long lastElectionTime;
    uint32_t sequenceNumber;
    bool electionInProgress;
    bool receivedOK;
    
    int32_t timeOffset; // New


    uint8_t broadcastAddress[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

    // Frame assembly for slaves
    struct FrameBuffer {
        uint32_t sequenceNumber;
        uint8_t totalPackets;
        uint8_t receivedPackets;
        CRGB* leds;
        unsigned long lastPacketTime;
    };
    FrameBuffer frameBuffer = {0, 0, 0, nullptr, 0};

    static MeshNetworkManager* instance;

    static void onReceiveWrapper(const uint8_t* mac, const uint8_t* data, int len);
    void onReceive(const uint8_t* mac, const uint8_t* data, int len);

    void sendTimeSync();
    void handleTimeSync(const MeshMessage& msg);
    void handleAnimationState(const MeshMessage& msg);
    void handleHeartbeat(const MeshMessage& msg);
    void handleElection(const MeshMessage& msg);
    void handleOK(const MeshMessage& msg);
    void handleCoordinator(const MeshMessage& msg);
    void handleFrameData(const MeshMessage& msg);
    void handleShutdown(const MeshMessage& msg);
    void handlePeerAnnouncement(const MeshMessage& msg);
    void startElection();
    void becomeCoordinator();
    void sendHeartbeat();
    void sendMessage(const MeshMessage& msg);
    
    // New: Peer Discovery
    void sendPeerAnnouncement();
    std::vector<PeerInfo> knownPeers;

public: 
    std::vector<PeerInfo> getPeers() const { return knownPeers; }
};
