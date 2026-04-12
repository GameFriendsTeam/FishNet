#pragma once

/*
 * ACK (0xC0) / NAK (0xA0)
 *
 * Acknowledgement and negative-acknowledgement frames.
 * Sent after receiving frame sets to confirm or request retransmission.
 * Format: [ID] [record count BE u16] [records...]
 * Each record: [isSingle bool] [single u24 LE] or [first u24 LE] [last u24 LE]
 *
 * Also provides parseAckNak() to decode records into sequence number list.
 */

#include "fishnet/protocol/Constants.h"
#include "fishnet/utils/BinaryBuffer.h"
#include <algorithm>
#include <vector>

namespace fishnet {

struct AckPacket {
    // Encode a single ACK for one sequence number
    static BinaryBuffer encodeSingle(uint32_t sequenceNumber) {
        BinaryBuffer buf(7);
        buf.writeU8(PacketId::ACK);
        buf.writeU16BE(1);   // record count
        // RakNet boolean flag: 1 = single, 0 = range
        buf.writeU8(0x01);
        buf.writeU24LE(sequenceNumber);
        return buf;
    }

    // Encode an ACK range
    static BinaryBuffer encodeRange(uint32_t first, uint32_t last) {
        BinaryBuffer buf(10);
        buf.writeU8(PacketId::ACK);
        buf.writeU16BE(1);   // record count
        // RakNet boolean flag: 1 = single, 0 = range
        buf.writeU8(0x00);
        buf.writeU24LE(first);
        buf.writeU24LE(last);
        return buf;
    }
};

struct NakPacket {
    static BinaryBuffer encodeSingle(uint32_t sequenceNumber) {
        BinaryBuffer buf(7);
        buf.writeU8(PacketId::NAK);
        buf.writeU16BE(1);
        // RakNet boolean flag: 1 = single, 0 = range
        buf.writeU8(0x01);
        buf.writeU24LE(sequenceNumber);
        return buf;
    }
};

// Parse ACK/NAK records → list of acknowledged sequence numbers
inline std::vector<uint32_t> parseAckNak(const uint8_t* data, size_t len) {
    std::vector<uint32_t> sequences;
    if (len < 3) return sequences;

    BinaryBuffer buf(data, len);
    buf.skip(1); // packet id
    uint16_t recordCount = buf.readU16BE();

    for (uint16_t i = 0; i < recordCount && buf.remaining() >= 1; ++i) {
        uint8_t recordType = buf.readU8();

        // RakNet boolean flag: 1=single, 0=range.
        // Also keep compatibility with inverse/quirky senders.
        if (recordType == 0x01) {
            if (buf.remaining() < 3) break;
            sequences.push_back(buf.readU24LE());
            continue;
        }

        if (recordType == 0x00) {
            if (buf.remaining() >= 6) {
                uint32_t first = buf.readU24LE();
                uint32_t last  = buf.readU24LE();
                if (last < first) std::swap(first, last);
                // Safety cap to avoid pathological huge ranges.
                if (last - first > 4096) {
                    last = first + 4096;
                }
                for (uint32_t s = first; s <= last; ++s) {
                    sequences.push_back(s);
                }
            } else if (buf.remaining() >= 3) {
                // Compatibility fallback: treat as single.
                sequences.push_back(buf.readU24LE());
            }
            continue;
        }

        // Unknown type: last-chance single fallback.
        if (buf.remaining() >= 3) {
            sequences.push_back(buf.readU24LE());
        } else {
            break;
        }
    }
    return sequences;
}

} // namespace fishnet
