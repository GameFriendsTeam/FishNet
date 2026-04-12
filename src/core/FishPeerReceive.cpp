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
#include <cstdio>

namespace fishnet {

namespace {
constexpr uint32_t kMaxSplitParts = 4096;
}

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

        if (buf.remaining() < 2) return;
        const uint8_t lenB0 = buf.readU8();
        const uint8_t lenB1 = buf.readU8();
        const uint16_t bitsLenBE = (static_cast<uint16_t>(lenB0) << 8) |
                                    static_cast<uint16_t>(lenB1);
        const uint16_t bitsLenLE = static_cast<uint16_t>(lenB0) |
                                  (static_cast<uint16_t>(lenB1) << 8);
        size_t payloadLen = 0;

        uint32_t messageIndex = 0, sequenceIndex = 0, orderIndex = 0;
        uint8_t orderChannel = 0;

        if (reliabilityHasMessageIndex(reliability)) {
            if (buf.remaining() < 3) {
            #ifdef FISHNET_DEBUG
                std::printf("[FISHNET] PARSE drop: no messageIndex bytes, rel=%u from %s\n",
                            static_cast<unsigned>(reliability), sender.toString().c_str());
            #endif
                return;
            }
            messageIndex = buf.readU24LE();
        }
        if (reliabilityHasSequenceIndex(reliability)) {
            if (buf.remaining() < 3) {
            #ifdef FISHNET_DEBUG
                std::printf("[FISHNET] PARSE drop: no sequenceIndex bytes, rel=%u from %s\n",
                            static_cast<unsigned>(reliability), sender.toString().c_str());
            #endif
                return;
            }
            sequenceIndex = buf.readU24LE();
        }
        if (reliabilityHasOrderIndex(reliability)) {
            if (buf.remaining() < 4) {
            #ifdef FISHNET_DEBUG
                std::printf("[FISHNET] PARSE drop: no orderIndex bytes, rel=%u from %s\n",
                            static_cast<unsigned>(reliability), sender.toString().c_str());
            #endif
                return;
            }
            orderIndex = buf.readU24LE();
            orderChannel = buf.readU8();
            if (orderChannel >= 32) orderChannel = 31;
        }

        uint32_t splitCount = 0, splitIndex = 0;
        uint16_t splitId = 0;
        if (isSplit) {
            if (buf.remaining() < 10) {
            #ifdef FISHNET_DEBUG
                std::printf("[FISHNET] PARSE drop: no split bytes from %s\n",
                            sender.toString().c_str());
            #endif
                return;
            }

            uint8_t raw[10];
            for (int i = 0; i < 10; ++i) raw[i] = buf.readU8();

            auto readU16BEFrom = [](const uint8_t* p) -> uint16_t {
                return (static_cast<uint16_t>(p[0]) << 8) |
                        static_cast<uint16_t>(p[1]);
            };
            auto readU16LEFrom = [](const uint8_t* p) -> uint16_t {
                return static_cast<uint16_t>(p[0]) |
                      (static_cast<uint16_t>(p[1]) << 8);
            };
            auto readU32BEFrom = [](const uint8_t* p) -> uint32_t {
                return (static_cast<uint32_t>(p[0]) << 24) |
                       (static_cast<uint32_t>(p[1]) << 16) |
                       (static_cast<uint32_t>(p[2]) << 8) |
                        static_cast<uint32_t>(p[3]);
            };
            auto readU32LEFrom = [](const uint8_t* p) -> uint32_t {
                return static_cast<uint32_t>(p[0]) |
                      (static_cast<uint32_t>(p[1]) << 8) |
                      (static_cast<uint32_t>(p[2]) << 16) |
                      (static_cast<uint32_t>(p[3]) << 24);
            };

            const uint32_t beCount = readU32BEFrom(raw + 0);
            const uint32_t leCount = readU32LEFrom(raw + 0);
            const uint16_t beId = readU16BEFrom(raw + 4);
            const uint16_t leId = readU16LEFrom(raw + 4);
            const uint32_t beIndex = readU32BEFrom(raw + 6);
            const uint32_t leIndex = readU32LEFrom(raw + 6);

            const bool beLooksValid = beCount > 0 && beCount <= kMaxSplitParts && beIndex < beCount;
            const bool leLooksValid = leCount > 0 && leCount <= kMaxSplitParts && leIndex < leCount;

            if (beLooksValid || !leLooksValid) {
                splitCount = beCount;
                splitId = beId;
                splitIndex = beIndex;
            } else {
                splitCount = leCount;
                splitId = leId;
                splitIndex = leIndex;
            }

            if (splitCount == 0 || splitCount > kMaxSplitParts || splitIndex >= splitCount) {
            #ifdef FISHNET_DEBUG
                std::printf("[FISHNET] PARSE drop: bad split count=%u index=%u id=%u from %s\n",
                            static_cast<unsigned>(splitCount),
                            static_cast<unsigned>(splitIndex),
                            static_cast<unsigned>(splitId),
                            sender.toString().c_str());
            #endif
                return;
            }
        }

