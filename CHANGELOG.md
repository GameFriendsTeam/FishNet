# Changelog

All notable changes to FishNet will be documented in this file.

## \[v2.1.0\] - 2025.04.12

### Fixed:
-   GamePacket Format

### Addded:
-   IP address change
-   GamePacket types

## \[v2.0.0\] - 2025.04.04

### Initial Alpha Release

-   Core RakNet protocol implementation (C++20, cross-platform)
-   Full connection handshake with MTU discovery
-   Reliability layer: Unreliable, Reliable, ReliableOrdered
-   Split packet sending and reassembly
-   ACK/NAK with automatic retransmission
-   Connected Ping/Pong keep-alive
-   32 ordered delivery channels
-   Connection timeout detection
-   Graceful disconnect (0x15)
-   FishPeer core engine with configurable tick rate
-   FishServer / FishClient high-level wrappers
-   Bedrock extension: BedrockServer with MOTD, BedrockClient
-   GamePacket (0xFE) automatic wrap/unwrap with batch support
-   Compression API (Zlib/Snappy/None) via user-supplied functions
-   Cross-platform: Windows (x64/arm64), macOS (x64/arm64), Linux (x64/arm64)
-   Shared library build (fishnet.dll / [libfishnet.so](http://libfishnet.so) / libfishnet.dylib)
-   xmake build system with --bedrock and --examples options
-   Packet logging in debug builds (FISHNET\_DEBUG)