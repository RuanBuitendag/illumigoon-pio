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

struct AnimationStatePayload {
    uint8_t animationIndex;
    uint32_t startTime;
};

struct MeshMessage {
    MessageType type;
    uint64_t senderId;
    uint32_t sequenceNumber;
    uint8_t totalPackets;
    uint8_t packetIndex;
    uint8_t dataLength;
    uint8_t data[240]; // 250 total - 10 bytes header = 240 for data
};

class MeshNetworkManager {
public:


    MeshNetworkManager(LedController& ledController);

    void begin();
    void update();

    // New: Broadcast Animation State
    void broadcastAnimationState(uint8_t index, uint32_t startTime);

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
    void startElection();
    void becomeCoordinator();
    void sendHeartbeat();
    void sendMessage(const MeshMessage& msg);
};
