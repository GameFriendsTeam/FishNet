# Changelog

All notable changes to FishNet will be documented in this file.

## \[v2.2.4\] - 2026.06.11

### Fixed:
- Bedrock encryption now covers the compression byte as well as the batch payload (everything after `0xFE`), matching CloudBurst/gophertunnel.
- Decryption verifies the SHA-256 trailer before advancing the receive counter; failed decrypts no longer desync the session.

## \[v2.2.3\] - 2026.06.11

### Added:
- Bedrock post-login encryption: ECDH key exchange (secp384r1), AES-256-CTR packet encryption, SHA-256 trailer.
- ES384 JWT generation for `ServerToClientHandshake` via OpenSSL.
- `LoginChain` parser: extracts client public key from Xbox Live `Token` JWT (`cpk`) or legacy `identityPublicKey`.
- Bedrock server API: `beginEncryptionHandshake()`, `activatePeerEncryption()`, `isPeerEncrypted()`, `getPeerIdentity()`.

### Updated:
- `bedrock=y` xmake config now requires OpenSSL.

## \[v2.2.2\] - 2026.06.07

### Fixed:
- Fixed `xmake install` not copying header files to the installation directory.
- Fixed GitHub Actions release workflow failing on Windows due to `Copy-Item` directory merging issues.

## \[v2.2.1\] - 2026.06.07

### Added:
- Modular includes architecture: headers are now included individually (e.g., `<fishnet/FishServer.h>`, `<fishnet/bedrock/BedrockServer.h>`).
- Separated Bedrock extension headers into an isolated `include-bedrock` directory that is only exposed when the `bedrock` configuration is enabled.

### Removed:
- Removed the monolithic `fishnet.h` and `fishnet-bedrock.h` umbrella headers.
- Removed the deprecated `--examples` build parameter.

### Fixed:
- Fixed multiple packet header files containing duplicate `#pragma once` directives.

### Updated:
- Updated `getting-started.md`, `api-reference.md`, and `architecture.md` to reflect the new modular include paths and directory structure.

## \[v2.1.0\] - 2025.04.12

### Fixed:
-   GamePacket Format

### Addded:
-   IP address change
-   GamePacket types

### Updated:
    docs
    github actions

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
-   xmake build system with --bedrock option
-   Packet logging in debug builds (FISHNET\_DEBUG)