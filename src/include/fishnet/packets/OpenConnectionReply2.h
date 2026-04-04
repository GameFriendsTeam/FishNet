#pragma once

/*
 * OpenConnectionReply2 (0x08)
 *
 * Final offline handshake packet, server → client.
 * Format: [ID] [magic 16] [server GUID BE u64] [client address 7] [MTU BE u16] [encryption bool]
 * After this, all communication happens inside Frame Set packets (0x80-0x8D).
 */

#pragma once

// 0x08 | magic(16) | serverGuid(8) | clientAddress(7) | mtuSize(2) | security(1)

#include "fishnet/protocol/Constants.h"
#include "fishnet/utils/BinaryBuffer.h"
#include "fishnet/utils/Address.h"
#include "fishnet/utils/AddressSerialization.h"

namespace fishnet {

struct OpenConnectionReply2 {
    uint64_t serverGuid = 0;
    Address clientAddress;
    uint16_t mtuSize = DEFAULT_MTU;
    bool encryptionEnabled = false;

    static BinaryBuffer encode(uint64_t serverGuid, const Address& clientAddr,
                               uint16_t mtuSize, bool encryption) {
        BinaryBuffer buf(35);
        buf.writeU8(PacketId::OpenConnectionReply2);
        buf.writeBytes(OFFLINE_MESSAGE_ID, 16);
        buf.writeU64BE(serverGuid);
        writeAddress(buf, clientAddr);
        buf.writeU16BE(mtuSize);
        buf.writeU8(encryption ? 1 : 0);
        return buf;
    }

    static bool decode(const uint8_t* data, size_t len, OpenConnectionReply2& out) {
        if (len < 35) return false;
        BinaryBuffer buf(data, len);
        buf.skip(1);  // id
        buf.skip(16); // magic
        out.serverGuid = buf.readU64BE();
        readAddress(buf, out.clientAddress);
        out.mtuSize = buf.readU16BE();
        if (buf.remaining() >= 1) out.encryptionEnabled = buf.readU8() != 0;
        return true;
    }
};

} // namespace fishnet
