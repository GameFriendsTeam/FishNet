/*
 * Example: Generic FishNet Server
 * Shows: pong data, connection/disconnect callbacks, packet handling
 */

#include <fishnet.h>
#include <iostream>
#include <thread>
#include <atomic>
#include <csignal>

static std::atomic<bool> running{true};
void signalHandler(int) { running = false; }

int main() {
    std::signal(SIGINT, signalHandler);

    fishnet::FishServer server(19132);

    server.setPongDataProvider([]() -> std::string {
        return "MyCustomProtocol;MyServer;100;1.0.0";
    });

    server.setConnectionCallback([](const fishnet::Address& peer, uint64_t guid) {
        std::cout << "[+] New connection: " << peer.toString()
                  << " (guid: " << guid << ")" << std::endl;
    });

    server.setDisconnectCallback([](const fishnet::Address& peer, uint64_t guid) {
        std::cout << "[-] Disconnected: " << peer.toString()
                  << " (guid: " << guid << ")" << std::endl;
    });

    server.setPacketCallback([&](const uint8_t* data, size_t len, const fishnet::Address& sender) {
        std::cout << "[PACKET] " << len << " bytes from " << sender.toString()
                  << " | ID: 0x" << std::hex << static_cast<int>(data[0])
                  << std::dec << std::endl;

        server.sendReliableTo(data, len, sender);
    });

    if (!server.start()) {
        std::cerr << "Failed to start server on port 19132" << std::endl;
        return 1;
    }

    std::cout << "Server running on port 19132 (Ctrl+C to stop)" << std::endl;

    while (running) {
        std::cout << "  Connections: " << server.getConnectionCount() << "\r" << std::flush;
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    server.stop();
    std::cout << "\nServer stopped." << std::endl;
    return 0;
}
