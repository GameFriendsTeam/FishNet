#pragma once

/*
 * FishNet Connection
 *
 * Represents a single peer connection with its state.
 * Tracks: address, GUID, connected flag, last activity timestamp.
 * Used internally by FishPeer to manage active connections
 * and detect timeouts.
 */

#include <fishnet/utils/Address.h>
#include <cstdint>
#include <chrono>

namespace fishnet {

class FISHNET_API Connection {
public:
    Address address;
    uint64_t guid = 0;
    uint64_t lastPingTime = 0;
    bool connected = false;
    std::chrono::steady_clock::time_point lastActivity;

    Connection() : lastActivity(std::chrono::steady_clock::now()) {}

    Connection(const Address& addr, uint64_t guid)
        : address(addr), guid(guid), connected(true),
          lastActivity(std::chrono::steady_clock::now()) {}

    void touch() {
        lastActivity = std::chrono::steady_clock::now();
    }

    bool isTimedOut(uint64_t timeoutMs) const {
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - lastActivity
        ).count();
        return elapsed > static_cast<int64_t>(timeoutMs);
    }
};

} // namespace fishnet
