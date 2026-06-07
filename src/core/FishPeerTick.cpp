/*
 * FishPeer — Tick Loop
 *
 * Runs at configurable interval (default 50ms = 20 ticks/sec).
 * Each tick: retransmit unacked datagrams, send keep-alive pings,
 * check connection timeouts, clean up stale split buffers,
 * retry client handshake if still in progress.
 */

#include <fishnet/FishPeer.h>
#include <fishnet/packets/ConnectedPing.h>
#include <fishnet/packets/OpenConnectionRequest1.h>
#include <fishnet/packets/OpenConnectionRequest2.h>
#include <fishnet/packets/ConnectionRequest.h>

namespace fishnet {

void FishPeer::tickLoop() {
    while (running_.load()) {
        auto start = std::chrono::steady_clock::now();

        tickRetransmit();
        tickPing();
        tickTimeouts();
        tickSplitCleanup();
        tickClientHandshake();

        auto elapsed = std::chrono::steady_clock::now() - start;
        auto sleepTime = std::chrono::milliseconds(config_.tickIntervalMs) - elapsed;
        if (sleepTime.count() > 0) {
            std::this_thread::sleep_for(sleepTime);
        }
    }
}

void FishPeer::tickRetransmit() {
    auto now = std::chrono::steady_clock::now();
    std::lock_guard<std::mutex> lock(sendMutex_);

    for (auto& [seq, pending] : pendingDatagrams_) {
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - pending.sentAt).count();
        if (elapsed > static_cast<int64_t>(config_.retransmitTimeoutMs)) {
            sendRaw(pending.bytes.data(), pending.bytes.size(), pending.dest);
            pending.sentAt = now;
            pending.retransmitCount++;
        }
    }

    // Drop after too many retransmits
    for (auto it = pendingDatagrams_.begin(); it != pendingDatagrams_.end(); ) {
        if (it->second.retransmitCount > 10) {
            it = pendingDatagrams_.erase(it);
        } else {
            ++it;
        }
    }
}

void FishPeer::tickPing() {
    auto now = std::chrono::steady_clock::now();
    std::lock_guard<std::mutex> lock(connMutex_);

    for (auto& [guid, conn] : connectionsByGuid_) {
        if (!conn.connected) continue;
        auto sinceActivity = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - conn.lastActivity).count();

        if (sinceActivity > static_cast<int64_t>(config_.pingIntervalMs)) {
            auto ping = ConnectedPing::encode(getTimestamp());
            sendUnreliable(ping.data(), ping.size(), conn.address);
            conn.lastPingTime = getTimestamp();
        }
    }
}

void FishPeer::tickTimeouts() {
    std::vector<std::pair<Address, uint64_t>> timedOut;
    {
        std::lock_guard<std::mutex> lock(connMutex_);
        for (auto& [guid, conn] : connectionsByGuid_) {
            if (conn.isTimedOut(config_.connectionTimeoutMs)) {
                timedOut.push_back({conn.address, guid});
            }
        }
        for (auto& [addr, guid] : timedOut) {
            removeConnection(addr, guid);
        }
    }

    if (!timedOut.empty()) {
        std::lock_guard<std::mutex> lock(sendMutex_);
        for (auto it = pendingDatagrams_.begin(); it != pendingDatagrams_.end(); ) {
            bool erase = false;
            for (const auto& [addr, _guid] : timedOut) {
                if (it->second.dest == addr) {
                    erase = true;
                    break;
                }
            }
            if (erase) {
                it = pendingDatagrams_.erase(it);
            } else {
                ++it;
            }
        }
    }

    for (auto& [addr, guid] : timedOut) {
        if (disconnectCallback_) disconnectCallback_(addr, guid);
    }
}

void FishPeer::tickSplitCleanup() {
    auto now = std::chrono::steady_clock::now();
    std::lock_guard<std::mutex> lock(splitMutex_);

    for (auto it = splitStore_.begin(); it != splitStore_.end(); ) {
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - it->second.createdAt).count();
        if (elapsed > static_cast<int64_t>(config_.splitStaleTimeoutMs)) {
            it = splitStore_.erase(it);
        } else {
            ++it;
        }
    }
}

void FishPeer::tickClientHandshake() {
    if (clientState_ == ClientState::Disconnected ||
        clientState_ == ClientState::Connected) return;

    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - clientHandshakeTime_).count();

    if (elapsed < 2000) return; // Retry every 2s
    clientHandshakeTime_ = now;

    switch (clientState_) {
        case ClientState::MtuDiscovery: {
            mtuAttempt_++;
            if (mtuAttempt_ >= 3) {
                clientState_ = ClientState::Disconnected;
                return;
            }
            uint16_t tryMtu = config_.mtuSizes[mtuAttempt_];
            auto req = OpenConnectionRequest1::encode(11, tryMtu);
            sendRaw(req.data(), req.size(), clientTargetServer_);
            break;
        }
        case ClientState::WaitingReply2: {
            auto req = OpenConnectionRequest2::encode(clientTargetServer_, static_cast<uint16_t>(mtu_), guid_);
            sendRaw(req.data(), req.size(), clientTargetServer_);
            break;
        }
        case ClientState::WaitingAccepted: {
            auto req = ConnectionRequest::encode(guid_, getTimestamp(), false);
            sendReliableOrdered(req.data(), req.size(), clientTargetServer_, 0);
            break;
        }
        default:
            break;
    }
}

} // namespace fishnet
