/*
 * FishPeer — Lifecycle, dispatch, connection management, helpers
 *
 * Creates UDP socket, binds to port, starts receive + tick threads.
 * Dispatches incoming packets by ID to appropriate handlers.
 * Manages connection table with address hashing.
 */

#include "fishnet/FishPeer.h"
#include "fishnet/packets/DisconnectionNotification.h"

#include <random>
#include <cstdio>

namespace fishnet {

namespace {

bool resolveBindAddressV4(const std::string& bindIp, in_addr& out) {
    if (bindIp.empty() || bindIp == "*" || bindIp == "0.0.0.0" || bindIp == "any") {
        out.s_addr = INADDR_ANY;
        return true;
    }

    if (inet_pton(AF_INET, bindIp.c_str(), &out) == 1) {
        return true;
    }

    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;

    addrinfo* result = nullptr;
    if (getaddrinfo(bindIp.c_str(), nullptr, &hints, &result) != 0 || result == nullptr) {
        return false;
    }

    out = reinterpret_cast<sockaddr_in*>(result->ai_addr)->sin_addr;
    freeaddrinfo(result);
    return true;
}

} // namespace

// Helpers

uint64_t FishPeer::addrHash(const Address& addr) {
    return (static_cast<uint64_t>(addr.raw.sin_addr.s_addr) << 16) ^
            static_cast<uint64_t>(addr.raw.sin_port);
}

Connection* FishPeer::findConnectionByAddr(const Address& addr) {
    auto h = addrHash(addr);
    auto it = connectionsByAddr_.find(h);
    if (it != connectionsByAddr_.end()) return it->second;
    return nullptr;
}

void FishPeer::removeConnection(const Address& addr, uint64_t guid) {
    connectionsByAddr_.erase(addrHash(addr));
    connectionsByGuid_.erase(guid);
}

uint64_t FishPeer::getTimestamp() const {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - startTime_
    ).count();
}

// Construction / destruction

FishPeer::FishPeer(uint16_t port, const std::string& bindIp)
    : port_(port), bindIp_(bindIp) {
    std::random_device rd;
    std::mt19937_64 gen(rd());
    guid_ = gen();
    startTime_ = std::chrono::steady_clock::now();
}

FishPeer::~FishPeer() {
    stop();
}

// Lifecycle

bool FishPeer::start() {
    if (running_.load()) return false;
    if (!platform::initSockets()) return false;

    sock_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock_ == InvalidSocket) return false;

    platform::setRecvBufferSize(sock_, static_cast<int>(config_.recvBufferSize));
    int sendBuf = static_cast<int>(config_.recvBufferSize);
    setsockopt(sock_, SOL_SOCKET, SO_SNDBUF,
               reinterpret_cast<const char*>(&sendBuf), sizeof(sendBuf));

    sockaddr_in bindAddr{};
    bindAddr.sin_family = AF_INET;
    if (!resolveBindAddressV4(bindIp_, bindAddr.sin_addr)) {
        platform::closeSocket(sock_);
        sock_ = InvalidSocket;
        return false;
    }
    bindAddr.sin_port = htons(port_);

    if (bind(sock_, reinterpret_cast<sockaddr*>(&bindAddr), sizeof(bindAddr)) == SocketError) {
        platform::closeSocket(sock_);
        sock_ = InvalidSocket;
        return false;
    }

    if (port_ == 0) {
        sockaddr_in assigned{};
        socklen_t assignedLen = sizeof(assigned);
        if (getsockname(sock_, reinterpret_cast<sockaddr*>(&assigned), &assignedLen) == 0) {
            port_ = ntohs(assigned.sin_port);
        }
    }

    running_ = true;
    recvThread_ = std::thread(&FishPeer::receiveLoop, this);
    tickThread_ = std::thread(&FishPeer::tickLoop, this);
    return true;
}

void FishPeer::stop() {
    if (!running_.load()) return;
    disconnectAll();
    running_ = false;
    platform::closeSocket(sock_);
    sock_ = InvalidSocket;

    if (recvThread_.joinable()) recvThread_.join();
    if (tickThread_.joinable()) tickThread_.join();

    {
        std::lock_guard<std::mutex> lock(sendMutex_);
        pendingDatagrams_.clear();
    }
    {
        std::lock_guard<std::mutex> lock(connMutex_);
        connectionsByGuid_.clear();
        connectionsByAddr_.clear();
    }
    {
        std::lock_guard<std::mutex> lock(splitMutex_);
        splitStore_.clear();
    }
    peerRecvState_.clear();
    orderedStatesByPeer_.clear();
    platform::cleanupSockets();
}

// Callbacks

void FishPeer::setPacketCallback(PacketCallback cb) { packetCallback_ = std::move(cb); }
void FishPeer::setPongDataProvider(PongDataProvider provider) { pongDataProvider_ = std::move(provider); }
void FishPeer::setConnectionCallback(ConnectionCallback cb) { connectionCallback_ = std::move(cb); }
void FishPeer::setDisconnectCallback(DisconnectCallback cb) { disconnectCallback_ = std::move(cb); }

// Connection management

bool FishPeer::isConnected(const Address& peer) const {
    std::lock_guard<std::mutex> lock(connMutex_);
    auto h = addrHash(peer);
    auto it = connectionsByAddr_.find(h);
    return it != connectionsByAddr_.end() && it->second->connected;
}

size_t FishPeer::getConnectionCount() const {
    std::lock_guard<std::mutex> lock(connMutex_);
    return connectionsByGuid_.size();
}

