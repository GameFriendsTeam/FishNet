#pragma once

/*
 * ConnectionRequest (0x09)
 *
 * Sent inside a frame set after the offline handshake completes.
 * Format: [ID] [client GUID BE u64] [timestamp BE u64] [security bool]
 * Server responds with ConnectionRequestAccepted (0x10).
 */

#pragma once

// 0x09 | clientGuid(8) | requestTimestamp(8) | useSecurity(1)

#include "fishnet/protocol/Constants.h"
#include "fishnet/utils/BinaryBuffer.h"

namespace fishnet {

struct ConnectionRequest {
    uint64_t clientGuid = 0;
    uint64_t requestTimestamp = 0;
    bool useSecurity = false;

    static bool decode(const uint8_t* data, size_t len, ConnectionRequest& out) {
        if (len < 17) return false;
        BinaryBuffer buf(data, len);
        buf.skip(1); // id
        out.clientGuid = buf.readU64BE();
        out.requestTimestamp = buf.readU64BE();
        if (buf.remaining() >= 1) out.useSecurity = buf.readU8() != 0;
        return true;
    }

    static BinaryBuffer encode(uint64_t clientGuid, uint64_t timestamp, bool useSecurity) {
        BinaryBuffer buf(18);
        buf.writeU8(PacketId::ConnectionRequest);
        buf.writeU64BE(clientGuid);
        buf.writeU64BE(timestamp);
        buf.writeU8(useSecurity ? 1 : 0);
        return buf;
    }
};

} // namespace fishnet
