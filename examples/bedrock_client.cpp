/*
 * Example: Minecraft Bedrock Client (Bot) with GamePacket handling
 *
 * Connects to a Bedrock server, receives GamePackets (0xFE),
 * and logs each sub-packet by ID. This is the foundation for
 * writing a Bedrock bot — add your own packet handlers on top.
 */

#include <fishnet-bedrock.h>
#include <iostream>
#include <thread>
#include <string>
#include <atomic>

int main() {
    fishnet::bedrock::BedrockClient client;

    std::atomic<bool> connected{false};

    client.setConnectionCallback([&](const fishnet::Address& peer, uint64_t /*guid*/) {
        std::cout << "[+] Connected to " << peer.toString() << std::endl;
        connected = true;
    });

    client.setDisconnectCallback([&](const fishnet::Address& /*peer*/, uint64_t /*guid*/) {
        std::cout << "[-] Disconnected" << std::endl;
        connected = false;
    });

    client.setGamePacketCallback([](uint32_t packetId, const std::vector<uint8_t>& payload,
                                    const fishnet::Address& /*sender*/) {
        std::cout << "[GAME] Received packet 0x" << std::hex << packetId << std::dec
                  << " (" << payload.size() << " bytes)" << std::endl;
    });

    if (!client.start()) {
        std::cerr << "Failed to start client" << std::endl;
        return 1;
    }

    std::string host;
    std::cout << "Enter server IP [127.0.0.1]: ";
    std::getline(std::cin, host);
    if (host.empty()) host = "127.0.0.1";

    std::cout << "Connecting to " << host << ":19132..." << std::endl;
    client.connect(host, 19132);

    for (int i = 0; i < 100 && !connected; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    if (!connected) {
        std::cerr << "Connection timed out" << std::endl;
        client.stop();
        return 1;
    }

    std::cout << "Connected! Waiting for game packets..." << std::endl;
    std::cout << "Press Enter to disconnect." << std::endl;
    std::cin.get();

    client.disconnect();
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    client.stop();
    std::cout << "Client stopped." << std::endl;
    return 0;
}
