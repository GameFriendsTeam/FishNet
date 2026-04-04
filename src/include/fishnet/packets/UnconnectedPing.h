#pragma once

/*
 * UnconnectedPing (0x01 / 0x02)
 *
 * Sent by clients to discover servers on LAN or query server status.
 * Format: [ID] [timestamp BE u64] [magic 16] [client GUID BE u64]
 * Server should respond with UnconnectedPong (0x1C).
 */

#include "fishnet/protocol/Constants.h"
#include "fishnet/utils/BinaryBuffer.h"
#include <cstring>

namespace fishnet {

struct UnconnectedPing {
    uint64_t clientTimestamp = 0;
    uint64_t clientGuid = 0;

    static bool decode(const uint8_t* data, size_t len, UnconnectedPing& out) {
        if (len < 25) return false;
        // Byte 0 = packet id (0x01)
        // Bytes 1-8 = timestamp
        // Bytes 9-24 = magic
        if (std::memcmp(data + 9, OFFLINE_MESSAGE_ID, 16) != 0) return false;

        BinaryBuffer buf(data, len);
        buf.skip(1); // id
        out.clientTimestamp = buf.readU64BE();
        buf.skip(16); // magic
        if (buf.remaining() >= 8) {
            out.clientGuid = buf.readU64BE();
        }
        return true;
    }
};

} // namespace fishnet
