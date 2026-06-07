#pragma once

/*
 * OpenConnectionReply1 (0x06)
 *
 * Server response to OpenConnectionRequest1.
 * Format: [ID] [magic 16] [server GUID BE u64] [security bool] [MTU BE u16]
 * If security is enabled, a cookie follows (not implemented).
 */


// 0x06 | magic(16) | serverGuid(8) | useSecurity(1) | mtuSize(2)
// Total: 28 bytes (no cookie when useSecurity=false)

#include <fishnet/protocol/Constants.h>
#include <fishnet/utils/BinaryBuffer.h>

namespace fishnet {

struct OpenConnectionReply1 {
    uint64_t serverGuid = 0;
    bool useSecurity = false;
    uint16_t mtuSize = DEFAULT_MTU;

    static BinaryBuffer encode(uint64_t serverGuid, bool useSecurity, uint16_t mtuSize) {
        BinaryBuffer buf(28);
        buf.writeU8(PacketId::OpenConnectionReply1);
        buf.writeBytes(OFFLINE_MESSAGE_ID, 16);
        buf.writeU64BE(serverGuid);
        buf.writeU8(useSecurity ? 1 : 0);
        buf.writeU16BE(mtuSize);
        return buf;
    }

    static bool decode(const uint8_t* data, size_t len, OpenConnectionReply1& out) {
        if (len < 28) return false;
        BinaryBuffer buf(data, len);
        buf.skip(1);  // id
        buf.skip(16); // magic
        out.serverGuid = buf.readU64BE();
        out.useSecurity = buf.readU8() != 0;
        out.mtuSize = buf.readU16BE();
        return true;
    }
};

} // namespace fishnet
