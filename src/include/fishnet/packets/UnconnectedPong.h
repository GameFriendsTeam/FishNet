#pragma once

/*
 * UnconnectedPong (0x1C)
 *
 * Server response to UnconnectedPing. Contains server info string.
 * Format: [ID] [ping timestamp BE u64] [server GUID BE u64] [magic 16] [string]
 * The string content is user-defined (for Bedrock: MCPE;name;proto;ver;...)
 */

#pragma once

#include "fishnet/protocol/Constants.h"
#include "fishnet/utils/BinaryBuffer.h"
#include <string>

namespace fishnet {

struct UnconnectedPong {
    uint64_t pingTimestamp = 0;
    uint64_t serverGuid = 0;
    std::string serverData;

    // 0x1C | pingTimestamp(8) | serverGuid(8) | magic(16) | stringLen(2) | string
    static BinaryBuffer encode(uint64_t pingTimestamp, uint64_t serverGuid,
                               const std::string& serverData) {
        BinaryBuffer buf(35 + serverData.size());
        buf.writeU8(PacketId::UnconnectedPong);
        buf.writeU64BE(pingTimestamp);
        buf.writeU64BE(serverGuid);
        buf.writeBytes(OFFLINE_MESSAGE_ID, 16);
        buf.writeString(serverData);
        return buf;
    }

    static bool decode(const uint8_t* data, size_t len, UnconnectedPong& out) {
        if (len < 35) return false;
        BinaryBuffer buf(data, len);
        buf.skip(1);
        out.pingTimestamp = buf.readU64BE();
        out.serverGuid = buf.readU64BE();
        buf.skip(16); // magic
        if (buf.remaining() >= 2) out.serverData = buf.readString();
        return true;
    }
};

} // namespace fishnet
