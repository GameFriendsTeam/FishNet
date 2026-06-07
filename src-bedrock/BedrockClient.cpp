#include <fishnet/bedrock/BedrockClient.h>

namespace fishnet::bedrock {

BedrockClient::BedrockClient()
    : client_(std::make_unique<fishnet::FishClient>()) {
    client_->setPacketCallback([this](const uint8_t* data, size_t len, const fishnet::Address& sender) {
        onRawPacket(data, len, sender);
    });
}

BedrockClient::~BedrockClient() { stop(); }
bool BedrockClient::start() { return client_->start(); }
void BedrockClient::stop() { client_->stop(); }
bool BedrockClient::isRunning() const { return client_->isRunning(); }
bool BedrockClient::isConnected() const { return client_->isConnected(); }

void BedrockClient::connect(const std::string& host, uint16_t port) { client_->connect(host, port); }
void BedrockClient::disconnect() { client_->disconnect(); }

void BedrockClient::setGamePacketCallback(GamePacketCallback cb) { gamePacketCallback_ = std::move(cb); }
void BedrockClient::setConnectionCallback(fishnet::ConnectionCallback cb) { client_->setConnectionCallback(std::move(cb)); }
void BedrockClient::setDisconnectCallback(fishnet::DisconnectCallback cb) { client_->setDisconnectCallback(std::move(cb)); }
void BedrockClient::setConfig(const fishnet::PeerConfig& config) { client_->setConfig(config); }

void BedrockClient::setCompressor(GamePacket::CompressFunc func) { compressor_ = func; }
void BedrockClient::setDecompressor(GamePacket::DecompressFunc func) { decompressor_ = func; }
void BedrockClient::setCompressionMethod(CompressionMethod method) { compression_ = method; }

void BedrockClient::sendGamePacket(uint32_t packetId, const uint8_t* data, size_t len) {
    SubPacket sp;
    sp.packetId = packetId;
    if (data && len > 0) sp.payload.assign(data, data + len);
    sendGamePackets({sp});
}

void BedrockClient::sendGamePackets(const std::vector<SubPacket>& packets) {
    auto frame = GamePacket::wrap(packets, compression_, compressor_, true);
    client_->sendReliable(frame.data(), frame.size());
}

void BedrockClient::sendPreLoginGamePacket(uint32_t packetId, const uint8_t* data, size_t len) {
    SubPacket sp;
    sp.packetId = packetId;
    if (data && len > 0) sp.payload.assign(data, data + len);
    sendPreLoginGamePackets({sp});
}

void BedrockClient::sendPreLoginGamePackets(const std::vector<SubPacket>& packets) {
    auto frame = GamePacket::wrap(packets, CompressionMethod::None, nullptr, false);
    client_->sendReliable(frame.data(), frame.size());
}

void BedrockClient::sendRawPacket(const uint8_t* data, size_t len) {
    client_->sendReliable(data, len);
}

void BedrockClient::onRawPacket(const uint8_t* data, size_t len, const fishnet::Address& sender) {
    if (len >= 2 && data[0] == GAME_PACKET_ID) {
        std::vector<SubPacket> packets;
        if (GamePacket::unwrap(data, len, packets, decompressor_, GamePacketFormat::Auto)) {
            if (gamePacketCallback_) {
                for (const auto& sp : packets) {
                    gamePacketCallback_(sp.packetId, sp.payload, sender);
                }
            }
        }
    }
}

} // namespace fishnet::bedrock
