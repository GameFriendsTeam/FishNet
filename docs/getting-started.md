# Getting Started

## 1. Download

Go to [Releases](../../releases) and download the archive for your platform:

| Archive | Contents |
|---------|----------|
| `fishnet-<tag>-<platform>.zip` | Core RakNet only |
| `fishnet-<tag>-<platform>-bedrock.zip` | Core + Minecraft Bedrock extension |

Verify the download:
```bash
sha256sum -c SHA256SUMS.txt
```

Each archive contains:
```
fishnet-v0.1.0-windows-x64/
├── include/     # Headers
├── lib/         # fishnet.lib / libfishnet.a
├── bin/         # fishnet.dll / libfishnet.so / libfishnet.dylib + examples
└── LICENSE
```

## 2. Setup

Extract the archive and point your build system to `include/` and `lib/`.

---

### xmake

```lua
-- xmake.lua
add_rules("mode.release", "mode.debug")

target("myapp")
    set_kind("binary")
    set_languages("c++20")
    add_files("src/*.cpp")

    -- FishNet paths (adjust to where you extracted)
    add_includedirs("fishnet/include")
    add_linkdirs("fishnet/lib")
    add_links("fishnet")

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

### CMake

```cmake
# CMakeLists.txt
cmake_minimum_required(VERSION 3.16)
project(myapp LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)

add_executable(myapp src/main.cpp)

# FishNet paths (adjust to where you extracted)
set(FISHNET_DIR "${CMAKE_SOURCE_DIR}/fishnet")

target_include_directories(myapp PRIVATE "${FISHNET_DIR}/include")
target_link_directories(myapp PRIVATE "${FISHNET_DIR}/lib")
target_link_libraries(myapp PRIVATE fishnet)

# Windows needs these
if(WIN32)
    target_link_libraries(myapp PRIVATE ws2_32 iphlpapi)
endif()
```

```bash
mkdir build && cd build
cmake ..
cmake --build .
```

---

### Makefile

```makefile
# Makefile
CXX      = g++
CXXFLAGS = -std=c++20 -Wall -O2
FISHNET  = ./fishnet

INCLUDES = -I$(FISHNET)/include
LDFLAGS  = -L$(FISHNET)/lib -lfishnet -lpthread

# macOS: add rpath
UNAME := $(shell uname)
ifeq ($(UNAME), Darwin)
    LDFLAGS += -Wl,-rpath,$(FISHNET)/lib
endif
ifeq ($(UNAME), Linux)
    LDFLAGS += -Wl,-rpath,$(FISHNET)/lib
endif

all: myapp

myapp: src/main.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ $^ $(LDFLAGS)

clean:
	rm -f myapp
```

```bash
make
./myapp
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
    server.setProtocolVersion(729);
    server.setGameVersion("1.21.50");
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
git clone <repo-url>
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
