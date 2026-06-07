/*
 * FishServer Implementation
 *
 * Thin wrapper over FishPeer. Delegates all operations
 * and exposes a clean server API with callbacks.
 */

#include <fishnet/FishServer.h>

namespace fishnet {

FishServer::FishServer(uint16_t port, const std::string& bindIp)
    : peer_(std::make_unique<FishPeer>(port, bindIp)) {}

FishServer::~FishServer() {
    stop();
}

bool FishServer::start() {
    return peer_->start();
}

void FishServer::stop() {
    peer_->stop();
}

bool FishServer::isRunning() const {
    return peer_->isRunning();
}

void FishServer::setPacketCallback(PacketCallback cb) {
    peer_->setPacketCallback(std::move(cb));
}

void FishServer::setPongDataProvider(PongDataProvider provider) {
    peer_->setPongDataProvider(std::move(provider));
}

void FishServer::setConnectionCallback(ConnectionCallback cb) {
    peer_->setConnectionCallback(std::move(cb));
}

void FishServer::setDisconnectCallback(DisconnectCallback cb) {
    peer_->setDisconnectCallback(std::move(cb));
}

void FishServer::setConfig(const PeerConfig& config) {
    peer_->setConfig(config);
}

void FishServer::sendTo(const uint8_t* data, size_t len, const Address& dest) {
    peer_->sendRaw(data, len, dest);
}

void FishServer::sendReliableTo(const uint8_t* data, size_t len, const Address& dest, uint8_t channel) {
    peer_->sendReliableOrdered(data, len, dest, channel);
}

void FishServer::disconnectPeer(const Address& peer) {
    peer_->disconnect(peer);
}

void FishServer::disconnectAll() {
    peer_->disconnectAll();
}

size_t FishServer::getConnectionCount() const {
    return peer_->getConnectionCount();
}

} // namespace fishnet
