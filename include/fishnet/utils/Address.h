#pragma once

/*
 * FishNet Address
 *
 * Cross-platform wrapper around sockaddr_in.
 * Construct from IP string + port, or from raw sockaddr_in.
 * Provides ip(), port(), toString() helpers, equality comparison,
 * and a Hash struct for use in unordered containers.
 */

#include <fishnet/platform.h>
#include <string>
#include <cstring>
#include <functional>

namespace fishnet {

struct Address {
    sockaddr_in raw{};

    Address() {
        std::memset(&raw, 0, sizeof(raw));
        raw.sin_family = AF_INET;
    }

    Address(const sockaddr_in& addr) : raw(addr) {}

    Address(const std::string& ip, uint16_t port) {
        std::memset(&raw, 0, sizeof(raw));
        raw.sin_family = AF_INET;
        raw.sin_port = htons(port);
        if (inet_pton(AF_INET, ip.c_str(), &raw.sin_addr) == 1) {
            return;
        }

        addrinfo hints{};
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_DGRAM;

        addrinfo* result = nullptr;
        if (getaddrinfo(ip.c_str(), nullptr, &hints, &result) == 0 && result != nullptr) {
            auto* addr = reinterpret_cast<sockaddr_in*>(result->ai_addr);
            raw.sin_addr = addr->sin_addr;
            freeaddrinfo(result);
            return;
        }

        if (result) {
            freeaddrinfo(result);
        }

        raw.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    }

    std::string ip() const {
        char buf[INET_ADDRSTRLEN]{};
        inet_ntop(AF_INET, &raw.sin_addr, buf, sizeof(buf));
        return std::string(buf);
    }

    uint16_t port() const {
        return ntohs(raw.sin_port);
    }

    std::string toString() const {
        return ip() + ":" + std::to_string(port());
    }

    bool operator==(const Address& other) const {
        return raw.sin_addr.s_addr == other.raw.sin_addr.s_addr &&
               raw.sin_port == other.raw.sin_port;
    }

    bool operator!=(const Address& other) const {
        return !(*this == other);
    }

    struct Hash {
        size_t operator()(const Address& addr) const {
            return std::hash<uint32_t>()(addr.raw.sin_addr.s_addr) ^
                   (std::hash<uint16_t>()(addr.raw.sin_port) << 16);
        }
    };
};

} // namespace fishnet
