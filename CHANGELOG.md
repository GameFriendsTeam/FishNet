# Changelog

All notable changes to FishNet will be documented in this file.

## [v0.1.0-A.1] - 2025.04.04

### Initial Alpha Release

- Core RakNet protocol implementation (C++20, cross-platform)
- Full connection handshake with MTU discovery
- Reliability layer: Unreliable, Reliable, ReliableOrdered
- Split packet sending and reassembly
- ACK/NAK with automatic retransmission
- Connected Ping/Pong keep-alive
- 32 ordered delivery channels
- Connection timeout detection
- Graceful disconnect (0x15)
- FishPeer core engine with configurable tick rate
- FishServer / FishClient high-level wrappers
- Bedrock extension: BedrockServer with MOTD, BedrockClient
- GamePacket (0xFE) automatic wrap/unwrap with batch support
- Compression API (Zlib/Snappy/None) via user-supplied functions
- Cross-platform: Windows (x64/arm64), macOS (x64/arm64), Linux (x64/arm64)
- Shared library build (fishnet.dll / libfishnet.so / libfishnet.dylib)
- xmake build system with --bedrock and --examples options
- Packet logging in debug builds (FISHNET_DEBUG)
