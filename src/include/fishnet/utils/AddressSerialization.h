#pragma once

/*
 * FishNet Address Serialization
 *
 * RakNet encodes addresses in packets with a specific wire format:
 *   IPv4: [0x04] [~IP0] [~IP1] [~IP2] [~IP3] [port BE u16]
 * The IP bytes are bitwise-inverted (NOT). Total: 7 bytes.
 *
 * Provides: writeAddress(), readAddress(), writeEmptyAddress()
 * Used by all handshake packets (OpenConnection, ConnectionRequestAccepted, etc).
 */

#include "fishnet/utils/BinaryBuffer.h"
#include "fishnet/utils/Address.h"
#include <cstring>

namespace fishnet {

/// Write an Address in RakNet wire format (7 bytes for IPv4)
inline void writeAddress(BinaryBuffer& buf, const Address& addr) {
    buf.writeU8(0x04); // IPv4

    // RakNet inverts (bitwise NOT) the IP bytes
    uint32_t ip = addr.raw.sin_addr.s_addr; // already network byte order
    auto* ipBytes = reinterpret_cast<const uint8_t*>(&ip);
    buf.writeU8(~ipBytes[0]);
    buf.writeU8(~ipBytes[1]);
    buf.writeU8(~ipBytes[2]);
    buf.writeU8(~ipBytes[3]);

    // Port is big-endian
    buf.writeU16BE(ntohs(addr.raw.sin_port));
}

/// Read an Address in RakNet wire format. Returns true on success.
inline bool readAddress(BinaryBuffer& buf, Address& out) {
    if (buf.remaining() < 7) return false;

    uint8_t version = buf.readU8();
    if (version == 0x04) {
        // IPv4: inverted bytes
        sockaddr_in addr{};
        addr.sin_family = AF_INET;

        uint8_t ip[4];
        ip[0] = ~buf.readU8();
        ip[1] = ~buf.readU8();
        ip[2] = ~buf.readU8();
        ip[3] = ~buf.readU8();
        std::memcpy(&addr.sin_addr.s_addr, ip, 4);

        addr.sin_port = htons(buf.readU16BE());
        out = Address(addr);
        return true;
    } else if (version == 0x06) {
        // IPv6: skip (not implemented, but don't crash)
        // family(2) + port(2) + flow(4) + addr(16) + scope(4) = 28 bytes after version
        if (buf.remaining() < 28) return false;
        buf.skip(28);
        // Return a dummy address
        out = Address();
        return true;
    }
    return false;
}

/// Write a placeholder/empty address (used for internal address slots)
inline void writeEmptyAddress(BinaryBuffer& buf) {
    buf.writeU8(0x04); // IPv4
    buf.writeU8(0xFF); // ~0 = 0xFF (0.0.0.0 inverted)
    buf.writeU8(0xFF);
    buf.writeU8(0xFF);
    buf.writeU8(0xFF);
    buf.writeU16BE(0);
}

} // namespace fishnet
