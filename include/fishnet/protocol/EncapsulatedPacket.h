#pragma once

/*
 * FishNet Encapsulated Packet
 *
 * Internal frame structure carried inside data frames (0x80-0x8D).
 * Each frame contains: reliability flags, optional message/sequence/order
 * indices, optional split metadata, and the actual payload bytes.
 * Multiple encapsulated packets can be packed into one frame set.
 */

#include <cstdint>
#include <vector>

namespace fishnet {

struct EncapsulatedPacket {
    uint8_t reliability = 0;
    bool isSplit = false;
    uint32_t messageIndex = 0;
    uint32_t sequenceIndex = 0;
    uint32_t orderIndex = 0;
    uint8_t orderChannel = 0;
    uint32_t splitCount = 0;
    uint16_t splitId = 0;
    uint32_t splitIndex = 0;

    std::vector<uint8_t> payload;
};

} // namespace fishnet
