/*
 * Example: Generic FishNet Client
 * Shows: full handshake, connection callback, send/receive, disconnect
 */

#include <fishnet.h>
#include <iostream>
#include <thread>
#include <string>
#include <atomic>
#include <cstring>

int main() {
    fishnet::FishClient client;

    std::atomic<bool> connected{false};

    client.setConnectionCallback([&](const fishnet::Address& peer, uint64_t /*guid*/) {
        std::cout << "[+] Connected to " << peer.toString() << std::endl;
        connected = true;
    });

    client.setDisconnectCallback([&](const fishnet::Address& peer, uint64_t /*guid*/) {
        std::cout << "[-] Disconnected from " << peer.toString() << std::endl;
        connected = false;
    });

    client.setPacketCallback([](const uint8_t* data, size_t len, const fishnet::Address& sender) {
        std::cout << "[PACKET] " << len << " bytes from " << sender.toString()
                  << " | ID: 0x" << std::hex << static_cast<int>(data[0])
                  << std::dec << std::endl;
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

    // Wait for connection (up to 10 seconds)
    for (int i = 0; i < 100 && !connected; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    if (!connected) {
        std::cerr << "Connection timed out" << std::endl;
        client.stop();
        return 1;
    }

    // Send some test data
    const char* msg = "Hello from FishNet client!";
    uint8_t packet[256];
    packet[0] = 0xFE; // Custom packet ID
    std::memcpy(packet + 1, msg, std::strlen(msg));
    client.sendReliable(packet, 1 + std::strlen(msg));
    std::cout << "Sent test packet" << std::endl;

    std::this_thread::sleep_for(std::chrono::seconds(3));

    client.disconnect();
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    client.stop();
    std::cout << "Client stopped." << std::endl;
    return 0;
}
