/*
 * FishPeer — Handshake
 *
 * Server-side: handles UnconnectedPing, OpenConnectionRequest1/2,
 *   ConnectionRequest, NewIncomingConnection, Disconnect, ConnectedPing/Pong.
 * Client-side: handles OpenConnectionReply1/2, ConnectionRequestAccepted.
 *   Implements MTU discovery (tries 1464 → 1172 → 548).
 *   Initiates connection via connectTo().
 */

#include "fishnet/FishPeer.h"
#include "fishnet/packets/UnconnectedPing.h"
#include "fishnet/packets/UnconnectedPong.h"
#include "fishnet/packets/OpenConnectionRequest1.h"
#include "fishnet/packets/OpenConnectionReply1.h"
#include "fishnet/packets/OpenConnectionRequest2.h"
#include "fishnet/packets/OpenConnectionReply2.h"
#include "fishnet/packets/ConnectionRequest.h"
#include "fishnet/packets/ConnectionRequestAccepted.h"
#include "fishnet/packets/NewIncomingConnection.h"
#include "fishnet/packets/ConnectedPing.h"

namespace fishnet {

// Server-side handlers

void FishPeer::handleUnconnectedPing(const uint8_t* data, size_t len, const Address& sender) {
    UnconnectedPing ping;
    if (!UnconnectedPing::decode(data, len, ping)) return;

    std::string pongData;
    if (pongDataProvider_) pongData = pongDataProvider_();
    if (pongData.empty()) return;

    auto pong = UnconnectedPong::encode(ping.clientTimestamp, guid_, pongData);
    sendRaw(pong.data(), pong.size(), sender);
}

void FishPeer::handleOpenConnRequest1(const uint8_t* data, size_t len, const Address& sender) {
    OpenConnectionRequest1 req;
    if (!OpenConnectionRequest1::decode(data, len, req)) return;

    auto reply = OpenConnectionReply1::encode(guid_, false, req.mtuSize);
    sendRaw(reply.data(), reply.size(), sender);
}

void FishPeer::handleOpenConnRequest2(const uint8_t* data, size_t len, const Address& sender) {
    OpenConnectionRequest2 req;
    if (!OpenConnectionRequest2::decode(data, len, req)) return;

    mtu_ = req.mtuSize ? req.mtuSize : DEFAULT_MTU;
    auto reply = OpenConnectionReply2::encode(guid_, sender, static_cast<uint16_t>(mtu_), false);
    sendRaw(reply.data(), reply.size(), sender);
}

void FishPeer::handleConnectionRequest(const uint8_t* data, size_t len, const Address& sender) {
    ConnectionRequest req;
    if (!ConnectionRequest::decode(data, len, req)) return;

    bool duplicateConnectedRequest = false;
    {
        std::lock_guard<std::mutex> lock(connMutex_);
        auto* existing = findConnectionByAddr(sender);
        if (existing && existing->guid == req.clientGuid && existing->connected) {
            duplicateConnectedRequest = true;
            existing->touch();
        }
    }

    if (!duplicateConnectedRequest) {
        // A fresh ConnectionRequest means a new RakNet session attempt.
        // Reset transient receive/order/split state for this peer to avoid
        // stale fragments or order indices blocking the new handshake.
        {
            const uint64_t ah = addrHash(sender);
            peerRecvState_.erase(ah);
            orderedStatesByPeer_.erase(ah);
        }
        {
            std::lock_guard<std::mutex> lock(splitMutex_);
            const uint32_t senderAddr = sender.raw.sin_addr.s_addr;
            const uint16_t senderPort = ntohs(sender.raw.sin_port);
            for (auto it = splitStore_.begin(); it != splitStore_.end(); ) {
                if (it->first.addr == senderAddr && it->first.port == senderPort) {
                    it = splitStore_.erase(it);
                } else {
                    ++it;
                }
            }
        }
        {
            std::lock_guard<std::mutex> lock(sendMutex_);
            for (auto it = pendingDatagrams_.begin(); it != pendingDatagrams_.end(); ) {
                if (it->second.dest == sender) {
                    it = pendingDatagrams_.erase(it);
                } else {
                    ++it;
                }
            }
        }
    }

    #ifdef FISHNET_DEBUG
    std::printf("[FISHNET] ConnectionRequest raw (%zu bytes): ", len);
    for (size_t i = 0; i < len; ++i) {
        std::printf("%02X ", data[i]);
    }
    std::printf("\n");
    std::printf("[FISHNET] ConnectionRequest: guid=%llu timestamp=%llu security=%d\n",
                static_cast<unsigned long long>(req.clientGuid),
                static_cast<unsigned long long>(req.requestTimestamp),
                req.useSecurity);
    #endif

    if (!duplicateConnectedRequest) {
        Connection conn(sender, req.clientGuid);
        {
            std::lock_guard<std::mutex> lock(connMutex_);
            connectionsByGuid_[req.clientGuid] = conn;
            connectionsByAddr_[addrHash(sender)] = &connectionsByGuid_[req.clientGuid];
        }
    }

    auto accepted = ConnectionRequestAccepted::encode(sender, req.requestTimestamp, getTimestamp());

    #ifdef FISHNET_DEBUG
    std::printf("[FISHNET] ConnectionRequestAccepted: %zu bytes, hex: ", accepted.size());
    for (size_t i = 0; i < accepted.size() && i < 32; ++i) {
        std::printf("%02X ", accepted.data()[i]);
    }
    std::printf("...\n");
    #endif

    sendReliableOrdered(accepted.data(), accepted.size(), sender, 0);
}

void FishPeer::handleNewIncomingConnection(const uint8_t* data, size_t len, const Address& sender) {
    std::lock_guard<std::mutex> lock(connMutex_);
    auto* conn = findConnectionByAddr(sender);
    if (conn) {
        const bool wasConnected = conn->connected;
        conn->connected = true;
        conn->touch();
        if (!wasConnected && connectionCallback_) {
            connectionCallback_(sender, conn->guid);
        }
    }
}

void FishPeer::handleDisconnect(const uint8_t* data, size_t len, const Address& sender) {
    (void)data; (void)len;
    uint64_t guid = 0;
    const uint64_t ah = addrHash(sender);
    {
        std::lock_guard<std::mutex> lock(connMutex_);
        auto* conn = findConnectionByAddr(sender);
        if (!conn) return;
        guid = conn->guid;
        removeConnection(sender, guid);
    }
    peerRecvState_.erase(ah);
    orderedStatesByPeer_.erase(ah);
    {
        std::lock_guard<std::mutex> lock(splitMutex_);
        const uint32_t senderAddr = sender.raw.sin_addr.s_addr;
        const uint16_t senderPort = ntohs(sender.raw.sin_port);
        for (auto it = splitStore_.begin(); it != splitStore_.end(); ) {
            if (it->first.addr == senderAddr && it->first.port == senderPort) {
                it = splitStore_.erase(it);
            } else {
                ++it;
            }
        }
    }
    {
        std::lock_guard<std::mutex> lock(sendMutex_);
        for (auto it = pendingDatagrams_.begin(); it != pendingDatagrams_.end(); ) {
            if (it->second.dest == sender) {
                it = pendingDatagrams_.erase(it);
            } else {
                ++it;
            }
        }
    }
    if (disconnectCallback_) disconnectCallback_(sender, guid);
}

void FishPeer::handleConnectedPing(const uint8_t* data, size_t len, const Address& sender) {
    ConnectedPing ping;
    if (!ConnectedPing::decode(data, len, ping)) return;

    auto pong = ConnectedPong::encode(ping.timestamp, getTimestamp());
    sendUnreliable(pong.data(), pong.size(), sender);
}

void FishPeer::handleConnectedPong(const uint8_t* data, size_t len, const Address& sender) {
    (void)data; (void)len; (void)sender;
    // Touch is done in handlePacket already. RTT calculation could go here.
}

// Client-side handlers

void FishPeer::connectTo(const Address& server) {
    clientTargetServer_ = server;
    clientState_ = ClientState::MtuDiscovery;
    mtuAttempt_ = 0;
    clientHandshakeTime_ = std::chrono::steady_clock::now();

    uint16_t tryMtu = config_.mtuSizes[0];
    auto req = OpenConnectionRequest1::encode(11, tryMtu);
    sendRaw(req.data(), req.size(), server);
}

void FishPeer::handleOpenConnReply1(const uint8_t* data, size_t len, const Address& sender) {
    if (clientState_ != ClientState::MtuDiscovery) return;

    OpenConnectionReply1 reply;
    if (!OpenConnectionReply1::decode(data, len, reply)) return;

    mtu_ = reply.mtuSize;
    clientState_ = ClientState::WaitingReply2;
    clientHandshakeTime_ = std::chrono::steady_clock::now();

    auto req = OpenConnectionRequest2::encode(sender, static_cast<uint16_t>(mtu_), guid_);
    sendRaw(req.data(), req.size(), sender);
}

void FishPeer::handleOpenConnReply2(const uint8_t* data, size_t len, const Address& sender) {
    if (clientState_ != ClientState::WaitingReply2) return;

    OpenConnectionReply2 reply;
    if (!OpenConnectionReply2::decode(data, len, reply)) return;

    mtu_ = reply.mtuSize;
    clientState_ = ClientState::WaitingAccepted;
    clientHandshakeTime_ = std::chrono::steady_clock::now();

    auto req = ConnectionRequest::encode(guid_, getTimestamp(), false);
    sendReliableOrdered(req.data(), req.size(), sender, 0);
}

void FishPeer::handleConnectionRequestAccepted(const uint8_t* data, size_t len, const Address& sender) {
    if (clientState_ != ClientState::WaitingAccepted) return;

    ConnectionRequestAccepted accepted;
    if (!ConnectionRequestAccepted::decode(data, len, accepted)) return;

    clientState_ = ClientState::Connected;

    uint64_t serverGuid = addrHash(sender);
    Connection conn(sender, serverGuid);
    {
        std::lock_guard<std::mutex> lock(connMutex_);
        connectionsByGuid_[serverGuid] = conn;
        connectionsByAddr_[addrHash(sender)] = &connectionsByGuid_[serverGuid];
    }

    auto nic = NewIncomingConnection::encode(sender, accepted.requestTimestamp, getTimestamp());
    sendReliableOrdered(nic.data(), nic.size(), sender, 0);

    if (connectionCallback_) connectionCallback_(sender, serverGuid);
}

} // namespace fishnet
