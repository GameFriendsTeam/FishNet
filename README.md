# FishNet

Cross-platform RakNet protocol implementation in C++20.
Shared library with optional Minecraft Bedrock extension.

## Features

- Full RakNet handshake with MTU discovery
- Reliability: Unreliable, Reliable, ReliableOrdered
- Split packets, ACK/NAK, retransmission
- Keep-alive ping/pong, connection timeouts
- Bedrock: MOTD, GamePacket (0xFE) auto wrap/unwrap
- Windows (x64/arm64), macOS (x64/arm64), Linux (x64/arm64)

## Quick Start

### Install

Download the latest release for your platform from [Releases](https://github.com/GameFriendsTeam/FishNet/releases), or build from source:

```bash
git clone https://github.com/GameFriendsTeam/FishNet.git
cd fishnet
xmake f -m release
xmake build
```

### Server

```cpp
#include <fishnet.h>
#include <iostream>
#include <thread>
#include <atomic>
#include <csignal>

static std::atomic<bool> running{true};
void onSignal(int) { running = false; }

int main() {
    std::signal(SIGINT, onSignal);

    fishnet::FishServer server(19132);

    // Respond to pings with your own format
    server.setPongDataProvider([]() {
        return "MyGame;MyServer;v1.0;3;20";
    });

    // New player connected
    server.setConnectionCallback([](const fishnet::Address& peer, uint64_t guid) {
        std::cout << "[+] " << peer.toString() << std::endl;
    });

    // Player disconnected
    server.setDisconnectCallback([](const fishnet::Address& peer, uint64_t guid) {
        std::cout << "[-] " << peer.toString() << std::endl;
    });

    // Handle incoming packets
    server.setPacketCallback([&](const uint8_t* data, size_t len,
                                 const fishnet::Address& sender) {
        std::cout << "Packet " << len << " bytes from " << sender.toString() << std::endl;

        // Echo back
        server.sendReliableTo(data, len, sender);
    });

    server.start();
    std::cout << "Server running on port 19132" << std::endl;

    while (running) std::this_thread::sleep_for(std::chrono::seconds(1));
    server.stop();
}
```

## Releases

Each release provides two variants:
- **Core** (`fishnet-<tag>-<platform>.zip`) — generic RakNet, no Bedrock
- **Bedrock** (`fishnet-<tag>-<platform>-bedrock.zip`) — core + Minecraft Bedrock extension

All archives include `include/`, `lib/`, `bin/`, and `LICENSE`.
SHA256 checksums are published with every release.

## Documentation

- [Getting Started](docs/getting-started.md) — download, setup xmake/CMake/Make, first program
- [API Reference](docs/api-reference.md) — full class and method documentation
- [Architecture](docs/architecture.md) — project structure, layer diagram, build options

## Credits

FishNet is a clean-room implementation of the RakNet protocol.
It does not contain any code from the original RakNet library.

The original RakNet was created by Jenkins Software (Oculus VR, LLC / Meta Platforms, Inc.)
and is available under the BSD 2-Clause License at:
https://github.com/facebookarchive/RakNet

Protocol documentation sources:
- https://minecraft.wiki/w/RakNet
- https://wiki.bedrock.dev/servers/raknet

## License

This project is licensed under the **GNU General Public License v3.0** — see [LICENSE](LICENSE) for details.

If you use FishNet in your project, your project must also be open source under a GPL-3.0 compatible license.
