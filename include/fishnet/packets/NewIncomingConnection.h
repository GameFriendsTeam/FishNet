#pragma once

/*
 * NewIncomingConnection (0x13)
 *
 * Sent by client after receiving ConnectionRequestAccepted.
 * Completes the RakNet handshake — connection is now fully established.
 * Format: [ID] [server address 7] [10x internal addresses 7 each]
 *         [ping time BE u64] [pong time BE u64]
 */


// 0x13 | serverAddress(7) | internalAddresses(20 * 7) | pingTime(8) | pongTime(8)

#include <fishnet/protocol/Constants.h>
#include <fishnet/utils/BinaryBuffer.h>
#include <fishnet/utils/Address.h>
#include <fishnet/utils/AddressSerialization.h>

namespace fishnet {

struct NewIncomingConnection {
    Address serverAddress;
    uint64_t requestTimestamp = 0;
    uint64_t acceptedTimestamp = 0;

    static BinaryBuffer encode(const Address& serverAddr,
                               uint64_t requestTimestamp,
                               uint64_t acceptedTimestamp) {
        // 1(id) + 7(addr) + 10*7(internal) + 8(ping) + 8(pong) = 94
        BinaryBuffer buf(100);
        buf.writeU8(PacketId::NewIncomingConnection);
        writeAddress(buf, serverAddr);

        // 10 internal addresses (all empty/placeholder)
        for (int i = 0; i < 10; ++i) {
            writeEmptyAddress(buf);
        }

        buf.writeU64BE(requestTimestamp);
        buf.writeU64BE(acceptedTimestamp);
        return buf;
    }

    static bool decode(const uint8_t* data, size_t len, NewIncomingConnection& out) {
        if (len < 18) return false;
        BinaryBuffer buf(data, len);
        buf.skip(1);
        readAddress(buf, out.serverAddress);

        // Skip internal addresses, read timestamps from end
        if (buf.remaining() > 16) {
            buf.skip(buf.remaining() - 16);
        }
        if (buf.remaining() >= 16) {
            out.requestTimestamp = buf.readU64BE();
            out.acceptedTimestamp = buf.readU64BE();
        }
        return true;
    }
};

} // namespace fishnet
