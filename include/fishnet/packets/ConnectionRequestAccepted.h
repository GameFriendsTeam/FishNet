#pragma once

/*
 * ConnectionRequestAccepted (0x10)
 *
 * Server confirms the connection request.
 * Format: [ID] [client address 7] [system index BE u16]
 *         [10x system addresses 7 each] [ping time BE u64] [pong time BE u64]
 * Total: 96 bytes. Client responds with NewIncomingConnection (0x13).
 */


// 0x10 | clientAddress(7) | systemIndex(2) | systemAddresses(20 * 7) | pingTime(8) | pongTime(8)

#include <fishnet/protocol/Constants.h>
#include <fishnet/utils/BinaryBuffer.h>
#include <fishnet/utils/Address.h>
#include <fishnet/utils/AddressSerialization.h>

namespace fishnet {

struct ConnectionRequestAccepted {
    Address clientAddress;
    uint16_t systemIndex = 0;
    uint64_t requestTimestamp = 0;
    uint64_t acceptedTimestamp = 0;

    static BinaryBuffer encode(const Address& clientAddr, uint64_t requestTimestamp,
                               uint64_t acceptedTimestamp) {
        // 1(id) + 7(addr) + 2(sysIdx) + 10*7(sysAddrs) + 8(ping) + 8(pong) = 96
        BinaryBuffer buf(100);
        buf.writeU8(PacketId::ConnectionRequestAccepted);
        writeAddress(buf, clientAddr);
        buf.writeU16BE(0); // system index

        // 10 system addresses (all empty/placeholder)
        for (int i = 0; i < 10; ++i) {
            writeEmptyAddress(buf);
        }

        buf.writeU64BE(requestTimestamp);
        buf.writeU64BE(acceptedTimestamp);
        return buf;
    }

    static bool decode(const uint8_t* data, size_t len, ConnectionRequestAccepted& out) {
        if (len < 20) return false;
        BinaryBuffer buf(data, len);
        buf.skip(1); // id
        readAddress(buf, out.clientAddress);
        out.systemIndex = buf.readU16BE();

        // Skip system addresses (20 * 7 = 140 bytes, but some may be IPv6)
        // Just skip until we have 16 bytes left for timestamps
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
