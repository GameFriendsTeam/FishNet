#pragma once

/*
 * Bedrock Client
 *
 * Extends FishClient for connecting to Minecraft Bedrock servers.
 *
 * GamePacket (0xFE) handling:
 *   - Incoming 0xFE packets are automatically unwrapped into sub-packets
 *   - Users receive a GamePacketCallback with (packetId, payload, sender)
 *   - sendGamePacket() wraps sub-packets into 0xFE and sends reliably
 *   - Users can set compress/decompress functions for Zlib/Snappy
 */

#include "fishnet/FishClient.h"
#include "fishnet-bedrock/GamePacket.h"
#include <memory>
#include <functional>

namespace fishnet::bedrock {

using GamePacketCallback = std::function<void(uint32_t packetId, const std::vector<uint8_t>& payload, const fishnet::Address& sender)>;

class FISHNET_API BedrockClient {
public:
    BedrockClient();
    ~BedrockClient();

    bool start();
    void stop();
    bool isRunning() const;
    bool isConnected() const;

    void connect(const std::string& host, uint16_t port = 19132);
    void disconnect();

    void setGamePacketCallback(GamePacketCallback cb);
    void setConnectionCallback(fishnet::ConnectionCallback cb);
    void setDisconnectCallback(fishnet::DisconnectCallback cb);
    void setConfig(const fishnet::PeerConfig& config);

    void setCompressor(GamePacket::CompressFunc func);
    void setDecompressor(GamePacket::DecompressFunc func);
    void setCompressionMethod(CompressionMethod method);

    void sendGamePacket(uint32_t packetId, const uint8_t* data, size_t len);
    void sendGamePackets(const std::vector<SubPacket>& packets);
    void sendRawPacket(const uint8_t* data, size_t len);

    fishnet::FishClient& getFishClient() { return *client_; }
    const fishnet::FishClient& getFishClient() const { return *client_; }

private:
    std::unique_ptr<fishnet::FishClient> client_;

    GamePacketCallback gamePacketCallback_;
    GamePacket::CompressFunc compressor_ = nullptr;
    GamePacket::DecompressFunc decompressor_ = nullptr;
    CompressionMethod compression_ = CompressionMethod::None;

    void onRawPacket(const uint8_t* data, size_t len, const fishnet::Address& sender);
};

} // namespace fishnet::bedrock
