# API Reference

Complete documentation of every public class, method, type, and constant in FishNet.

---

## Table of Contents

- [Headers](#headers)
- [Namespace fishnet](#namespace-fishnet)
  - [Platform](#platform)
  - [Address](#address)
  - [BinaryBuffer](#binarybuffer)
  - [Address Serialization](#address-serialization)
  - [Constants](#constants)
  - [EncapsulatedPacket](#encapsulatedpacket)
  - [Connection](#connection)
  - [Callback Types](#callback-types)
  - [PeerConfig](#peerconfig)
  - [FishPeer](#fishpeer)
  - [FishServer](#fishserver)
  - [FishClient](#fishclient)
  - [Packets](#packets)
- [Namespace fishnet::bedrock](#namespace-fishnetbedrock)
  - [BedrockMotd](#bedrockmotd)
  - [GamePacket](#gamepacket)
  - [BedrockServer](#bedrockserver)
  - [BedrockClient](#bedrockclient)

---

## Headers

FishNet uses modular headers. You only need to include the specific components you use.

| Example Component | Header Path |
|-------------------|-------------|
| `FishServer` | `<fishnet/FishServer.h>` |
| `FishClient` | `<fishnet/FishClient.h>` |
| `Connection` | `<fishnet/Connection.h>` |
| `BedrockServer` | `<fishnet/bedrock/BedrockServer.h>` |
| `BedrockClient` | `<fishnet/bedrock/BedrockClient.h>` |

---

## Namespace `fishnet`

### Platform

**Header:** `<fishnet/platform.h>`

#### Macros

| Macro | Defined when |
|-------|-------------|
| `FISHNET_PLATFORM_WINDOWS` | Windows (MSVC, MinGW) |
| `FISHNET_PLATFORM_LINUX` | Linux |
| `FISHNET_PLATFORM_MACOS` | macOS |
| `FISHNET_API` | DLL export/import on Windows, visibility on GCC/Clang |

#### Types

```cpp
using SocketHandle = SOCKET;        // Windows
using SocketHandle = int;           // POSIX
constexpr SocketHandle InvalidSocket;
constexpr int SocketError;
```

#### Functions (`fishnet::platform`)

```cpp
bool initSockets();
```
Initialize the socket subsystem. On Windows calls `WSAStartup()`. On POSIX, no-op. Returns `true` on success.

```cpp
void cleanupSockets();
```
Cleanup the socket subsystem. On Windows calls `WSACleanup()`.

```cpp
void closeSocket(SocketHandle sock);
```
Close a socket handle. Cross-platform.

```cpp
int getLastError();
```
Returns the last socket error code. `WSAGetLastError()` on Windows, `errno` on POSIX.

```cpp
bool setNonBlocking(SocketHandle sock);
```
Set a socket to non-blocking mode.

```cpp
void setRecvBufferSize(SocketHandle sock, int size);
```
Set the socket receive buffer size via `SO_RCVBUF`.

---

### Address

**Header:** `<fishnet/utils/Address.h>`

```cpp
struct Address {
    sockaddr_in raw;

    Address();
    Address(const sockaddr_in& addr);
    Address(const std::string& ip, uint16_t port);

    std::string ip() const;
    uint16_t port() const;
    std::string toString() const;     // "192.168.0.1:19132"

    bool operator==(const Address& other) const;
    bool operator!=(const Address& other) const;

    struct Hash {
        size_t operator()(const Address& addr) const;
    };
};
```

| Member | Description |
|--------|-------------|
| `raw` | Underlying `sockaddr_in` structure |
| `Address()` | Default constructor, `0.0.0.0:0` |
| `Address(ip, port)` | Construct from IP string and port |
| `ip()` | Returns IP as dotted string |
| `port()` | Returns port in host byte order |
| `toString()` | Returns `"ip:port"` |
| `Hash` | For use in `std::unordered_map<Address, ..., Address::Hash>` |

---

### BinaryBuffer

**Header:** `<fishnet/utils/BinaryBuffer.h>`

```cpp
class BinaryBuffer {
public:
    BinaryBuffer();
    explicit BinaryBuffer(size_t reserveSize);
    BinaryBuffer(const uint8_t* data, size_t len);
```

#### Write Methods

| Method | Description |
|--------|-------------|
| `writeU8(uint8_t)` | Write 1 byte |
| `writeU16BE(uint16_t)` | Write 2 bytes big-endian |
| `writeU16LE(uint16_t)` | Write 2 bytes little-endian |
| `writeU24LE(uint32_t)` | Write 3 bytes little-endian |
| `writeU32BE(uint32_t)` | Write 4 bytes big-endian |
| `writeU64BE(uint64_t)` | Write 8 bytes big-endian |
| `writeBytes(const uint8_t*, size_t)` | Write raw bytes |
| `writeString(const std::string&)` | Write U16BE length prefix + string data |

#### Read Methods

| Method | Description |
|--------|-------------|
| `readU8()` | Read 1 byte |
| `readU16BE()` | Read 2 bytes big-endian |
| `readU16LE()` | Read 2 bytes little-endian |
| `readU24LE()` | Read 3 bytes little-endian |
| `readU32BE()` | Read 4 bytes big-endian |
| `readU64BE()` | Read 8 bytes big-endian |
| `readBytes(size_t)` | Read N bytes → `std::vector<uint8_t>` |
| `readString()` | Read U16BE length + string data |
| `skip(size_t)` | Advance read position |

#### Accessors

| Method | Description |
|--------|-------------|
| `data()` | Pointer to buffer start |
| `size()` | Total bytes written |
| `readPosition()` | Current read offset |
| `remaining()` | Bytes left to read |
| `empty()` | True if buffer is empty |
| `setReadPosition(size_t)` | Set read offset manually |
| `clear()` | Reset buffer and read position |
| `rawBuffer()` | Reference to internal `std::vector<uint8_t>` |

All read methods throw `std::out_of_range` on buffer overrun.

---

### Address Serialization

**Header:** `<fishnet/utils/AddressSerialization.h>`

```cpp
void writeAddress(BinaryBuffer& buf, const Address& addr);
```
Write address in RakNet wire format: `[0x04] [~IP0] [~IP1] [~IP2] [~IP3] [port BE]`. IP bytes are bitwise-inverted.

```cpp
bool readAddress(BinaryBuffer& buf, Address& out);
```
Read address from RakNet wire format. Returns `false` on error.

```cpp
void writeEmptyAddress(BinaryBuffer& buf);
```
Write a placeholder address (`0.0.0.0:0` in RakNet format).

---

### Constants

**Header:** `<fishnet/protocol/Constants.h>`

#### OFFLINE_MESSAGE_ID

```cpp
constexpr uint8_t OFFLINE_MESSAGE_ID[16] = {
    0x00, 0xFF, 0xFF, 0x00, 0xFE, 0xFE, 0xFE, 0xFE,
    0xFD, 0xFD, 0xFD, 0xFD, 0x12, 0x34, 0x56, 0x78
};
```
16-byte magic sequence used in all offline RakNet packets.

#### DEFAULT_MTU

```cpp
constexpr uint32_t DEFAULT_MTU = 1464;
```

#### Reliability

```cpp
enum class Reliability : uint8_t {
    Unreliable          = 0,
    UnreliableSequenced = 1,
    Reliable            = 2,
    ReliableOrdered     = 3,
    ReliableSequenced   = 7
};
```

| Value | Guaranteed delivery | Ordered |
|-------|-------------------|---------|
| `Unreliable` | No | No |
| `UnreliableSequenced` | No | Sequenced |
| `Reliable` | Yes | No |
| `ReliableOrdered` | Yes | Yes |
| `ReliableSequenced` | Yes | Sequenced |

#### Helper Functions

```cpp
bool reliabilityHasMessageIndex(uint8_t r);
bool reliabilityHasSequenceIndex(uint8_t r);
bool reliabilityHasOrderIndex(uint8_t r);
```

#### PacketId

```cpp
namespace PacketId {
    constexpr uint8_t ConnectedPing           = 0x00;
    constexpr uint8_t UnconnectedPing         = 0x01;
    constexpr uint8_t UnconnectedPingOpenConn = 0x02;
    constexpr uint8_t ConnectedPong           = 0x03;
    constexpr uint8_t OpenConnectionRequest1  = 0x05;
    constexpr uint8_t OpenConnectionReply1    = 0x06;
    constexpr uint8_t OpenConnectionRequest2  = 0x07;
    constexpr uint8_t OpenConnectionReply2    = 0x08;
    constexpr uint8_t ConnectionRequest       = 0x09;
    constexpr uint8_t ConnectionRequestAccepted = 0x10;
    constexpr uint8_t NewIncomingConnection    = 0x13;
    constexpr uint8_t DisconnectionNotification = 0x15;
    constexpr uint8_t UnconnectedPong         = 0x1C;
    constexpr uint8_t ACK = 0xC0;
    constexpr uint8_t NAK = 0xA0;
    constexpr uint8_t FrameSetMin = 0x80;
    constexpr uint8_t FrameSetMax = 0x8D;

    bool isFrameSet(uint8_t id);
}
```

---

### EncapsulatedPacket

**Header:** `<fishnet/protocol/EncapsulatedPacket.h>`

```cpp
struct EncapsulatedPacket {
    uint8_t reliability = 0;
    bool isSplit = false;
    uint32_t messageIndex = 0;
    uint32_t sequenceIndex = 0;
    uint32_t orderIndex = 0;
    uint8_t orderChannel = 0;
    uint32_t splitCount = 0;
    uint16_t splitId = 0;
    uint32_t splitIndex = 0;
    std::vector<uint8_t> payload;
};
```

Internal frame structure. Users normally don't interact with this directly.

---

### Connection

**Header:** `<fishnet/Connection.h>`

```cpp
class Connection {
public:
    Address address;
    uint64_t guid = 0;
    uint64_t lastPingTime = 0;
    bool connected = false;
    std::chrono::steady_clock::time_point lastActivity;

    Connection();
    Connection(const Address& addr, uint64_t guid);
    void touch();
    bool isTimedOut(uint64_t timeoutMs) const;
};
```

| Member | Description |
|--------|-------------|
| `address` | Peer's network address |
| `guid` | Peer's unique identifier |
| `connected` | `true` after full handshake |
| `lastActivity` | Timestamp of last received packet |
| `touch()` | Update `lastActivity` to now |
| `isTimedOut(ms)` | `true` if no activity for `ms` milliseconds |

---

### Callback Types

**Header:** `<fishnet/PacketHandler.h>`

```cpp
using PacketCallback = std::function<void(
    const uint8_t* data,
    size_t len,
    const Address& sender
)>;
```
Called when a user-level packet is received (after RakNet processing).

**Header:** `<fishnet/FishPeer.h>`

```cpp
using PongDataProvider = std::function<std::string()>;
```
Returns the string to include in unconnected pong responses. Return empty to ignore pings.

```cpp
using ConnectionCallback = std::function<void(const Address& peer, uint64_t guid)>;
```
Called when a new peer completes the connection handshake.

```cpp
using DisconnectCallback = std::function<void(const Address& peer, uint64_t guid)>;
```
Called when a peer disconnects (timeout or graceful).

---

### PeerConfig

**Header:** `<fishnet/FishPeer.h>`

```cpp
struct PeerConfig {
    uint32_t recvBufferSize      = 1024 * 1024;
    uint32_t connectionTimeoutMs = 10000;
    uint32_t pingIntervalMs      = 2000;
    uint32_t retransmitTimeoutMs = 1500;
    uint32_t maxPendingDatagrams = 512;
    uint32_t splitStaleTimeoutMs = 30000;
    uint32_t tickIntervalMs      = 50;
    uint16_t mtuSizes[3]         = {1464, 1172, 548};
};
```

| Field | Default | Description |
|-------|---------|-------------|
| `recvBufferSize` | 1 MB | Socket receive buffer size |
| `connectionTimeoutMs` | 10000 | Disconnect peer after N ms of silence |
| `pingIntervalMs` | 2000 | Send keep-alive ping every N ms |
| `retransmitTimeoutMs` | 1500 | Retransmit unacked datagrams after N ms |
| `maxPendingDatagrams` | 512 | Max unacked datagrams before dropping |
| `splitStaleTimeoutMs` | 30000 | Discard incomplete splits after N ms |
| `tickIntervalMs` | 50 | Tick loop interval (50ms = 20 ticks/sec) |
| `mtuSizes` | 1464, 1172, 548 | MTU sizes to try during discovery |

---

### FishPeer

**Header:** `<fishnet/FishPeer.h>`

The core networking engine. Usually accessed through FishServer/FishClient.

```cpp
class FishPeer {
public:
    explicit FishPeer(uint16_t port);
    ~FishPeer();
```

#### Lifecycle

| Method | Description |
|--------|-------------|
| `bool start()` | Bind socket, start receive + tick threads. Returns `false` on failure. |
| `void stop()` | Disconnect all peers, close socket, join threads. |
| `bool isRunning() const` | `true` if started and not stopped. |

#### Configuration

| Method | Description |
|--------|-------------|
| `void setConfig(const PeerConfig&)` | Set configuration. Call before `start()`. |
| `const PeerConfig& getConfig() const` | Get current configuration. |

#### Callbacks

| Method | Description |
|--------|-------------|
| `void setPacketCallback(PacketCallback)` | Receive user-level packets. |
| `void setPongDataProvider(PongDataProvider)` | Supply pong response string. |
| `void setConnectionCallback(ConnectionCallback)` | New connection established. |
| `void setDisconnectCallback(DisconnectCallback)` | Connection lost/closed. |

#### Sending

| Method | Description |
|--------|-------------|
| `void sendRaw(data, len, dest)` | Send raw UDP datagram. No reliability. |
| `void sendReliableOrdered(data, len, dest, channel=0)` | Send with guaranteed delivery + ordering. Auto-splits if larger than MTU. |
| `void sendUnreliable(data, len, dest)` | Send without delivery guarantee. |

#### Connection Management

| Method | Description |
|--------|-------------|
| `void disconnect(const Address&)` | Gracefully disconnect a peer. Sends 0x15. |
| `void disconnectAll()` | Disconnect all peers. |
| `bool isConnected(const Address&) const` | Check if peer is connected. |
| `size_t getConnectionCount() const` | Number of active connections. |
| `void connectTo(const Address&)` | Client-side: initiate handshake with MTU discovery. |

#### Accessors

| Method | Description |
|--------|-------------|
| `uint64_t getGuid() const` | This peer's unique identifier. |
| `uint16_t getPort() const` | Bound port (may differ from constructor if 0 was passed). |
| `uint32_t getMtu() const` | Current MTU size. |

---

### FishServer

**Header:** `<fishnet/FishServer.h>`

High-level server wrapper. All methods are thread-safe.

```cpp
class FishServer {
public:
    explicit FishServer(uint16_t port);
    ~FishServer();

    bool start();
    void stop();
    bool isRunning() const;

    void setPacketCallback(PacketCallback cb);
    void setPongDataProvider(PongDataProvider provider);
    void setConnectionCallback(ConnectionCallback cb);
    void setDisconnectCallback(DisconnectCallback cb);
    void setConfig(const PeerConfig& config);

    void sendTo(const uint8_t* data, size_t len, const Address& dest);
    void sendReliableTo(const uint8_t* data, size_t len, const Address& dest, uint8_t channel = 0);

    void disconnectPeer(const Address& peer);
    void disconnectAll();
    size_t getConnectionCount() const;

    FishPeer& getPeer();
    const FishPeer& getPeer() const;
};
```

---

### FishClient

**Header:** `<fishnet/FishClient.h>`

High-level client wrapper. Binds to port 0 (OS-assigned).

```cpp
class FishClient {
public:
    FishClient();
    ~FishClient();

    bool start();
    void stop();
    bool isRunning() const;
    bool isConnected() const;

    void connect(const std::string& host, uint16_t port);
    void disconnect();

    void setPacketCallback(PacketCallback cb);
    void setConnectionCallback(ConnectionCallback cb);
    void setDisconnectCallback(DisconnectCallback cb);
    void setConfig(const PeerConfig& config);

    void send(const uint8_t* data, size_t len);
    void sendReliable(const uint8_t* data, size_t len, uint8_t channel = 0);

    FishPeer& getPeer();
    const FishPeer& getPeer() const;
};
```

| Method | Description |
|--------|-------------|
| `connect(host, port)` | Initiate connection with MTU discovery. Non-blocking. |
| `disconnect()` | Send disconnect notification and close. |
| `isConnected()` | `true` after handshake completes. |
| `send(data, len)` | Send unreliable packet to server. |
| `sendReliable(data, len, channel)` | Send reliable ordered packet to server. |

---

### Packets

All packet structs live in `<fishnet/packets/*.h>`. Each provides static `encode()` and `decode()` methods.

#### UnconnectedPing (0x01)

```cpp
struct UnconnectedPing {
    uint64_t clientTimestamp = 0;
    uint64_t clientGuid = 0;
    static bool decode(const uint8_t* data, size_t len, UnconnectedPing& out);
};
```

#### UnconnectedPong (0x1C)

```cpp
struct UnconnectedPong {
    uint64_t pingTimestamp = 0;
    uint64_t serverGuid = 0;
    std::string serverData;
    static BinaryBuffer encode(uint64_t pingTimestamp, uint64_t serverGuid, const std::string& serverData);
    static bool decode(const uint8_t* data, size_t len, UnconnectedPong& out);
};
```

#### OpenConnectionRequest1 (0x05)

```cpp
struct OpenConnectionRequest1 {
    uint8_t protocolVersion = 0;
    uint16_t mtuSize = DEFAULT_MTU;
    static bool decode(const uint8_t* data, size_t len, OpenConnectionRequest1& out);
    static BinaryBuffer encode(uint8_t protocolVersion, uint16_t mtuSize);
};
```

#### OpenConnectionReply1 (0x06)

```cpp
struct OpenConnectionReply1 {
    uint64_t serverGuid = 0;
    bool useSecurity = false;
    uint16_t mtuSize = DEFAULT_MTU;
    static BinaryBuffer encode(uint64_t serverGuid, bool useSecurity, uint16_t mtuSize);
    static bool decode(const uint8_t* data, size_t len, OpenConnectionReply1& out);
};
```

#### OpenConnectionRequest2 (0x07)

```cpp
struct OpenConnectionRequest2 {
    Address serverAddress;
    uint16_t mtuSize = DEFAULT_MTU;
    uint64_t clientGuid = 0;
    static bool decode(const uint8_t* data, size_t len, OpenConnectionRequest2& out);
    static BinaryBuffer encode(const Address& serverAddr, uint16_t mtuSize, uint64_t clientGuid);
};
```

#### OpenConnectionReply2 (0x08)

```cpp
struct OpenConnectionReply2 {
    uint64_t serverGuid = 0;
    Address clientAddress;
    uint16_t mtuSize = DEFAULT_MTU;
    bool encryptionEnabled = false;
    static BinaryBuffer encode(uint64_t serverGuid, const Address& clientAddr, uint16_t mtuSize, bool encryption);
    static bool decode(const uint8_t* data, size_t len, OpenConnectionReply2& out);
};
```

#### ConnectionRequest (0x09)

```cpp
struct ConnectionRequest {
    uint64_t clientGuid = 0;
    uint64_t requestTimestamp = 0;
    bool useSecurity = false;
    static bool decode(const uint8_t* data, size_t len, ConnectionRequest& out);
    static BinaryBuffer encode(uint64_t clientGuid, uint64_t timestamp, bool useSecurity);
};
```

#### ConnectionRequestAccepted (0x10)

```cpp
struct ConnectionRequestAccepted {
    Address clientAddress;
    uint16_t systemIndex = 0;
    uint64_t requestTimestamp = 0;
    uint64_t acceptedTimestamp = 0;
    static BinaryBuffer encode(const Address& clientAddr, uint64_t requestTimestamp, uint64_t acceptedTimestamp);
    static bool decode(const uint8_t* data, size_t len, ConnectionRequestAccepted& out);
};
```

#### NewIncomingConnection (0x13)

```cpp
struct NewIncomingConnection {
    Address serverAddress;
    uint64_t requestTimestamp = 0;
    uint64_t acceptedTimestamp = 0;
    static BinaryBuffer encode(const Address& serverAddr, uint64_t requestTimestamp, uint64_t acceptedTimestamp);
    static bool decode(const uint8_t* data, size_t len, NewIncomingConnection& out);
};
```

#### ConnectedPing (0x00)

```cpp
struct ConnectedPing {
    uint64_t timestamp = 0;
    static BinaryBuffer encode(uint64_t timestamp);
    static bool decode(const uint8_t* data, size_t len, ConnectedPing& out);
};
```

#### ConnectedPong (0x03)

```cpp
struct ConnectedPong {
    uint64_t pingTimestamp = 0;
    uint64_t pongTimestamp = 0;
    static BinaryBuffer encode(uint64_t pingTimestamp, uint64_t pongTimestamp);
    static bool decode(const uint8_t* data, size_t len, ConnectedPong& out);
};
```

#### DisconnectionNotification (0x15)

```cpp
struct DisconnectionNotification {
    static BinaryBuffer encode();
};
```

#### AckPacket (0xC0) / NakPacket (0xA0)

```cpp
struct AckPacket {
    static BinaryBuffer encodeSingle(uint32_t sequenceNumber);
    static BinaryBuffer encodeRange(uint32_t first, uint32_t last);
};

struct NakPacket {
    static BinaryBuffer encodeSingle(uint32_t sequenceNumber);
};

std::vector<uint32_t> parseAckNak(const uint8_t* data, size_t len);
```

---

## Namespace `fishnet::bedrock`

### BedrockMotd

**Header:** `<fishnet-bedrock/BedrockMotd.h>`

```cpp
struct BedrockMotd {
    std::string serverName = "Dedicated Server";
    int protocolVersion = 0;
    std::string gameVersion = "0.0.0";
    int currentPlayers = 0;
    int maxPlayers = 20;
    uint64_t serverId = 0;
    std::string levelName = "Bedrock level";
    std::string gameMode = "Survival";
    int gameModeNumeric = 1;
    uint16_t portV4 = 19132;
    uint16_t portV6 = 19133;

    std::string build() const;
};
```

`build()` returns: `"MCPE;name;proto;ver;players;max;id;level;mode;modeNum;portV4;portV6;"`

---

### GamePacket

**Header:** `<fishnet-bedrock/GamePacket.h>`

#### Constants

```cpp
constexpr uint8_t GAME_PACKET_ID = 0xFE;
```

#### CompressionMethod

```cpp
enum class CompressionMethod : uint8_t {
    Zlib   = 0x00,
    Snappy = 0x01,
    None   = 0xFF
};
```

#### SubPacket

```cpp
struct SubPacket {
    uint32_t packetId = 0;
    std::vector<uint8_t> payload;
};
```

#### Function Pointers

```cpp
using CompressFunc   = bool(*)(const uint8_t* in, size_t inLen, std::vector<uint8_t>& out);
using DecompressFunc = bool(*)(const uint8_t* in, size_t inLen, std::vector<uint8_t>& out);
```

#### GamePacket struct

```cpp
struct GamePacket {
    static std::vector<uint8_t> wrap(
        const std::vector<SubPacket>& subPackets,
        CompressionMethod method = CompressionMethod::None,
        CompressFunc compressor = nullptr
    );

    static bool unwrap(
        const uint8_t* data, size_t len,
        std::vector<SubPacket>& out,
        DecompressFunc decompressor = nullptr
    );

    static bool isGamePacket(const uint8_t* data, size_t len);
};
```

| Method | Description |
|--------|-------------|
| `wrap(packets, method, compressor)` | Batch sub-packets into a `0xFE` frame with optional compression. Returns raw bytes ready to send. |
| `unwrap(data, len, out, decompressor)` | Decode a `0xFE` frame into sub-packets. Returns `false` on malformed data. |
| `isGamePacket(data, len)` | Returns `true` if data starts with `0xFE` and has at least 2 bytes. |

#### VarInt Helpers

```cpp
size_t writeVarInt(std::vector<uint8_t>& out, uint32_t value);
uint32_t readVarInt(const uint8_t* data, size_t len, size_t& offset);
```

---

### BedrockServer

**Header:** `<fishnet-bedrock/BedrockServer.h>`

```cpp
using GamePacketCallback = std::function<void(
    uint32_t packetId,
    const std::vector<uint8_t>& payload,
    const fishnet::Address& sender
)>;
```

```cpp
class BedrockServer {
public:
    explicit BedrockServer(uint16_t port = 19132);
    ~BedrockServer();

    bool start();
    void stop();
    bool isRunning() const;
```

#### MOTD Configuration

| Method | Description |
|--------|-------------|
| `setServerName(name)` | Server name shown in server list |
| `setProtocolVersion(int)` | Protocol version (e.g. 729) |
| `setGameVersion(string)` | Game version (e.g. "1.21.50") |
| `setPlayerCount(current, max)` | Player count shown in list |
| `setLevelName(name)` | Level name |
| `setGameMode(name, numeric)` | Game mode ("Survival", 1) |

#### Callbacks

| Method | Description |
|--------|-------------|
| `setGamePacketCallback(cb)` | Receive decoded sub-packets from `0xFE` frames |
| `setConnectionCallback(cb)` | New player connected |
| `setDisconnectCallback(cb)` | Player disconnected |

#### Sending

| Method | Description |
|--------|-------------|
| `sendGamePacket(packetId, data, len, dest)` | Send one sub-packet wrapped in `0xFE` |
| `sendGamePackets(vector<SubPacket>, dest)` | Send multiple sub-packets batched in one `0xFE` |
| `sendRawPacket(data, len, dest)` | Send raw bytes (no `0xFE` wrapping) |

#### Compression

| Method | Description |
|--------|-------------|
| `setCompressor(CompressFunc)` | Set compression function |
| `setDecompressor(DecompressFunc)` | Set decompression function |
| `setCompressionMethod(CompressionMethod)` | Set method for outgoing packets |

#### Connection Management

| Method | Description |
|--------|-------------|
| `disconnectPeer(addr)` | Disconnect a specific player |
| `disconnectAll()` | Disconnect all players |
| `getConnectionCount()` | Number of connected players |
| `setConfig(PeerConfig)` | Set peer configuration |
| `getFishServer()` | Access underlying FishServer |
| `getMotd()` | Access BedrockMotd struct |

---

### BedrockClient

**Header:** `<fishnet-bedrock/BedrockClient.h>`

```cpp
class BedrockClient {
public:
    BedrockClient();
    ~BedrockClient();

    bool start();
    void stop();
    bool isRunning() const;
    bool isConnected() const;

    void connect(const std::string& host, uint16_t port = 19132);
    void disconnect();
```

#### Callbacks

| Method | Description |
|--------|-------------|
| `setGamePacketCallback(cb)` | Receive decoded sub-packets from server |
| `setConnectionCallback(cb)` | Connected to server |
| `setDisconnectCallback(cb)` | Disconnected from server |

#### Sending

| Method | Description |
|--------|-------------|
| `sendGamePacket(packetId, data, len)` | Send one sub-packet wrapped in `0xFE` to server |
| `sendGamePackets(vector<SubPacket>)` | Send multiple sub-packets batched |
| `sendRawPacket(data, len)` | Send raw bytes to server |

#### Compression

| Method | Description |
|--------|-------------|
| `setCompressor(CompressFunc)` | Set compression function |
| `setDecompressor(DecompressFunc)` | Set decompression function |
| `setCompressionMethod(CompressionMethod)` | Set method for outgoing packets |

#### Other

| Method | Description |
|--------|-------------|
| `setConfig(PeerConfig)` | Set peer configuration |
| `getFishClient()` | Access underlying FishClient |
