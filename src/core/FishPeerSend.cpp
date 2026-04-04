/*
 * FishPeer — Sending
 *
 * Raw UDP send, frame set construction with encapsulated packets,
 * reliable ordered sending, unreliable sending, and automatic
 * split sending for payloads larger than MTU.
 * Stores reliable datagrams for retransmission until ACK'd.
 */

#include "fishnet/FishPeer.h"
#include <algorithm>
#include <cstdio>

namespace fishnet {

void FishPeer::sendRaw(const uint8_t* data, size_t len, const Address& dest) {
    if (sock_ == InvalidSocket) return;

    #ifdef FISHNET_DEBUG
    if (len > 0) {
        std::printf("[FISHNET] OUT 0x%02X                              %zu bytes to   %s\n",
                    data[0], len, dest.toString().c_str());
    }
    #endif

    sendto(sock_, reinterpret_cast<const char*>(data), static_cast<int>(len), 0,
           reinterpret_cast<const sockaddr*>(&dest.raw), sizeof(sockaddr_in));
}

void FishPeer::sendEncapsulated(const Address& dest, const std::vector<EncapsulatedPacket>& packets) {
    BinaryBuffer out(mtu_);
    out.writeU8(0x84);

    uint32_t seq;
    {
        std::lock_guard<std::mutex> lock(sendMutex_);
        seq = outSeq_++;
    }
    out.writeU24LE(seq);

    for (const auto& ep : packets) {
        uint8_t flags = static_cast<uint8_t>((ep.reliability & 0x07) << 5);
        if (ep.isSplit) flags |= 0x10;
        out.writeU8(flags);

        uint16_t bitsLen = static_cast<uint16_t>(ep.payload.size() * 8);
        out.writeU16BE(bitsLen);

        if (reliabilityHasMessageIndex(ep.reliability)) out.writeU24LE(ep.messageIndex);
        if (reliabilityHasSequenceIndex(ep.reliability)) out.writeU24LE(ep.sequenceIndex);
        if (reliabilityHasOrderIndex(ep.reliability)) {
            out.writeU24LE(ep.orderIndex);
            out.writeU8(ep.orderChannel);
        }
        if (ep.isSplit) {
            out.writeU32BE(ep.splitCount);
            out.writeU16BE(ep.splitId);
            out.writeU32BE(ep.splitIndex);
        }

        out.writeBytes(ep.payload.data(), ep.payload.size());
        if (out.size() >= mtu_ - 50) break;
    }

    sendRaw(out.data(), out.size(), dest);

    bool hasReliable = false;
    for (auto& ep : packets) {
        if (reliabilityHasMessageIndex(ep.reliability)) { hasReliable = true; break; }
    }
    if (hasReliable) {
        std::lock_guard<std::mutex> lock(sendMutex_);
        if (pendingDatagrams_.size() < config_.maxPendingDatagrams) {
            pendingDatagrams_[seq] = {
                std::vector<uint8_t>(out.data(), out.data() + out.size()),
                dest, std::chrono::steady_clock::now(), 0
            };
        }
    }
}

void FishPeer::sendReliableOrdered(const uint8_t* data, size_t len, const Address& dest, uint8_t channel) {
    uint32_t maxPayloadPerFrame = mtu_ - 60;
    if (len > maxPayloadPerFrame) {
        sendSplitReliableOrdered(data, len, dest, channel);
        return;
    }

    EncapsulatedPacket ep;
    ep.reliability = static_cast<uint8_t>(Reliability::ReliableOrdered);
    ep.orderChannel = channel;
    {
        std::lock_guard<std::mutex> lock(sendMutex_);
        ep.messageIndex = outMsgIndex_++;
        ep.orderIndex = outOrderIndex_++;
    }
    ep.payload.assign(data, data + len);
    sendEncapsulated(dest, {ep});
}

void FishPeer::sendUnreliable(const uint8_t* data, size_t len, const Address& dest) {
    EncapsulatedPacket ep;
    ep.reliability = static_cast<uint8_t>(Reliability::Unreliable);
    ep.payload.assign(data, data + len);
    sendEncapsulated(dest, {ep});
}

void FishPeer::sendSplitReliableOrdered(const uint8_t* data, size_t len, const Address& dest, uint8_t channel) {
    uint32_t maxPayload = mtu_ - 4 - 20 - 36;
    if (maxPayload < 100) maxPayload = 100;

    uint32_t splitCount = static_cast<uint32_t>((len + maxPayload - 1) / maxPayload);

    uint16_t splitId;
    uint32_t baseOrderIndex;
    {
        std::lock_guard<std::mutex> lock(sendMutex_);
        splitId = outSplitId_++;
        baseOrderIndex = outOrderIndex_++;
    }

    for (uint32_t i = 0; i < splitCount; ++i) {
        size_t offset = static_cast<size_t>(i) * maxPayload;
        size_t chunkLen = std::min(static_cast<size_t>(maxPayload), len - offset);

        EncapsulatedPacket ep;
        ep.reliability = static_cast<uint8_t>(Reliability::ReliableOrdered);
        ep.orderChannel = channel;
        ep.isSplit = true;
        ep.splitCount = splitCount;
        ep.splitId = splitId;
        ep.splitIndex = i;
        {
            std::lock_guard<std::mutex> lock(sendMutex_);
            ep.messageIndex = outMsgIndex_++;
        }
        ep.orderIndex = baseOrderIndex;
        ep.payload.assign(data + offset, data + offset + chunkLen);
        sendEncapsulated(dest, {ep});
    }
}

} // namespace fishnet
