# Architecture

## Project Structure

```
fishnet/
├── src/                              Core FishNet (always compiled)
│   ├── include/
│   │   ├── fishnet.h                 Umbrella header
│   │   └── fishnet/
│   │       ├── platform.h            Cross-platform sockets
│   │       ├── FishPeer.h            Core engine (250 lines)
│   │       ├── FishServer.h          Server wrapper
│   │       ├── FishClient.h          Client wrapper
│   │       ├── Connection.h          Per-peer state
│   │       ├── PacketHandler.h       Callback types
│   │       ├── protocol/
│   │       │   ├── Constants.h       Magic, reliability, packet IDs
│   │       │   └── EncapsulatedPacket.h  Frame structure
│   │       ├── packets/              One file per packet type
│   │       │   ├── UnconnectedPing.h
│   │       │   ├── UnconnectedPong.h
│   │       │   ├── OpenConnectionRequest1.h
│   │       │   ├── OpenConnectionReply1.h
│   │       │   ├── OpenConnectionRequest2.h
│   │       │   ├── OpenConnectionReply2.h
│   │       │   ├── ConnectionRequest.h
│   │       │   ├── ConnectionRequestAccepted.h
│   │       │   ├── NewIncomingConnection.h
│   │       │   ├── ConnectedPing.h
│   │       │   ├── DisconnectionNotification.h
│   │       │   └── AckNak.h
│   │       └── utils/
│   │           ├── BinaryBuffer.h    Binary read/write
│   │           ├── Address.h         sockaddr_in wrapper
│   │           └── AddressSerialization.h  RakNet wire format
│   ├── core/
│   │   ├── FishPeer.cpp              Lifecycle, dispatch
│   │   ├── FishPeerSend.cpp          Sending, split
│   │   ├── FishPeerReceive.cpp       Frame parsing, ACK/NAK
│   │   ├── FishPeerHandshake.cpp     Server + client handshake
│   │   └── FishPeerTick.cpp          Tick loop
│   ├── server/FishServer.cpp
│   └── client/FishClient.cpp
│
├── src-bedrock/                      Bedrock extension (--bedrock=y)
│   ├── include/
│   │   ├── fishnet-bedrock.h         Umbrella header
│   │   └── fishnet-bedrock/
│   │       ├── BedrockMotd.h         MOTD string builder
│   │       ├── BedrockServer.h       Server + GamePacket handling
│   │       ├── BedrockClient.h       Client + GamePacket handling
│   │       └── GamePacket.h          0xFE wrap/unwrap, batch, compression
│   └── core/
│       ├── BedrockServer.cpp
│       └── BedrockClient.cpp
│
├── examples/
│   ├── server.cpp                    Generic FishNet server
│   ├── client.cpp                    Generic FishNet client
│   ├── bedrock_server.cpp            Bedrock server with GamePacket
│   └── bedrock_client.cpp            Bedrock bot with GamePacket
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
| `--examples=y` | `n` | Build example programs |

## Platforms

| Platform | Architectures | Output |
|----------|--------------|--------|
| Windows  | x64, arm64 | `fishnet.dll` |
| macOS    | x64, arm64 | `libfishnet.dylib` |
| Linux    | x64, arm64 | `libfishnet.so` |
