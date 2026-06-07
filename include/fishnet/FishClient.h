#pragma once

/*
 * FishNet Client
 *
 * High-level client wrapper around FishPeer.
 * Connects to a remote server with full MTU discovery handshake.
 * Provides connect/disconnect/packet callbacks.
 *
 * Usage: create → start() → connect(host, port) → wait for
 * ConnectionCallback → send/receive → disconnect() → stop().
 */

#include <fishnet/FishPeer.h>
#include <fishnet/PacketHandler.h>
#include <fishnet/utils/Address.h>
#include <memory>
#include <string>

namespace fishnet {

class FISHNET_API FishClient {
public:
    FishClient();
    ~FishClient();

    bool start();
    void stop();
    bool isRunning() const;

    /// Connect to a remote server (full handshake with MTU discovery)
    void connect(const std::string& host, uint16_t port);

    /// Graceful disconnect
    void disconnect();

    /// Is the connection fully established?
    bool isConnected() const;

    // Callbacks
    void setPacketCallback(PacketCallback cb);
    void setConnectionCallback(ConnectionCallback cb);
    void setDisconnectCallback(DisconnectCallback cb);

    // Configuration (call before start())
    void setConfig(const PeerConfig& config);

    // Sending
    void send(const uint8_t* data, size_t len);
    void sendReliable(const uint8_t* data, size_t len, uint8_t channel = 0);

    // Access underlying peer
    FishPeer& getPeer() { return *peer_; }
    const FishPeer& getPeer() const { return *peer_; }

private:
    std::unique_ptr<FishPeer> peer_;
    Address serverAddress_;
};

} // namespace fishnet
