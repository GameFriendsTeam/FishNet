#include <fishnet/bedrock/BedrockServer.h>
#include <fishnet/bedrock/LoginChain.h>

namespace fishnet::bedrock {

BedrockServer::BedrockServer(uint16_t port, const std::string& bindIp)
    : server_(std::make_unique<fishnet::FishServer>(port, bindIp)) {
    motd_.serverId = server_->getPeer().getGuid();
    motd_.portV4 = port;
    motd_.portV6 = port;

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

void BedrockServer::setMotdPorts(uint16_t portV4, uint16_t portV6) {
    std::lock_guard<std::mutex> lock(motdMutex_);
    motd_.portV4 = portV4;
    motd_.portV6 = portV6;
}

void BedrockServer::setMotdPortV4(uint16_t port) {
    std::lock_guard<std::mutex> lock(motdMutex_);
    motd_.portV4 = port;
}

void BedrockServer::setMotdPortV6(uint16_t port) {
    std::lock_guard<std::mutex> lock(motdMutex_);
    motd_.portV6 = port;
}

void BedrockServer::setGamePacketCallback(GamePacketCallback cb) { gamePacketCallback_ = std::move(cb); }

void BedrockServer::setConnectionCallback(fishnet::ConnectionCallback cb) {
    server_->setConnectionCallback(std::move(cb));
}

void BedrockServer::setDisconnectCallback(fishnet::DisconnectCallback cb) {
    server_->setDisconnectCallback([this, cb = std::move(cb)](const fishnet::Address& peer, uint64_t guid) {
        clearPeerState(peer);
        if (cb) {
            cb(peer, guid);
        }
    });
}

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
    auto frame = GamePacket::wrap(packets, compression_, compressor_, true);
    frame = maybeEncryptFrame(frame, dest);
    server_->sendReliableTo(frame.data(), frame.size(), dest);
}

void BedrockServer::sendPreLoginGamePacket(uint32_t packetId, const uint8_t* data, size_t len,
                                           const fishnet::Address& dest) {
    SubPacket sp;
    sp.packetId = packetId;
    if (data && len > 0) sp.payload.assign(data, data + len);
    sendPreLoginGamePackets({sp}, dest);
}

void BedrockServer::sendPreLoginGamePackets(const std::vector<SubPacket>& packets,
                                            const fishnet::Address& dest) {
    auto frame = GamePacket::wrap(packets, CompressionMethod::None, nullptr, false);
    server_->sendReliableTo(frame.data(), frame.size(), dest);
}

void BedrockServer::sendRawPacket(const uint8_t* data, size_t len, const fishnet::Address& dest) {
    server_->sendReliableTo(data, len, dest);
}

void BedrockServer::disconnectPeer(const fishnet::Address& peer) {
    clearPeerState(peer);
    server_->disconnectPeer(peer);
}

void BedrockServer::disconnectAll() {
    {
        std::lock_guard<std::mutex> lock(cryptoMutex_);
        cryptoSessions_.clear();
        peerIdentities_.clear();
    }
    server_->disconnectAll();
}

size_t BedrockServer::getConnectionCount() const { return server_->getConnectionCount(); }

std::string BedrockServer::beginEncryptionHandshake(const fishnet::Address& peer, const std::string& loginChainJson) {
    ParsedLoginIdentity identity;
    if (!parseLoginChain(loginChainJson, identity) || identity.clientPublicKeyB64.empty()) {
        return {};
    }

    PeerCryptoState state;
    state.handshakeSalt = BedrockEncryption::generateRandomBytes(16);
    if (!BedrockEncryption::generateEcKeyPair(state.serverPrivateKeyDer, state.serverPublicKeyDer)) {
        return {};
    }
    state.clientPublicKeyB64 = identity.clientPublicKeyB64;

    const std::string jwt = BedrockEncryption::createHandshakeJwt(
        state.serverPrivateKeyDer, state.serverPublicKeyDer, state.handshakeSalt);
    if (jwt.empty()) {
        return {};
    }

    std::lock_guard<std::mutex> lock(cryptoMutex_);
    cryptoSessions_[peer] = std::move(state);
    peerIdentities_[peer] = std::move(identity);
    return jwt;
}

bool BedrockServer::activatePeerEncryption(const fishnet::Address& peer) {
    std::lock_guard<std::mutex> lock(cryptoMutex_);
    auto it = cryptoSessions_.find(peer);
    if (it == cryptoSessions_.end() || it->second.clientPublicKeyB64.empty()) {
        return false;
    }

    auto aesKey = BedrockEncryption::deriveAesKey(
        it->second.serverPrivateKeyDer,
        it->second.clientPublicKeyB64,
        it->second.handshakeSalt);
    if (aesKey.size() != 32) {
        return false;
    }

    it->second.aesKey = std::move(aesKey);
    it->second.encryptionEnabled = true;
    it->second.sendCounter = 0;
    it->second.recvCounter = 0;
    return true;
}

bool BedrockServer::isPeerEncrypted(const fishnet::Address& peer) const {
    std::lock_guard<std::mutex> lock(cryptoMutex_);
    auto it = cryptoSessions_.find(peer);
    return it != cryptoSessions_.end() && it->second.encryptionEnabled;
}

bool BedrockServer::getPeerIdentity(const fishnet::Address& peer, ParsedLoginIdentity& out) const {
    std::lock_guard<std::mutex> lock(cryptoMutex_);
    auto it = peerIdentities_.find(peer);
    if (it == peerIdentities_.end()) {
        return false;
    }
    out = it->second;
    return true;
}

void BedrockServer::clearPeerState(const fishnet::Address& peer) {
    std::lock_guard<std::mutex> lock(cryptoMutex_);
    cryptoSessions_.erase(peer);
    peerIdentities_.erase(peer);
}

std::vector<uint8_t> BedrockServer::maybeEncryptFrame(const std::vector<uint8_t>& frame, const fishnet::Address& dest) {
    if (frame.size() < 3 || frame[0] != GAME_PACKET_ID) {
        return frame;
    }

    std::lock_guard<std::mutex> lock(cryptoMutex_);
    auto it = cryptoSessions_.find(dest);
    if (it == cryptoSessions_.end() || !it->second.encryptionEnabled) {
        return frame;
    }

    std::vector<uint8_t> encryptedPayload;
    // Bedrock encrypts everything after 0xFE (compression byte + batch), not just the batch body.
    if (!BedrockEncryption::encryptPayload(frame.data() + 1, frame.size() - 1, it->second, encryptedPayload)) {
        return frame;
    }

    std::vector<uint8_t> out;
    out.reserve(1 + encryptedPayload.size());
    out.push_back(frame[0]);
    out.insert(out.end(), encryptedPayload.begin(), encryptedPayload.end());
    return out;
}

std::vector<uint8_t> BedrockServer::maybeDecryptFrame(const uint8_t* data, size_t len, const fishnet::Address& sender) {
    if (len < 3 || data[0] != GAME_PACKET_ID) {
        return std::vector<uint8_t>(data, data + len);
    }

    std::lock_guard<std::mutex> lock(cryptoMutex_);
    auto it = cryptoSessions_.find(sender);
    if (it == cryptoSessions_.end() || !it->second.encryptionEnabled) {
        return std::vector<uint8_t>(data, data + len);
    }

    std::vector<uint8_t> decryptedPayload;
    if (!BedrockEncryption::decryptPayload(data + 1, len - 1, it->second, decryptedPayload)) {
        return {};
    }

    std::vector<uint8_t> out;
    out.reserve(1 + decryptedPayload.size());
    out.push_back(data[0]);
    out.insert(out.end(), decryptedPayload.begin(), decryptedPayload.end());
    return out;
}

void BedrockServer::onRawPacket(const uint8_t* data, size_t len, const fishnet::Address& sender) {
    if (len < 2 || data[0] != GAME_PACKET_ID) {
        return;
    }

    const auto dispatch = [&](const uint8_t* frameData, size_t frameLen) -> bool {
        std::vector<SubPacket> packets;
        if (!GamePacket::unwrap(frameData, frameLen, packets, decompressor_, GamePacketFormat::Auto)) {
            return false;
        }
        if (gamePacketCallback_) {
            for (const auto& sp : packets) {
                gamePacketCallback_(sp.packetId, sp.payload, sender);
            }
        }
        return true;
    };

    if (isPeerEncrypted(sender)) {
        const auto decrypted = maybeDecryptFrame(data, len, sender);
        if (!decrypted.empty()) {
            dispatch(decrypted.data(), decrypted.size());
        }
        return;
    }

    dispatch(data, len);
}

} // namespace fishnet::bedrock
