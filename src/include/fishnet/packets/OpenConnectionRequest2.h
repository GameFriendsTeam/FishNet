#pragma once

/*
 * OpenConnectionRequest2 (0x07)
 *
 * Second step of handshake, client → server.
 * Format: [ID] [magic 16] [server address 7] [MTU BE u16] [client GUID BE u64]
 * Address uses RakNet wire format (inverted IP bytes).
 */

#pragma once

// 0x07 | magic(16) | serverAddress(7) | mtuSize(2) | clientGuid(8)

#include "fishnet/protocol/Constants.h"
#include "fishnet/utils/BinaryBuffer.h"
#include "fishnet/utils/Address.h"
#include "fishnet/utils/AddressSerialization.h"
#include <cstring>

namespace fishnet {

struct OpenConnectionRequest2 {
    Address serverAddress;
    uint16_t mtuSize = DEFAULT_MTU;
    uint64_t clientGuid = 0;

    static bool decode(const uint8_t* data, size_t len, OpenConnectionRequest2& out) {
        if (len < 34) return false;
        if (std::memcmp(data + 1, OFFLINE_MESSAGE_ID, 16) != 0) return false;

        BinaryBuffer buf(data, len);
        buf.skip(1);  // id
        buf.skip(16); // magic
        readAddress(buf, out.serverAddress);
        out.mtuSize = buf.readU16BE();
        out.clientGuid = buf.readU64BE();
        return true;
    }

    static BinaryBuffer encode(const Address& serverAddr, uint16_t mtuSize, uint64_t clientGuid) {
        BinaryBuffer buf(34);
        buf.writeU8(PacketId::OpenConnectionRequest2);
        buf.writeBytes(OFFLINE_MESSAGE_ID, 16);
        writeAddress(buf, serverAddr);
        buf.writeU16BE(mtuSize);
        buf.writeU64BE(clientGuid);
        return buf;
    }
};

} // namespace fishnet
