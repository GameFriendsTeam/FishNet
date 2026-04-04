#pragma once

/*
 * FishNet Server
 *
 * High-level server wrapper around FishPeer.
 * Listens on a port, accepts incoming RakNet connections,
 * handles keep-alive, timeouts, and provides callbacks for
 * connect/disconnect/packet events.
 *
 * Users supply a PongDataProvider callback to respond to
 * unconnected pings (e.g. server list MOTD).
 */

#include "fishnet/FishPeer.h"
#include "fishnet/PacketHandler.h"
#include <memory>

namespace fishnet {

class FISHNET_API FishServer {
public:
    explicit FishServer(uint16_t port);
    ~FishServer();

    bool start();
    void stop();
    bool isRunning() const;

    // Callbacks
    void setPacketCallback(PacketCallback cb);
    void setPongDataProvider(PongDataProvider provider);
    void setConnectionCallback(ConnectionCallback cb);
    void setDisconnectCallback(DisconnectCallback cb);

    // Configuration (call before start())
    void setConfig(const PeerConfig& config);

    // Sending
    void sendTo(const uint8_t* data, size_t len, const Address& dest);
    void sendReliableTo(const uint8_t* data, size_t len, const Address& dest, uint8_t channel = 0);

    // Connection management
    void disconnectPeer(const Address& peer);
    void disconnectAll();
    size_t getConnectionCount() const;

    // Access underlying peer
    FishPeer& getPeer() { return *peer_; }
    const FishPeer& getPeer() const { return *peer_; }

private:
    std::unique_ptr<FishPeer> peer_;
};

} // namespace fishnet
