/*
 * FishClient Implementation
 *
 * Thin wrapper over FishPeer with port=0 (OS-assigned).
 * Provides connect/disconnect lifecycle and delegates sending.
 */

#include "fishnet/FishClient.h"

namespace fishnet {

FishClient::FishClient()
    : peer_(std::make_unique<FishPeer>(static_cast<uint16_t>(0))) {}

FishClient::~FishClient() {
    stop();
}

bool FishClient::start() {
    return peer_->start();
}

void FishClient::stop() {
    if (peer_->isRunning()) {
        disconnect();
        peer_->stop();
    }
}

bool FishClient::isRunning() const {
    return peer_->isRunning();
}

void FishClient::connect(const std::string& host, uint16_t port) {
    serverAddress_ = Address(host, port);
    peer_->connectTo(serverAddress_);
}

void FishClient::disconnect() {
    if (peer_->isConnected(serverAddress_)) {
        peer_->disconnect(serverAddress_);
    }
}

bool FishClient::isConnected() const {
    return peer_->isConnected(serverAddress_);
}

void FishClient::setPacketCallback(PacketCallback cb) {
    peer_->setPacketCallback(std::move(cb));
}

void FishClient::setConnectionCallback(ConnectionCallback cb) {
    peer_->setConnectionCallback(std::move(cb));
}

void FishClient::setDisconnectCallback(DisconnectCallback cb) {
    peer_->setDisconnectCallback(std::move(cb));
}

void FishClient::setConfig(const PeerConfig& config) {
    peer_->setConfig(config);
}

void FishClient::send(const uint8_t* data, size_t len) {
    peer_->sendUnreliable(data, len, serverAddress_);
}

void FishClient::sendReliable(const uint8_t* data, size_t len, uint8_t channel) {
    peer_->sendReliableOrdered(data, len, serverAddress_, channel);
}

} // namespace fishnet
