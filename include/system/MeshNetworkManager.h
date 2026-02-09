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
    PEER_ANNOUNCEMENT = 5,
    SHUTDOWN = 6,
    TIME_SYNC = 7, // New 
    ANIMATION_STATE = 8,
    QUERY_PRESET = 9,
    PRESET_EXIST_RESPONSE = 10,
    SAVE_PRESET = 11,
    DELETE_PRESET = 12,
    CHECK_FOR_UPDATES = 13,
    RENAME_PRESET = 14,
    ASSIGN_GROUP = 15,
    SYNC_PARAM = 16,
    SYNC_POWER = 17,
    REQUEST_SYNC_PRESETS = 18,
    PRESET_MANIFEST = 19,
    REQUEST_PRESET_DATA = 20
};

struct __attribute__((packed)) AnimationStatePayload {
    char animationName[32];
    char groupName[32]; // New
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
    char groupName[32];
    char deviceName[32];
};

struct PeerInfo {
    uint64_t id;
    uint32_t ip;
    NodeState role;
    std::string groupName;
    std::string deviceName;
    unsigned long lastSeen;
};

// Forward declaration
class AnimationManager;

class MeshNetworkManager {
public:

    MeshNetworkManager(LedController& ledController);

    void begin();
    void update();
    
    // Set Animation Manager for preset operations
    void setAnimationManager(AnimationManager* am) { animManager = am; }

    // New: Broadcast Animation State
    void broadcastAnimationState(const char* name, uint32_t startTime);

    // New: Get synchronized network time
    uint32_t getNetworkTime() const;

    // Preset Propagation
    bool checkPresetExists(const std::string& name); // Blocking check
    void broadcastSavePreset(const std::string& name, const std::string& baseType, const std::string& paramsJson);
    // Removed duplicate declaration
    void broadcastDeletePreset(const std::string& name);
    void broadcastRenamePreset(const std::string& oldName, const std::string& newName);
    
    // Preset Synchronization
    void broadcastRequestSyncPresets();
    void broadcastRequestPresetData(const std::string& name, uint64_t targetId);

    // OTA / Updates
    void setOtaCallback(std::function<void()> callback) { otaCallback = callback; }
    void broadcastCheckForUpdates();

    // Group Management
    void broadcastAssignGroup(uint64_t targetId, const char* newGroupName);
    std::string getGroupName() const { return myGroupName; }
    void setGroupName(const std::string& name); // Persist logic will be in SystemManager
    
    // Device Name
    std::string getDeviceName() const { return myDeviceName; }
    void setDeviceName(const std::string& name);
    
    bool isMaster() const;
    bool isSlave() const;

    void prepareForOTA();

private:
    LedController& ledController;
    AnimationManager* animManager = nullptr;
    uint64_t myId;
    NodeState currentState;
    uint64_t masterId;
    unsigned long lastHeartbeatTime;
    unsigned long lastElectionTime;
    uint32_t sequenceNumber;
    bool electionInProgress;
    bool receivedOK;
    
    int32_t timeOffset; 
    double smoothedOffset;
    bool hasSyncedOnce; 

    // Query state
    bool lastQueryFound;
    std::string lastQueryName;

    // Callbacks
    std::function<void()> otaCallback;


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
    
    // Preset assembly buffer
    struct PresetBuffer {
        uint32_t sequenceNumber;
        uint8_t totalPackets;
        uint8_t receivedPackets;
        std::string name;
        std::string baseType;
        std::vector<uint8_t> data; // Just the JSON part
        std::vector<bool> receivedPacketFlags; // Track which packets we have received
        unsigned long lastPacketTime;
    };
    PresetBuffer presetBuffer;

    // Param assembly buffer
    struct ParamBuffer {
        uint32_t sequenceNumber;
        uint8_t totalPackets;
        uint8_t receivedPackets;
        std::vector<uint8_t> data; // JSON value
        std::vector<bool> receivedPacketFlags;
        unsigned long lastPacketTime;
    };
    ParamBuffer paramBuffer;

    // Pending broadcast queue (to send from main loop, not HTTP callback)
    struct PendingPresetBroadcast {
        bool pending = false;
        std::string name;
        std::string baseType;
        std::string paramsJson;
    };
    PendingPresetBroadcast pendingSavePreset;
    std::string pendingDeletePreset;
    struct PendingRename { std::string oldName; std::string newName; };
    PendingRename pendingRenamePreset;

    struct PendingGroupAssignment { 
        bool pending = false;
        uint64_t targetId; 
        std::string groupName; 
    };
    PendingGroupAssignment pendingGroupAssignment;

    struct PendingParamSync {
        bool pending = false;
        std::string paramName;
        std::string jsonValue;
    };
    PendingParamSync pendingParamSync;

    struct PendingPowerSync {
        bool pending = false;
        bool powerOn;
    };
    PendingPowerSync pendingPowerSync;

    std::string myGroupName;
    std::string myDeviceName;
    
    // Track requested presets to avoid spamming requests
    struct RequestTracker {
        std::string name;
        unsigned long requestTime;
    };
    std::vector<RequestTracker> requestedPresets; 
    
    // Non-blocking sync queues
    struct ManifestQueue {
        std::vector<std::string> names;
        unsigned long nextSendTime = 0;
        bool active = false;
    };
    ManifestQueue manifestQueue;
    
    struct DataRequestQueue {
        struct Request {
            std::string name;
            uint64_t targetId;
        };
        std::vector<Request> requests;
        unsigned long nextSendTime = 0;
    };
    DataRequestQueue dataRequestQueue;
    
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
    
    void handleQueryPreset(const MeshMessage& msg);
    void handlePresetExistResponse(const MeshMessage& msg);
    void handleSavePreset(const MeshMessage& msg);
    // Removed duplicate handleSavePreset
    void handleDeletePreset(const MeshMessage& msg);
    void handleRenamePreset(const MeshMessage& msg);
    void handleCheckForUpdates(const MeshMessage& msg);
    void handleAssignGroup(const MeshMessage& msg);
    void handleSyncParam(const MeshMessage& msg);
    void handleSyncPower(const MeshMessage& msg);
    
    void handleRequestSyncPresets(const MeshMessage& msg);
    void handlePresetManifest(const MeshMessage& msg);
    void handleRequestPresetData(const MeshMessage& msg);

    // Actual broadcast implementations (called from update())
    void doSendSavePreset(const std::string& name, const std::string& baseType, const std::string& paramsJson);
    void doSendDeletePreset(const std::string& name);
    void doSendRenamePreset(const std::string& oldName, const std::string& newName);
    void doSendAssignGroup(uint64_t targetId, const std::string& groupName);
    void doSendSyncParam(const std::string& paramName, const std::string& jsonValue);
    void doSendSyncPower(bool powerOn);
    
    void startElection();
    void becomeCoordinator();
    void sendHeartbeat();
    void sendMessage(const MeshMessage& msg);
    
    // New: Peer Discovery
    void sendPeerAnnouncement();
    std::vector<PeerInfo> knownPeers;

public: 
    // Group Sync
    void broadcastSyncParam(const std::string& paramName, const std::string& jsonValue); // JSON value for simplicity
    void broadcastSyncPower(bool powerOn);
    
    std::vector<PeerInfo> getPeers() const { return knownPeers; }
};
