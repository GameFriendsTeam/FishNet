#pragma once

/*
 * OpenConnectionRequest1 (0x05)
 *
 * First step of the connection handshake, sent by client.
 * Format: [ID] [magic 16] [protocol version u8] [zero padding...]
 * The total packet size determines MTU. Server calculates MTU = packetLen + 28
 * (IP header 20 + UDP header 8). Client sends with decreasing padding until
 * server responds with OpenConnectionReply1.
 */

#include <fishnet/protocol/Constants.h>
#include <fishnet/utils/BinaryBuffer.h>
#include <cstring>

namespace fishnet {

struct OpenConnectionRequest1 {
    uint8_t protocolVersion = 0;
    uint16_t mtuSize = DEFAULT_MTU;

    static bool decode(const uint8_t* data, size_t len, OpenConnectionRequest1& out) {
        if (len < 18) return false;
        if (std::memcmp(data + 1, OFFLINE_MESSAGE_ID, 16) != 0) return false;
        out.protocolVersion = data[17];
        // MTU = total packet length + 28 (IP header 20 + UDP header 8)
        out.mtuSize = static_cast<uint16_t>(len + 28);
        return true;
    }

    static BinaryBuffer encode(uint8_t protocolVersion, uint16_t mtuSize) {
        // Packet size on wire should be mtuSize - 28 (IP+UDP overhead)
        uint16_t packetSize = (mtuSize > 28) ? (mtuSize - 28) : 18;
        BinaryBuffer buf(packetSize);
        buf.writeU8(PacketId::OpenConnectionRequest1);
        buf.writeBytes(OFFLINE_MESSAGE_ID, 16);
        buf.writeU8(protocolVersion);
        // Pad with zeros to reach desired packet size
        while (buf.size() < packetSize) {
            buf.writeU8(0x00);
        }
        return buf;
    }
};

} // namespace fishnet
