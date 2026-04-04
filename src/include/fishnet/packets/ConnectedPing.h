#pragma once

/*
 * ConnectedPing (0x00) / ConnectedPong (0x03)
 *
 * Keep-alive mechanism for established connections.
 * Ping:  [0x00] [timestamp BE u64]  — 9 bytes
 * Pong:  [0x03] [ping timestamp BE u64] [pong timestamp BE u64] — 17 bytes
 * Both should be sent as unreliable. If no ping/pong for ~10 seconds,
 * the connection is considered timed out.
 */

#pragma once

// ConnectedPing: 0x00 | timestamp(8)
// ConnectedPong: 0x03 | pingTimestamp(8) | pongTimestamp(8)

#include "fishnet/protocol/Constants.h"
#include "fishnet/utils/BinaryBuffer.h"

namespace fishnet {

struct ConnectedPing {
    uint64_t timestamp = 0;

    static BinaryBuffer encode(uint64_t timestamp) {
        BinaryBuffer buf(9);
        buf.writeU8(PacketId::ConnectedPing);
        buf.writeU64BE(timestamp);
        return buf;
    }

    static bool decode(const uint8_t* data, size_t len, ConnectedPing& out) {
        if (len < 9) return false;
        BinaryBuffer buf(data, len);
        buf.skip(1);
        out.timestamp = buf.readU64BE();
        return true;
    }
};

struct ConnectedPong {
    uint64_t pingTimestamp = 0;
    uint64_t pongTimestamp = 0;

    static BinaryBuffer encode(uint64_t pingTimestamp, uint64_t pongTimestamp) {
        BinaryBuffer buf(17);
        buf.writeU8(PacketId::ConnectedPong);
        buf.writeU64BE(pingTimestamp);
        buf.writeU64BE(pongTimestamp);
        return buf;
    }

    static bool decode(const uint8_t* data, size_t len, ConnectedPong& out) {
        if (len < 17) return false;
        BinaryBuffer buf(data, len);
        buf.skip(1);
        out.pingTimestamp = buf.readU64BE();
        out.pongTimestamp = buf.readU64BE();
        return true;
    }
};

} // namespace fishnet
