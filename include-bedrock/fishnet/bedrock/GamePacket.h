#pragma once

/*
 * Bedrock GamePacket (0xFE)
 *
 * After the RakNet handshake, all Minecraft Bedrock protocol packets
 * are wrapped in a GamePacket frame:
 *   [0xFE] [compression_id u8] [compressed batch payload]
 *
 * The batch payload contains one or more sub-packets:
 *   [varint length] [varint packet_id] [packet data] ...
 *
 * Compression: 0x00=Zlib, 0x01=Snappy, 0xFF=None.
 * Compress/decompress functions are user-supplied via function pointers
 * so FishNet has zero compression library dependencies.
 *
 * Provides: GamePacket::wrap() to batch+compress outgoing sub-packets,
 *           GamePacket::unwrap() to decompress+split incoming 0xFE frames.
 *           VarInt helpers, SubPacket struct, CompressionMethod enum.
 */

#include <fishnet/platform.h>
#include <fishnet/utils/BinaryBuffer.h>
#include <cstdint>
#include <vector>
#include <string>

namespace fishnet::bedrock {

// Constants

constexpr uint8_t GAME_PACKET_ID = 0xFE;

enum class CompressionMethod : uint8_t {
    Zlib       = 0x00,
    Snappy     = 0x01,
    None       = 0xFF
};

enum class GamePacketFormat : uint8_t {
    Standard,                 // [0xFE][compression][batch]
    PreLoginNoCompression,    // [0xFE][batch]
    Auto                      // Try Standard first, then PreLogin
};

// VarInt helpers

/// Write an unsigned varint to a buffer. Returns bytes written.
inline size_t writeVarInt(std::vector<uint8_t>& out, uint32_t value) {
    size_t written = 0;
    do {
        uint8_t byte = value & 0x7F;
        value >>= 7;
        if (value != 0) byte |= 0x80;
        out.push_back(byte);
        written++;
    } while (value != 0);
    return written;
}

/// Read an unsigned varint from data at offset. Advances offset.
/// Returns 0 on error (offset out of bounds).
inline uint32_t readVarInt(const uint8_t* data, size_t len, size_t& offset) {
    uint32_t result = 0;
    int shift = 0;
    do {
        if (offset >= len) return 0;
        uint8_t byte = data[offset++];
        result |= static_cast<uint32_t>(byte & 0x7F) << shift;
        if ((byte & 0x80) == 0) break;
        shift += 7;
        if (shift >= 35) return 0; // Too many bytes
    } while (true);
    return result;
}

// Sub-packet structure

/// A single Bedrock sub-packet inside a GamePacket batch
struct SubPacket {
    uint32_t packetId = 0;
    uint8_t senderSubClient = 0;
    uint8_t targetSubClient = 0;
    std::vector<uint8_t> payload; // Raw packet data (without the varint ID prefix)
};

// GamePacket encode/decode

struct FISHNET_API GamePacket {

    /// Encode sub-packets into a complete 0xFE game packet.
    ///
    /// @param subPackets   List of sub-packets to batch together
    /// @param method       Compression method (default: None)
    /// @param compressor   Optional external compression function.
    ///                     Signature: bool(const uint8_t* in, size_t inLen,
    ///                                     std::vector<uint8_t>& out)
    ///                     If nullptr and method != None, data is sent uncompressed.
    ///
    /// @return Complete packet ready to send via FishNet (starts with 0xFE)
    ///
    /// Usage example:
    /// ```
    ///   SubPacket login;
    ///   login.packetId = 0x01; // LoginPacket
    ///   login.payload = {...};
    ///
    ///   auto frame = GamePacket::wrap({login}, CompressionMethod::None);
    ///   server.sendReliableTo(frame.data(), frame.size(), client);
    /// ```
    using CompressFunc = bool(*)(const uint8_t* in, size_t inLen, std::vector<uint8_t>& out);

    static std::vector<uint8_t> wrap(
        const std::vector<SubPacket>& subPackets,
        CompressionMethod method = CompressionMethod::None,
        CompressFunc compressor = nullptr,
        bool includeCompressionHeader = true)
    {
        // 1. Build the uncompressed batch payload
        std::vector<uint8_t> batch;
        batch.reserve(1024);

        for (const auto& sp : subPackets) {
            uint32_t packetHeader = (sp.packetId & 0x3FFu) |
                                    ((static_cast<uint32_t>(sp.senderSubClient) & 0x3u) << 10) |
                                    ((static_cast<uint32_t>(sp.targetSubClient) & 0x3u) << 12);

            std::vector<uint8_t> headerBytes;
            writeVarInt(headerBytes, packetHeader);

            uint32_t totalLen = static_cast<uint32_t>(headerBytes.size() + sp.payload.size());
            writeVarInt(batch, totalLen);
            batch.insert(batch.end(), headerBytes.begin(), headerBytes.end());
            batch.insert(batch.end(), sp.payload.begin(), sp.payload.end());
        }

        // 2. Pre-login format has no compression byte
        if (!includeCompressionHeader) {
            std::vector<uint8_t> preLogin;
            preLogin.reserve(1 + batch.size());
            preLogin.push_back(GAME_PACKET_ID);
            preLogin.insert(preLogin.end(), batch.begin(), batch.end());
            return preLogin;
        }

        // 3. Compress if requested
        std::vector<uint8_t> compressed;
        bool didCompress = false;

        if (method != CompressionMethod::None && compressor != nullptr) {
            didCompress = compressor(batch.data(), batch.size(), compressed);
        }

        // 4. Build final 0xFE frame
        std::vector<uint8_t> result;
        result.reserve(2 + (didCompress ? compressed.size() : batch.size()));
        result.push_back(GAME_PACKET_ID);
        result.push_back(static_cast<uint8_t>(didCompress ? method : CompressionMethod::None));

        if (didCompress) {
            result.insert(result.end(), compressed.begin(), compressed.end());
        } else {
            result.insert(result.end(), batch.begin(), batch.end());
        }

        return result;
    }

