# Getting Started

### xmake

```lua
-- xmake.lua
add_rules("mode.release", "mode.debug")

add_repositories("my-repo https://github.com/GameFriendsTeam/xmake-repo.git")
add_requires("fishnet")

target("myapp")
    set_kind("binary")
    set_languages("c++20")
    add_files("src/*.cpp")
    add_packages("fishnet")

    -- Windows needs these
    if is_plat("windows") then
        add_syslinks("ws2_32", "iphlpapi")
    end
```

```bash
xmake build
xmake run myapp
```

---

## 3. First Program

### Server
```cpp
#include <fishnet.h>
#include <iostream>
#include <thread>
#include <csignal>
#include <atomic>

static std::atomic<bool> running{true};
void onSignal(int) { running = false; }

int main() {
    std::signal(SIGINT, onSignal);

    fishnet::FishServer server(19132);

    server.setPongDataProvider([]() {
        return "MyGame;MyServer;1.0";
    });

    server.setConnectionCallback([](const fishnet::Address& peer, uint64_t guid) {
        std::cout << "Connected: " << peer.toString() << std::endl;
    });

    server.setPacketCallback([](const uint8_t* data, size_t len,
                                const fishnet::Address& sender) {
        std::cout << "Packet: " << len << " bytes" << std::endl;
    });

    server.start();
    std::cout << "Server on port 19132" << std::endl;

    while (running) std::this_thread::sleep_for(std::chrono::seconds(1));

    server.stop();
}
```

### Client
```cpp
#include <fishnet.h>
#include <iostream>
#include <thread>

int main() {
    fishnet::FishClient client;

    client.setConnectionCallback([](const fishnet::Address& peer, uint64_t guid) {
        std::cout << "Connected to " << peer.toString() << std::endl;
    });

    client.start();
    client.connect("127.0.0.1", 19132);

    std::this_thread::sleep_for(std::chrono::seconds(5));

    client.disconnect();
    client.stop();
}
```

### Bedrock Server
```cpp
#include <fishnet-bedrock.h>
#include <iostream>
#include <thread>
#include <csignal>
#include <atomic>

static std::atomic<bool> running{true};
void onSignal(int) { running = false; }

int main() {
    std::signal(SIGINT, onSignal);

    fishnet::bedrock::BedrockServer server(19132);
    server.setServerName("My Bedrock Server");
    server.setProtocolVersion(944);
    server.setGameVersion("1.26.10");
    server.setPlayerCount(0, 50);

    server.setGamePacketCallback([](uint32_t packetId,
                                    const std::vector<uint8_t>& payload,
                                    const fishnet::Address& sender) {
        std::cout << "Game packet 0x" << std::hex << packetId
                  << " (" << std::dec << payload.size() << " bytes)" << std::endl;
    });

    server.start();
    std::cout << "Bedrock server on port 19132" << std::endl;

    while (running) std::this_thread::sleep_for(std::chrono::seconds(1));

    server.stop();
}
```

## 4. Runtime

On Windows, make sure `fishnet.dll` is next to your executable or in PATH.

On Linux/macOS, either:
- Set `LD_LIBRARY_PATH` / `DYLD_LIBRARY_PATH`
- Or use rpath (shown in Makefile example above)

## Building from source

If you want to build FishNet itself instead of using a release:

```bash
git clone https://github.com/GameFriendsTeam/FishNet.git
cd fishnet

# Core only
xmake f -m release
xmake build

# With Bedrock
xmake f -m release --bedrock=y
xmake build

# With examples
xmake f -m release --bedrock=y --examples=y
xmake build

# Debug (with packet logging)
xmake f -m debug --bedrock=y --examples=y
xmake build
```
