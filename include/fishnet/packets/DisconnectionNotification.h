#pragma once

/*
 * DisconnectionNotification (0x15)
 *
 * Sent to gracefully close a connection. Just 1 byte: [0x15].
 * Sent as reliable to ensure the other side receives it.
 */

#include <fishnet/protocol/Constants.h>
#include <fishnet/utils/BinaryBuffer.h>

namespace fishnet {

struct DisconnectionNotification {
    static BinaryBuffer encode() {
        BinaryBuffer buf(1);
        buf.writeU8(PacketId::DisconnectionNotification);
        return buf;
    }
};

} // namespace fishnet
