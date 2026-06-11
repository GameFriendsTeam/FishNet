#pragma once

#include <fishnet/platform.h>
#include <cstdint>
#include <string>
#include <vector>

namespace fishnet::bedrock {

struct FISHNET_API PeerCryptoState {
    bool encryptionEnabled = false;
    std::vector<uint8_t> aesKey;
    uint64_t sendCounter = 0;
    uint64_t recvCounter = 0;

    std::vector<uint8_t> serverPrivateKeyDer;
    std::vector<uint8_t> serverPublicKeyDer;
    std::vector<uint8_t> handshakeSalt;
    std::string clientPublicKeyB64;
};

class FISHNET_API BedrockEncryption {
public:
    static std::vector<uint8_t> generateRandomBytes(size_t count);
    static bool generateEcKeyPair(std::vector<uint8_t>& outPrivateDer, std::vector<uint8_t>& outPublicDer);

    static std::string createHandshakeJwt(const std::vector<uint8_t>& serverPrivateDer,
                                          const std::vector<uint8_t>& serverPublicDer,
                                          const std::vector<uint8_t>& salt);

    static std::vector<uint8_t> deriveAesKey(const std::vector<uint8_t>& serverPrivateDer,
                                            const std::string& clientPublicKeyBase64,
                                            const std::vector<uint8_t>& salt);

    static bool encryptPayload(const uint8_t* data, size_t len, PeerCryptoState& state, std::vector<uint8_t>& out);
    static bool decryptPayload(const uint8_t* data, size_t len, PeerCryptoState& state, std::vector<uint8_t>& out);

    static std::string base64Encode(const uint8_t* data, size_t len);
    static std::vector<uint8_t> base64Decode(const std::string& encoded);
};

} // namespace fishnet::bedrock