        const size_t payloadLenBE = (static_cast<size_t>(bitsLenBE) + 7) / 8;
        const size_t payloadLenLE = (static_cast<size_t>(bitsLenLE) + 7) / 8;
        const size_t availablePayload = buf.remaining();
        const bool beFits = payloadLenBE <= availablePayload;
        const bool leFits = payloadLenLE <= availablePayload;

        if (beFits || !leFits) {
            payloadLen = payloadLenBE;
        } else {
            payloadLen = payloadLenLE;
        }

        if (payloadLen > availablePayload) {
        #ifdef FISHNET_DEBUG
            std::printf("[FISHNET] PARSE drop: bitsLenBE=%u bitsLenLE=%u payload=%zu avail=%zu from %s\n",
                        static_cast<unsigned>(bitsLenBE),
                        static_cast<unsigned>(bitsLenLE),
                        payloadLen,
                        availablePayload,
                        sender.toString().c_str());
        #endif
            return;
        }
        auto payload = buf.readBytes(payloadLen);
        #ifdef FISHNET_DEBUG
        std::printf("[FISHNET] FRAME item seq=%u rel=%u split=%u msg=%u ord=%u ch=%u payload=%zu first=0x%02X from %s\n",
                    static_cast<unsigned>(datagramSeq),
                    static_cast<unsigned>(reliability),
                    isSplit ? 1u : 0u,
                    static_cast<unsigned>(messageIndex),
                    static_cast<unsigned>(orderIndex),
                    static_cast<unsigned>(orderChannel),
                    payload.size(),
                    payload.empty() ? 0u : static_cast<unsigned>(payload[0]),
                    sender.toString().c_str());
        #endif

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
            #ifdef FISHNET_DEBUG
            std::printf("[FISHNET] SPLIT state id=%u index=%u/%u received=%u from %s\n",
                        static_cast<unsigned>(splitId),
                        static_cast<unsigned>(splitIndex),
                        static_cast<unsigned>(splitCount),
                        static_cast<unsigned>(s.received),
                        sender.toString().c_str());
            #endif
            if (s.received == s.splitCount) {
                size_t total = 0;
                for (auto& v : s.parts) total += v.size();
                finalPayload.reserve(total);
                for (auto& v : s.parts) {
                    finalPayload.insert(finalPayload.end(), v.begin(), v.end());
                }
                #ifdef FISHNET_DEBUG
                std::printf("[FISHNET] SPLIT complete id=%u total=%zu from %s\n",
                            static_cast<unsigned>(splitId),
                            total,
                            sender.toString().c_str());
                #endif
                splitStore_.erase(key);
            } else {
                continue;
            }
        } else {
            finalPayload = std::move(payload);
        }

        // Ordering
        if (reliability == static_cast<uint8_t>(Reliability::ReliableOrdered)) {
            auto& peerOrdered = orderedStatesByPeer_[addrHash(sender)];
            auto& q = peerOrdered.channels[orderChannel];

            if (orderIndex == q.nextOrderIndex) {
                deliverInner(finalPayload.data(), finalPayload.size(), sender);
                q.nextOrderIndex++;

                while (!q.buffered.empty() && q.buffered.begin()->first == q.nextOrderIndex) {
                    auto payloadToDeliver = std::move(q.buffered.begin()->second);
                    q.buffered.erase(q.buffered.begin());
                    deliverInner(payloadToDeliver.data(), payloadToDeliver.size(), sender);
                    q.nextOrderIndex++;
                }
            } else if (orderIndex > q.nextOrderIndex) {
                q.buffered[orderIndex] = std::move(finalPayload);
            #ifdef FISHNET_DEBUG
                std::printf("[FISHNET] ORDER buffer ch=%u got=%u need=%u from %s\n",
                            static_cast<unsigned>(orderChannel),
                            static_cast<unsigned>(orderIndex),
                            static_cast<unsigned>(q.nextOrderIndex),
                            sender.toString().c_str());
            #endif
            } else {
            #ifdef FISHNET_DEBUG
                std::printf("[FISHNET] ORDER drop old ch=%u got=%u need=%u from %s\n",
                            static_cast<unsigned>(orderChannel),
                            static_cast<unsigned>(orderIndex),
                            static_cast<unsigned>(q.nextOrderIndex),
                            sender.toString().c_str());
            #endif
            }
        } else {
            deliverInner(finalPayload.data(), finalPayload.size(), sender);
        }
    }
}

} // namespace fishnet
