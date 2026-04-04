/*
 * BedrockServer Implementation
 *
 * Wires BedrockMotd into PongDataProvider.
 * Intercepts raw packets: if 0xFE, unwraps into sub-packets
 * and fires GamePacketCallback. Otherwise passes through.
 */

#include "fishnet-bedrock/BedrockServer.h"

namespace fishnet::bedrock {

BedrockServer::BedrockServer(uint16_t port)
    : server_(std::make_unique<fishnet::FishServer>(port)) {
    motd_.serverId = server_->getPeer().getGuid();
    motd_.portV4 = port;

    server_->setPongDataProvider([this]() -> std::string {
        std::lock_guard<std::mutex> lock(motdMutex_);
        return motd_.build();
    });

    server_->setPacketCallback([this](const uint8_t* data, size_t len, const fishnet::Address& sender) {
        onRawPacket(data, len, sender);
    });
}

BedrockServer::~BedrockServer() { stop(); }
bool BedrockServer::start() { return server_->start(); }
void BedrockServer::stop() { server_->stop(); }
bool BedrockServer::isRunning() const { return server_->isRunning(); }

void BedrockServer::setServerName(const std::string& name) {
    std::lock_guard<std::mutex> lock(motdMutex_);
    motd_.serverName = name;
}

void BedrockServer::setProtocolVersion(int version) {
    std::lock_guard<std::mutex> lock(motdMutex_);
    motd_.protocolVersion = version;
}

void BedrockServer::setGameVersion(const std::string& version) {
    std::lock_guard<std::mutex> lock(motdMutex_);
    motd_.gameVersion = version;
}

void BedrockServer::setPlayerCount(int current, int max) {
    std::lock_guard<std::mutex> lock(motdMutex_);
    motd_.currentPlayers = current;
    motd_.maxPlayers = max;
}

void BedrockServer::setLevelName(const std::string& name) {
    std::lock_guard<std::mutex> lock(motdMutex_);
    motd_.levelName = name;
}

void BedrockServer::setGameMode(const std::string& mode, int numeric) {
    std::lock_guard<std::mutex> lock(motdMutex_);
    motd_.gameMode = mode;
    motd_.gameModeNumeric = numeric;
}

void BedrockServer::setGamePacketCallback(GamePacketCallback cb) { gamePacketCallback_ = std::move(cb); }
void BedrockServer::setConnectionCallback(fishnet::ConnectionCallback cb) { server_->setConnectionCallback(std::move(cb)); }
void BedrockServer::setDisconnectCallback(fishnet::DisconnectCallback cb) { server_->setDisconnectCallback(std::move(cb)); }
void BedrockServer::setConfig(const fishnet::PeerConfig& config) { server_->setConfig(config); }

void BedrockServer::setCompressor(GamePacket::CompressFunc func) { compressor_ = func; }
void BedrockServer::setDecompressor(GamePacket::DecompressFunc func) { decompressor_ = func; }
void BedrockServer::setCompressionMethod(CompressionMethod method) { compression_ = method; }

void BedrockServer::sendGamePacket(uint32_t packetId, const uint8_t* data, size_t len,
                                    const fishnet::Address& dest) {
    SubPacket sp;
    sp.packetId = packetId;
    if (data && len > 0) sp.payload.assign(data, data + len);
    sendGamePackets({sp}, dest);
}

void BedrockServer::sendGamePackets(const std::vector<SubPacket>& packets,
                                     const fishnet::Address& dest) {
    auto frame = GamePacket::wrap(packets, compression_, compressor_);
    server_->sendReliableTo(frame.data(), frame.size(), dest);
}

void BedrockServer::sendRawPacket(const uint8_t* data, size_t len, const fishnet::Address& dest) {
    server_->sendReliableTo(data, len, dest);
}

void BedrockServer::disconnectPeer(const fishnet::Address& peer) { server_->disconnectPeer(peer); }
void BedrockServer::disconnectAll() { server_->disconnectAll(); }
size_t BedrockServer::getConnectionCount() const { return server_->getConnectionCount(); }

void BedrockServer::onRawPacket(const uint8_t* data, size_t len, const fishnet::Address& sender) {
    if (len >= 2 && data[0] == GAME_PACKET_ID) {
        std::vector<SubPacket> packets;
        if (GamePacket::unwrap(data, len, packets, decompressor_)) {
            if (gamePacketCallback_) {
                for (const auto& sp : packets) {
                    gamePacketCallback_(sp.packetId, sp.payload, sender);
                }
            }
        }
    }
}

} // namespace fishnet::bedrock