    /// Decode a 0xFE game packet into sub-packets.
    ///
    /// @param data         Raw packet data (must start with 0xFE)
    /// @param len          Length of data
    /// @param out          Output vector of decoded sub-packets
    /// @param decompressor Optional external decompression function.
    ///                     Signature: bool(const uint8_t* in, size_t inLen,
    ///                                     std::vector<uint8_t>& out)
    ///                     Required if compression != None.
    ///
    /// @return true on success, false on malformed data
    ///
    /// Usage example:
    /// ```
    ///   // In your packet callback:
    ///   if (data[0] == fishnet::bedrock::GAME_PACKET_ID) {
    ///       std::vector<SubPacket> packets;
    ///       GamePacket::unwrap(data, len, packets, myZlibDecompress);
    ///       for (auto& pkt : packets) {
    ///           handleBedrockPacket(pkt.packetId, pkt.payload);
    ///       }
    ///   }
    /// ```
    using DecompressFunc = bool(*)(const uint8_t* in, size_t inLen, std::vector<uint8_t>& out);

    static bool unwrap(
        const uint8_t* data, size_t len,
        std::vector<SubPacket>& out,
        DecompressFunc decompressor = nullptr,
        GamePacketFormat format = GamePacketFormat::Auto,
        CompressionMethod* outMethod = nullptr)
    {
        if (len < 2) return false;
        if (data[0] != GAME_PACKET_ID) return false;

        out.clear();

        auto parseBatch = [](const uint8_t* batchData, size_t batchLen, std::vector<SubPacket>& outPackets) {
            size_t offset = 0;
            while (offset < batchLen) {
                uint32_t totalLen = readVarInt(batchData, batchLen, offset);
                if (totalLen == 0 || offset + totalLen > batchLen) return false;

                size_t subStart = offset;
                size_t subEnd = subStart + totalLen;
                uint32_t packetHeader = readVarInt(batchData, subEnd, offset);
                if (offset <= subStart || offset > subEnd) return false;

                SubPacket sp;
                sp.packetId = packetHeader & 0x3FFu;
                sp.senderSubClient = static_cast<uint8_t>((packetHeader >> 10) & 0x3u);
                sp.targetSubClient = static_cast<uint8_t>((packetHeader >> 12) & 0x3u);
                if (offset < subEnd) {
                    sp.payload.assign(batchData + offset, batchData + subEnd);
                }
                offset = subEnd;

                outPackets.push_back(std::move(sp));
            }
            return offset == batchLen && !outPackets.empty();
        };

        auto isKnownCompression = [](uint8_t value) {
            return value == static_cast<uint8_t>(CompressionMethod::Zlib) ||
                   value == static_cast<uint8_t>(CompressionMethod::Snappy) ||
                   value == static_cast<uint8_t>(CompressionMethod::None);
        };

        auto decodeStandard = [&](CompressionMethod method) {
            if (outMethod) *outMethod = method;

            const uint8_t* payload = data + 2;
            size_t payloadLen = len - 2;

            std::vector<uint8_t> decompressed;
            const uint8_t* batchData = payload;
            size_t batchLen = payloadLen;

            if (method == CompressionMethod::None) {
                return parseBatch(batchData, batchLen, out);
            }

            // Preferred path for compressed payload.
            if (decompressor && decompressor(payload, payloadLen, decompressed)) {
                batchData = decompressed.data();
                batchLen = decompressed.size();
                if (parseBatch(batchData, batchLen, out)) {
                    return true;
                }
                out.clear();
            }

            // Compatibility path: some clients keep the compression method byte
            // but send payload as plain (uncompressed) batch when below threshold.
            return parseBatch(payload, payloadLen, out);
        };

        auto decodePreLogin = [&]() {
            if (outMethod) *outMethod = CompressionMethod::None;
            return parseBatch(data + 1, len - 1, out);
        };

        if (format == GamePacketFormat::PreLoginNoCompression) {
            return decodePreLogin();
        }

        if (format == GamePacketFormat::Standard) {
            auto method = static_cast<CompressionMethod>(data[1]);
            if (!isKnownCompression(data[1])) return false;
            return decodeStandard(method);
        }

        // Auto: prefer standard if byte 1 looks like compression.
        if (isKnownCompression(data[1])) {
            if (decodeStandard(static_cast<CompressionMethod>(data[1]))) {
                return true;
            }
            out.clear();
        }

        return decodePreLogin();
    }

    static bool isKnownCompressionMethod(uint8_t value) {
        return value == static_cast<uint8_t>(CompressionMethod::Zlib) ||
               value == static_cast<uint8_t>(CompressionMethod::Snappy) ||
               value == static_cast<uint8_t>(CompressionMethod::None);
    }

    /// Check if a raw packet is a GamePacket (0xFE)
    static bool isGamePacket(const uint8_t* data, size_t len) {
        return len >= 2 && data[0] == GAME_PACKET_ID;
    }
};

} // namespace fishnet::bedrock
