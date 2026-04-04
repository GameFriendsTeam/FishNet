/*
 * Example: Minecraft Bedrock Server with GamePacket handling
 */

#include <fishnet-bedrock.h>
#include <iostream>
#include <thread>
#include <atomic>
#include <csignal>

static std::atomic<bool> running{true};
static std::atomic<int> playerCount{0};
void signalHandler(int) { running = false; }

int main() {
    std::signal(SIGINT, signalHandler);

    fishnet::bedrock::BedrockServer server(19132);

    server.setServerName("FishNet Server");
    server.setProtocolVersion(729);
    server.setGameVersion("1.21.50");
    server.setPlayerCount(0, 50);
    server.setLevelName("Overworld");
    server.setGameMode("Survival", 1);

    server.setConnectionCallback([&](const fishnet::Address& peer, uint64_t /*guid*/) {
        int count = ++playerCount;
        std::cout << "[+] Player connected: " << peer.toString()
                  << " (" << count << " online)" << std::endl;
        server.setPlayerCount(count, 50);
    });

    server.setDisconnectCallback([&](const fishnet::Address& peer, uint64_t /*guid*/) {
        int count = --playerCount;
        if (count < 0) count = 0;
        playerCount = count;
        std::cout << "[-] Player disconnected: " << peer.toString()
                  << " (" << count << " online)" << std::endl;
        server.setPlayerCount(count, 50);
    });

    server.setGamePacketCallback([](uint32_t packetId, const std::vector<uint8_t>& payload,
                                    const fishnet::Address& sender) {
        std::cout << "[GAME] Packet 0x" << std::hex << packetId << std::dec
                  << " (" << payload.size() << " bytes) from "
                  << sender.toString() << std::endl;
    });

    if (!server.start()) {
        std::cerr << "Failed to start server" << std::endl;
        return 1;
    }

    std::cout << "Bedrock server running on port 19132 (Ctrl+C to stop)" << std::endl;

    while (running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    server.stop();
    std::cout << "Server stopped." << std::endl;
    return 0;
}
