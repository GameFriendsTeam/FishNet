/*
 * FishPeer — Receiving
 *
 * Frame set parsing: extracts encapsulated packets from data frames.
 * Handles ACK (remove from pending), NAK (retransmit immediately).
 * Split reassembly: collects fragments, delivers when complete.
 * Ordered delivery: buffers out-of-order packets per channel.
 * NAK detection: sends NAK for gaps in received sequence numbers.
 */

#include "fishnet/FishPeer.h"
#include "fishnet/packets/AckNak.h"

namespace fishnet {

void FishPeer::sendAck(const Address& dest, uint32_t seq) {
    auto ack = AckPacket::encodeSingle(seq);
    sendRaw(ack.data(), ack.size(), dest);
}

void FishPeer::handleAck(const uint8_t* data, size_t len) {
    auto sequences = parseAckNak(data, len);
    std::lock_guard<std::mutex> lock(sendMutex_);
    for (uint32_t seq : sequences) {
        pendingDatagrams_.erase(seq);
    }
}

void FishPeer::handleNak(const uint8_t* data, size_t len, const Address& sender) {
    auto sequences = parseAckNak(data, len);
    std::lock_guard<std::mutex> lock(sendMutex_);
    for (uint32_t seq : sequences) {
        auto it = pendingDatagrams_.find(seq);
        if (it != pendingDatagrams_.end()) {
            sendRaw(it->second.bytes.data(), it->second.bytes.size(), sender);
            it->second.sentAt = std::chrono::steady_clock::now();
            it->second.retransmitCount++;
        }
    }
}

void FishPeer::handleFrameSet(const uint8_t* data, size_t len, const Address& sender) {
    if (len < 4) return;

    BinaryBuffer buf(data, len);
    buf.skip(1);
    uint32_t datagramSeq = buf.readU24LE();
    sendAck(sender, datagramSeq);

    // NAK detection: check for sequence gaps
    {
        uint64_t ah = addrHash(sender);
        auto& state = peerRecvState_[ah];
        if (datagramSeq > state.expectedSeq + 1 && state.expectedSeq > 0) {
            for (uint32_t missing = state.expectedSeq; missing < datagramSeq; ++missing) {
                auto nak = NakPacket::encodeSingle(missing);
                sendRaw(nak.data(), nak.size(), sender);
            }
        }
        if (datagramSeq >= state.expectedSeq) {
            state.expectedSeq = datagramSeq + 1;
        }
    }

    while (buf.remaining() >= 3) {
        uint8_t flags = buf.readU8();
        uint8_t reliability = (flags >> 5) & 0x07;
        bool isSplit = (flags & 0x10) != 0;

        uint16_t bitsLen = buf.readU16BE();
        size_t payloadLen = (bitsLen + 7) / 8;

        uint32_t messageIndex = 0, sequenceIndex = 0, orderIndex = 0;
        uint8_t orderChannel = 0;

        if (reliabilityHasMessageIndex(reliability)) {
            if (buf.remaining() < 3) return;
            messageIndex = buf.readU24LE();
        }
        if (reliabilityHasSequenceIndex(reliability)) {
            if (buf.remaining() < 3) return;
            sequenceIndex = buf.readU24LE();
        }
        if (reliabilityHasOrderIndex(reliability)) {
            if (buf.remaining() < 4) return;
            orderIndex = buf.readU24LE();
            orderChannel = buf.readU8();
            if (orderChannel >= 32) orderChannel = 31;
        }

        uint32_t splitCount = 0, splitIndex = 0;
        uint16_t splitId = 0;
        if (isSplit) {
            if (buf.remaining() < 10) return;
            splitCount = buf.readU32BE();
            splitId = buf.readU16BE();
            splitIndex = buf.readU32BE();
        }

        if (buf.remaining() < payloadLen) return;
        auto payload = buf.readBytes(payloadLen);

        // Split reassembly
        std::vector<uint8_t> finalPayload;
        if (isSplit) {
            std::lock_guard<std::mutex> lock(splitMutex_);
            SplitKey key{sender.raw.sin_addr.s_addr, ntohs(sender.raw.sin_port), splitId};
            auto& s = splitStore_[key];
            if (s.parts.empty()) {
                s.splitCount = splitCount;
                s.parts.resize(splitCount);
                s.createdAt = std::chrono::steady_clock::now();
            }
            if (splitIndex < s.parts.size() && s.parts[splitIndex].empty()) {
                s.parts[splitIndex] = std::move(payload);
                s.received++;
            }
            if (s.received == s.splitCount) {
                size_t total = 0;
                for (auto& v : s.parts) total += v.size();
                finalPayload.reserve(total);
                for (auto& v : s.parts) {
                    finalPayload.insert(finalPayload.end(), v.begin(), v.end());
                }
                splitStore_.erase(key);
            } else {
                continue;
            }
        } else {
            finalPayload = std::move(payload);
        }

        // Ordering
        if (reliability == static_cast<uint8_t>(Reliability::ReliableOrdered)) {
            auto& q = orderedQueues_[orderChannel];
            if (orderIndex == q.nextOrderIndex) {
                deliverInner(finalPayload.data(), finalPayload.size(), sender);
                q.nextOrderIndex++;
                while (!q.buffered.empty() && q.buffered.begin()->first == q.nextOrderIndex) {
                    auto& p = q.buffered.begin()->second;
                    deliverInner(p.data(), p.size(), sender);
                    q.buffered.erase(q.buffered.begin());
                    q.nextOrderIndex++;
                }
            } else if (orderIndex > q.nextOrderIndex) {
                q.buffered[orderIndex] = std::move(finalPayload);
            }
        } else {
            deliverInner(finalPayload.data(), finalPayload.size(), sender);
        }
    }
}

} // namespace fishnet
