#pragma once

/*
 * FishNet FishPeer — Core Networking Engine
 *
 * The heart of FishNet. Manages a UDP socket with full RakNet protocol:
 *   - Offline handshake (server & client side, with MTU discovery)
 *   - Frame set encoding/decoding with reliability layer
 *   - Split packet sending and reassembly for large payloads
 *   - ACK/NAK with automatic retransmission on timeout
 *   - Connected ping/pong keep-alive
 *   - 32 ordered delivery channels
 *   - Connection timeout detection and cleanup
 *   - Graceful disconnect notification
 *
 * Tick loop runs at configurable rate (default 50ms / 20 ticks per sec).
 * All public methods are thread-safe. Callbacks fire from internal threads.
 */

#include <fishnet/platform.h>
#include <fishnet/protocol/Constants.h>
#include <fishnet/protocol/EncapsulatedPacket.h>
#include <fishnet/Connection.h>
#include <fishnet/PacketHandler.h>
#include <fishnet/utils/Address.h>
#include <fishnet/utils/BinaryBuffer.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace fishnet {

// User-supplied callback for unconnected pong payload.
// Return empty string to ignore pings.
using PongDataProvider = std::function<std::string()>;

// Called when a new peer completes the connection handshake.
using ConnectionCallback = std::function<void(const Address& peer, uint64_t guid)>;

// Called when a peer disconnects (timeout or graceful).
using DisconnectCallback = std::function<void(const Address& peer, uint64_t guid)>;

// Configuration
struct PeerConfig {
    uint32_t recvBufferSize     = 1024 * 1024;   // Socket recv buffer
    uint32_t connectionTimeoutMs = 10000;         // 10s connection timeout
    uint32_t pingIntervalMs     = 2000;           // 2s keep-alive ping
    uint32_t retransmitTimeoutMs = 1500;          // 1.5s before re-sending unacked datagrams
    uint32_t maxPendingDatagrams = 512;           // Max unacked datagrams before dropping
    uint32_t splitStaleTimeoutMs = 30000;         // 30s before discarding incomplete splits
    uint32_t tickIntervalMs     = 50;             // 50ms tick rate (20 ticks/sec)
    uint16_t mtuSizes[3]        = {1464, 1172, 548}; // MTU discovery sizes to try
};

class FISHNET_API FishPeer {
public:
    explicit FishPeer(uint16_t port, const std::string& bindIp = "0.0.0.0");
    ~FishPeer();

    FishPeer(const FishPeer&) = delete;
    FishPeer& operator=(const FishPeer&) = delete;

    // Lifecycle
    bool start();
    void stop();
    bool isRunning() const { return running_.load(); }

    // Configuration (call before start())
    void setConfig(const PeerConfig& config) { config_ = config; }
    const PeerConfig& getConfig() const { return config_; }

    // Callbacks
    void setPacketCallback(PacketCallback cb);
    void setPongDataProvider(PongDataProvider provider);
    void setConnectionCallback(ConnectionCallback cb);
    void setDisconnectCallback(DisconnectCallback cb);

    // Sending
    void sendRaw(const uint8_t* data, size_t len, const Address& dest);
    void sendReliableOrdered(const uint8_t* data, size_t len, const Address& dest, uint8_t channel = 0);
    void sendUnreliable(const uint8_t* data, size_t len, const Address& dest);

    // Connection management
    void disconnect(const Address& peer);
    void disconnectAll();
    bool isConnected(const Address& peer) const;
    size_t getConnectionCount() const;

    // Client-side handshake
    // Called by FishClient to initiate connection.
    // Starts MTU discovery → OpenConnectionRequest1/2 → ConnectionRequest
    void connectTo(const Address& server);

    // Accessors
    uint64_t getGuid() const { return guid_; }
    uint16_t getPort() const { return port_; }
    uint32_t getMtu() const { return mtu_; }
    const std::string& getBindIp() const { return bindIp_; }

private:
    // Socket
    SocketHandle sock_ = InvalidSocket;
    uint16_t port_;
    std::string bindIp_;
    std::atomic<bool> running_{false};

    // Threads
    std::thread recvThread_;
    std::thread tickThread_;

    // Identity
    uint64_t guid_;
    std::chrono::steady_clock::time_point startTime_;

    // Config
    PeerConfig config_;

    // Reliability state
    uint32_t mtu_ = DEFAULT_MTU;
    uint32_t outSeq_ = 0;
    uint32_t outMsgIndex_ = 0;
    uint32_t outOrderIndex_ = 0;
    uint16_t outSplitId_ = 0;
    mutable std::mutex sendMutex_;

    struct PendingDatagram {
        std::vector<uint8_t> bytes;
        Address dest;
        std::chrono::steady_clock::time_point sentAt;
        uint32_t retransmitCount = 0;
    };
    std::unordered_map<uint32_t, PendingDatagram> pendingDatagrams_;

