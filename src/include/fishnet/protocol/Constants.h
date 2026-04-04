#pragma once

/*
 * FishNet Protocol Constants
 *
 * Core RakNet protocol definitions:
 *   - OFFLINE_MESSAGE_ID: 16-byte magic sequence for offline packets
 *   - DEFAULT_MTU: default maximum transmission unit (1464)
 *   - Reliability enum: Unreliable, UnreliableSequenced, Reliable,
 *     ReliableOrdered, ReliableSequenced
 *   - PacketId namespace: all RakNet packet IDs (0x00-0x1C, 0x80-0x8D,
 *     0xC0 ACK, 0xA0 NAK)
 *   - Helper functions: reliabilityHasMessageIndex/SequenceIndex/OrderIndex
 */

#include <cstdint>

namespace fishnet {

// RakNet offline magic (16 bytes)
constexpr uint8_t OFFLINE_MESSAGE_ID[16] = {
    0x00, 0xFF, 0xFF, 0x00, 0xFE, 0xFE, 0xFE, 0xFE,
    0xFD, 0xFD, 0xFD, 0xFD, 0x12, 0x34, 0x56, 0x78
};

constexpr uint32_t DEFAULT_MTU = 1464;

// Reliability types
enum class Reliability : uint8_t {
    Unreliable          = 0,
    UnreliableSequenced = 1,
    Reliable            = 2,
    ReliableOrdered     = 3,
    ReliableSequenced   = 7
};

inline bool reliabilityHasMessageIndex(uint8_t r) {
    return r == 2 || r == 3 || r == 4 || r == 6 || r == 7;
}

inline bool reliabilityHasSequenceIndex(uint8_t r) {
    return r == 1 || r == 7;
}

inline bool reliabilityHasOrderIndex(uint8_t r) {
    return r == 3 || r == 4 || r == 7;
}

// Offline packet IDs
namespace PacketId {
    constexpr uint8_t UnconnectedPing         = 0x01;
    constexpr uint8_t UnconnectedPingOpenConn = 0x02;
    constexpr uint8_t UnconnectedPong         = 0x1C;

    constexpr uint8_t OpenConnectionRequest1  = 0x05;
    constexpr uint8_t OpenConnectionReply1    = 0x06;
    constexpr uint8_t OpenConnectionRequest2  = 0x07;
    constexpr uint8_t OpenConnectionReply2    = 0x08;

    constexpr uint8_t ConnectionRequest       = 0x09;
    constexpr uint8_t ConnectionRequestAccepted = 0x10;
    constexpr uint8_t NewIncomingConnection    = 0x13;
    constexpr uint8_t DisconnectionNotification = 0x15;

    constexpr uint8_t ConnectedPing           = 0x00;
    constexpr uint8_t ConnectedPong           = 0x03;

    // ACK / NAK
    constexpr uint8_t ACK = 0xC0;
    constexpr uint8_t NAK = 0xA0;

    // Frame set range (data frames)
    constexpr uint8_t FrameSetMin = 0x80;
    constexpr uint8_t FrameSetMax = 0x8D;

    inline bool isFrameSet(uint8_t id) {
        return id >= FrameSetMin && id <= FrameSetMax;
    }
}

} // namespace fishnet