void FishPeer::disconnect(const Address& peer) {
    uint64_t guid = 0;
    {
        std::lock_guard<std::mutex> lock(connMutex_);
        auto* conn = findConnectionByAddr(peer);
        if (!conn) return;
        guid = conn->guid;
    }

    auto pkt = DisconnectionNotification::encode();
    sendReliableOrdered(pkt.data(), pkt.size(), peer, 0);

    {
        std::lock_guard<std::mutex> lock(connMutex_);
        removeConnection(peer, guid);
    }

    if (disconnectCallback_) disconnectCallback_(peer, guid);
}

void FishPeer::disconnectAll() {
    std::vector<std::pair<Address, uint64_t>> toDisconnect;
    {
        std::lock_guard<std::mutex> lock(connMutex_);
        for (auto& [guid, conn] : connectionsByGuid_) {
            toDisconnect.push_back({conn.address, guid});
        }
    }

    for (auto& [addr, guid] : toDisconnect) {
        auto pkt = DisconnectionNotification::encode();
        sendReliableOrdered(pkt.data(), pkt.size(), addr, 0);
    }

    {
        std::lock_guard<std::mutex> lock(connMutex_);
        connectionsByGuid_.clear();
        connectionsByAddr_.clear();
    }
}

// Packet dispatch

void FishPeer::handlePacket(const uint8_t* data, size_t len, const Address& sender) {
    if (len < 1) return;
    uint8_t id = data[0];

    // Packet logging
    #ifdef FISHNET_DEBUG
    const char* packetName = "Unknown";
    switch (id) {
        case 0x00: packetName = "ConnectedPing"; break;
        case 0x01: packetName = "UnconnectedPing"; break;
        case 0x02: packetName = "UnconnectedPingOpenConn"; break;
        case 0x03: packetName = "ConnectedPong"; break;
        case 0x05: packetName = "OpenConnectionRequest1"; break;
        case 0x06: packetName = "OpenConnectionReply1"; break;
        case 0x07: packetName = "OpenConnectionRequest2"; break;
        case 0x08: packetName = "OpenConnectionReply2"; break;
        case 0x09: packetName = "ConnectionRequest"; break;
        case 0x10: packetName = "ConnectionRequestAccepted"; break;
        case 0x13: packetName = "NewIncomingConnection"; break;
        case 0x15: packetName = "DisconnectionNotification"; break;
        case 0x1C: packetName = "UnconnectedPong"; break;
        case 0xC0: packetName = "ACK"; break;
        case 0xA0: packetName = "NAK"; break;
        case 0xFE: packetName = "GamePacket"; break;
        default:
            if (id >= 0x80 && id <= 0x8D) packetName = "FrameSet";
            break;
    }
    std::printf("[FISHNET] IN  0x%02X %-28s %zu bytes from %s\n",
                id, packetName, len, sender.toString().c_str());
    #endif

    {
        std::lock_guard<std::mutex> lock(connMutex_);
        auto* conn = findConnectionByAddr(sender);
        if (conn) conn->touch();
    }

    if (id == PacketId::ACK) { handleAck(data, len); return; }
    if (id == PacketId::NAK) { handleNak(data, len, sender); return; }

    if (id <= 0x1F) {
        handleSystemPacket(data, len, sender);
    } else if (PacketId::isFrameSet(id)) {
        handleFrameSet(data, len, sender);
    } else if (packetCallback_) {
        packetCallback_(data, len, sender);
    }
}

void FishPeer::handleSystemPacket(const uint8_t* data, size_t len, const Address& sender) {
    switch (data[0]) {
        case PacketId::UnconnectedPing:
        case PacketId::UnconnectedPingOpenConn:
            handleUnconnectedPing(data, len, sender); break;
        case PacketId::OpenConnectionRequest1:
            handleOpenConnRequest1(data, len, sender); break;
        case PacketId::OpenConnectionReply1:
            handleOpenConnReply1(data, len, sender); break;
        case PacketId::OpenConnectionRequest2:
            handleOpenConnRequest2(data, len, sender); break;
        case PacketId::OpenConnectionReply2:
            handleOpenConnReply2(data, len, sender); break;
        default:
            if (packetCallback_) packetCallback_(data, len, sender);
            break;
    }
}

void FishPeer::deliverInner(const uint8_t* data, size_t len, const Address& sender) {
    if (len == 0) return;

    #ifdef FISHNET_DEBUG
    std::printf("[FISHNET] INNER 0x%02X %zu bytes from %s\n", data[0], len, sender.toString().c_str());
    #endif

    switch (data[0]) {
        case PacketId::ConnectionRequest:
            handleConnectionRequest(data, len, sender); break;
        case PacketId::ConnectionRequestAccepted:
            handleConnectionRequestAccepted(data, len, sender); break;
        case PacketId::NewIncomingConnection:
            handleNewIncomingConnection(data, len, sender); break;
        case PacketId::DisconnectionNotification:
            handleDisconnect(data, len, sender); break;
        case PacketId::ConnectedPing:
            handleConnectedPing(data, len, sender); break;
        case PacketId::ConnectedPong:
            handleConnectedPong(data, len, sender); break;
        default:
            if (packetCallback_) packetCallback_(data, len, sender);
            break;
    }
}

// Receive loop

void FishPeer::receiveLoop() {
    while (running_.load()) {
        uint8_t buffer[2048];
        sockaddr_in clientAddr{};
        socklen_t addrLen = sizeof(clientAddr);
        int received = recvfrom(sock_, reinterpret_cast<char*>(buffer), sizeof(buffer), 0,
                                reinterpret_cast<sockaddr*>(&clientAddr), &addrLen);
        if (received > 0) {
            handlePacket(buffer, static_cast<size_t>(received), Address(clientAddr));
        }
    }
}

} // namespace fishnet
