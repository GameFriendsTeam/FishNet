# Architecture

## Project Structure

```
fishnet/
├── src/                              Core FishNet (always compiled)
│   ├── core/
│   │   ├── FishPeer.cpp              Lifecycle, dispatch
│   │   ├── FishPeerSend.cpp          Sending, split
│   │   ├── FishPeerReceive.cpp       Frame parsing, ACK/NAK
│   │   ├── FishPeerHandshake.cpp     Server + client handshake
│   │   └── FishPeerTick.cpp          Tick loop
│   ├── server/FishServer.cpp
│   └── client/FishClient.cpp
│
├── include/                          Standard headers (always compiled)
│   └── fishnet/
│       ├── platform.h            Cross-platform sockets
│       ├── FishPeer.h            Core engine (250 lines)
│       ├── FishServer.h          Server wrapper
│       ├── FishClient.h          Client wrapper
│       ├── Connection.h          Per-peer state
│       ├── PacketHandler.h       Callback types
│       ├── protocol/
│       │   ├── Constants.h       Magic, reliability, packet IDs
│       │   └── EncapsulatedPacket.h  Frame structure
│       ├── packets/              One file per packet type
│       │   ├── UnconnectedPing.h
│       │   ├── UnconnectedPong.h
│       │   └── ...
│       └── utils/
│           ├── BinaryBuffer.h    Binary read/write
│           ├── Address.h         sockaddr_in wrapper
│           └── AddressSerialization.h  RakNet wire format
│
├── src-bedrock/                      Bedrock extension source (--bedrock=y)
│   └── core/
│       ├── BedrockServer.cpp
│       └── BedrockClient.cpp
│
├── include-bedrock/                  Bedrock extension headers (--bedrock=y)
│   └── fishnet/
│       └── bedrock/
│           ├── BedrockMotd.h         MOTD string builder
│           ├── BedrockServer.h       Server + GamePacket handling
│           ├── BedrockClient.h       Client + GamePacket handling
│           └── GamePacket.h          0xFE wrap/unwrap, batch, compression
│
├── docs/                             Documentation
├── .github/workflows/                CI/CD
├── xmake.lua                         Build configuration
├── CHANGELOG.md
├── LICENSE                           MIT
└── README.md
```

## Layer Diagram

```
┌─────────────────────────────────────────────┐
│  User Application (your server/bot/proxy)   │
├─────────────────────────────────────────────┤
│  BedrockServer / BedrockClient              │  ← GamePacket 0xFE auto wrap/unwrap
│  BedrockMotd, GamePacket                    │  ← src-bedrock/ (optional)
├─────────────────────────────────────────────┤
│  FishServer / FishClient                    │  ← High-level API
├─────────────────────────────────────────────┤
│  FishPeer                                   │  ← Core engine
│  ┌─────────┬──────────┬───────────────────┐ │
│  │ Send    │ Receive  │ Handshake         │ │
│  │ Split   │ ACK/NAK  │ MTU discovery     │ │
│  │ Reliable│ Ordering │ Ping/Pong         │ │
│  └─────────┴──────────┴───────────────────┘ │
├─────────────────────────────────────────────┤
│  Platform (WinSock2 / POSIX sockets)        │
├─────────────────────────────────────────────┤
│  UDP                                        │
└─────────────────────────────────────────────┘
```

## Build Options

| Option | Default | Description |
|--------|---------|-------------|
| `-m debug` | — | Debug build with packet logging |
| `-m release` | — | Optimized build, no logging |
| `--bedrock=y` | `n` | Include Bedrock extension |

## Platforms

| Platform | Architectures | Output |
|----------|--------------|--------|
| Windows  | x64, arm64 | `fishnet.dll` |
| macOS    | x64, arm64 | `libfishnet.dylib` |
| Linux    | x64, arm64 | `libfishnet.so` |
