#pragma once

/*
 * Bedrock Server
 *
 * Extends FishServer with Minecraft Bedrock Edition specifics.
 * Automatically wires BedrockMotd as the PongDataProvider.
 *
 * GamePacket (0xFE) handling:
 *   - Incoming 0xFE packets are automatically unwrapped into sub-packets
 *   - Users receive a GamePacketCallback with (packetId, payload, sender)
 *   - Outgoing sub-packets are wrapped into 0xFE with optional compression
 *   - Users can set compress/decompress functions for Zlib/Snappy
 */

#include <fishnet/FishServer.h>
#include <fishnet/bedrock/BedrockMotd.h>
#include <fishnet/bedrock/GamePacket.h>
#include <memory>
#include <mutex>
#include <functional>
#include <string>

namespace fishnet::bedrock {

using GamePacketCallback = std::function<void(uint32_t packetId, const std::vector<uint8_t>& payload, const fishnet::Address& sender)>;

class FISHNET_API BedrockServer {
public:
    explicit BedrockServer(uint16_t port = 19132, const std::string& bindIp = "0.0.0.0");
    ~BedrockServer();

    bool start();
    void stop();
    bool isRunning() const;

    void setServerName(const std::string& name);
    void setProtocolVersion(int version);
    void setGameVersion(const std::string& version);
    void setPlayerCount(int current, int max);
    void setLevelName(const std::string& name);
    void setGameMode(const std::string& mode, int numeric);
    void setMotdPorts(uint16_t portV4, uint16_t portV6);
    void setMotdPortV4(uint16_t port);
    void setMotdPortV6(uint16_t port);

    void setGamePacketCallback(GamePacketCallback cb);
    void setConnectionCallback(fishnet::ConnectionCallback cb);
    void setDisconnectCallback(fishnet::DisconnectCallback cb);
    void setConfig(const fishnet::PeerConfig& config);

    void setCompressor(GamePacket::CompressFunc func);
    void setDecompressor(GamePacket::DecompressFunc func);
    void setCompressionMethod(CompressionMethod method);

    void sendGamePacket(uint32_t packetId, const uint8_t* data, size_t len, const fishnet::Address& dest);
    void sendGamePackets(const std::vector<SubPacket>& packets, const fishnet::Address& dest);
    void sendPreLoginGamePacket(uint32_t packetId, const uint8_t* data, size_t len, const fishnet::Address& dest);
    void sendPreLoginGamePackets(const std::vector<SubPacket>& packets, const fishnet::Address& dest);
    void sendRawPacket(const uint8_t* data, size_t len, const fishnet::Address& dest);

    void disconnectPeer(const fishnet::Address& peer);
    void disconnectAll();
    size_t getConnectionCount() const;

    fishnet::FishServer& getFishServer() { return *server_; }
    const fishnet::FishServer& getFishServer() const { return *server_; }
    BedrockMotd& getMotd() { return motd_; }

private:
    std::unique_ptr<fishnet::FishServer> server_;
    BedrockMotd motd_;
    std::mutex motdMutex_;

    GamePacketCallback gamePacketCallback_;
    GamePacket::CompressFunc compressor_ = nullptr;
    GamePacket::DecompressFunc decompressor_ = nullptr;
    CompressionMethod compression_ = CompressionMethod::None;

    void onRawPacket(const uint8_t* data, size_t len, const fishnet::Address& sender);
};

} // namespace fishnet::bedrock