    // Duplicate detection
    // Track received datagram sequence numbers per peer to send NAK for gaps
    struct PeerReceiveState {
        uint32_t expectedSeq = 0;
    };
    std::unordered_map<uint64_t, PeerReceiveState> peerRecvState_; // key = addr hash

    // Split reassembly
    struct SplitKey {
        uint32_t addr;
        uint16_t port;
        uint16_t splitId;
        bool operator==(const SplitKey& o) const {
            return addr == o.addr && port == o.port && splitId == o.splitId;
        }
    };
    struct SplitKeyHash {
        size_t operator()(const SplitKey& k) const {
            return static_cast<size_t>(k.addr) ^
                   (static_cast<size_t>(k.port) << 16) ^
                   (static_cast<size_t>(k.splitId) << 32);
        }
    };
    struct SplitReassembly {
        uint32_t splitCount = 0;
        std::vector<std::vector<uint8_t>> parts;
        uint32_t received = 0;
        std::chrono::steady_clock::time_point createdAt;
    };
    std::unordered_map<SplitKey, SplitReassembly, SplitKeyHash> splitStore_;
    std::mutex splitMutex_;

    // Ordering queues
    struct OrderedQueue {
        uint32_t nextOrderIndex = 0;
        std::map<uint32_t, std::vector<uint8_t>> buffered;
    };
    struct PeerOrderedState {
        OrderedQueue channels[32];
    };
    std::unordered_map<uint64_t, PeerOrderedState> orderedStatesByPeer_;

    // Connections
    std::unordered_map<uint64_t, Connection> connectionsByGuid_;
    std::unordered_map<uint64_t, Connection*> connectionsByAddr_; // addr hash → pointer into connectionsByGuid_
    mutable std::mutex connMutex_;

    // Client-side handshake state
    enum class ClientState {
        Disconnected,
        MtuDiscovery,       // Sending OpenConnectionRequest1 with decreasing MTU
        WaitingReply2,      // Sent OpenConnectionRequest2, waiting reply
        WaitingAccepted,    // Sent ConnectionRequest, waiting accepted
        Connected
    };
    ClientState clientState_ = ClientState::Disconnected;
    Address clientTargetServer_;
    uint8_t mtuAttempt_ = 0; // Index into config_.mtuSizes
    std::chrono::steady_clock::time_point clientHandshakeTime_;

    // Callbacks
    PacketCallback packetCallback_;
    PongDataProvider pongDataProvider_;
    ConnectionCallback connectionCallback_;
    DisconnectCallback disconnectCallback_;

    // Internal: receive loop
    void receiveLoop();

    // Internal: tick loop (retransmit, ping, cleanup)
    void tickLoop();
    void tickRetransmit();
    void tickPing();
    void tickTimeouts();
    void tickSplitCleanup();
    void tickClientHandshake();

    // Packet dispatch
    void handlePacket(const uint8_t* data, size_t len, const Address& sender);
    void handleSystemPacket(const uint8_t* data, size_t len, const Address& sender);
    void handleFrameSet(const uint8_t* data, size_t len, const Address& sender);
    void handleAck(const uint8_t* data, size_t len);
    void handleNak(const uint8_t* data, size_t len, const Address& sender);
    void deliverInner(const uint8_t* data, size_t len, const Address& sender);

    // Offline / system handlers (server side)
    void handleUnconnectedPing(const uint8_t* data, size_t len, const Address& sender);
    void handleOpenConnRequest1(const uint8_t* data, size_t len, const Address& sender);
    void handleOpenConnRequest2(const uint8_t* data, size_t len, const Address& sender);
    void handleConnectionRequest(const uint8_t* data, size_t len, const Address& sender);
    void handleNewIncomingConnection(const uint8_t* data, size_t len, const Address& sender);
    void handleDisconnect(const uint8_t* data, size_t len, const Address& sender);
    void handleConnectedPing(const uint8_t* data, size_t len, const Address& sender);
    void handleConnectedPong(const uint8_t* data, size_t len, const Address& sender);

    // Offline / system handlers (client side)
    void handleOpenConnReply1(const uint8_t* data, size_t len, const Address& sender);
    void handleOpenConnReply2(const uint8_t* data, size_t len, const Address& sender);
    void handleConnectionRequestAccepted(const uint8_t* data, size_t len, const Address& sender);

    // Sending helpers
    void sendAck(const Address& dest, uint32_t seq);
    void sendEncapsulated(const Address& dest, const std::vector<EncapsulatedPacket>& packets);
    void sendSplitReliableOrdered(const uint8_t* data, size_t len, const Address& dest, uint8_t channel);

    // Connection helpers
    Connection* findConnectionByAddr(const Address& addr);
    void removeConnection(const Address& addr, uint64_t guid);
    static uint64_t addrHash(const Address& addr);

    uint64_t getTimestamp() const;
};

} // namespace fishnet
