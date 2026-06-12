#include <fishnet/bedrock/BedrockEncryption.h>

#include <openssl/bio.h>
#include <openssl/bn.h>
#include <openssl/ec.h>
#include <openssl/ecdh.h>
#include <openssl/ecdsa.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rand.h>
#include <openssl/sha.h>
#include <openssl/x509.h>

#include <array>
#include <cstring>

namespace fishnet::bedrock {
namespace {

std::string base64UrlEncode(const uint8_t* data, size_t len) {
    std::string encoded = BedrockEncryption::base64Encode(data, len);
    for (char& c : encoded) {
        if (c == '+') c = '-';
        else if (c == '/') c = '_';
    }
    while (!encoded.empty() && encoded.back() == '=') {
        encoded.pop_back();
    }
    return encoded;
}

std::vector<uint8_t> base64UrlDecode(const std::string& input) {
    std::string normalized = input;
    for (char& c : normalized) {
        if (c == '-') c = '+';
        else if (c == '_') c = '/';
    }
    while (normalized.size() % 4 != 0) {
        normalized.push_back('=');
    }
    return BedrockEncryption::base64Decode(normalized);
}

EVP_PKEY* loadPrivateKey(const std::vector<uint8_t>& der) {
    const uint8_t* ptr = der.data();
    return d2i_PrivateKey(EVP_PKEY_EC, nullptr, &ptr, static_cast<long>(der.size()));
}

EVP_PKEY* loadPublicKeyFromDer(const std::vector<uint8_t>& der) {
    const uint8_t* ptr = der.data();
    return d2i_PUBKEY(nullptr, &ptr, static_cast<long>(der.size()));
}

EVP_PKEY* loadPublicKeyFromBase64Spki(const std::string& base64Key) {
    auto der = base64UrlDecode(base64Key);
    return loadPublicKeyFromDer(der);
}

std::vector<uint8_t> derSignatureToRaw(const uint8_t* der, size_t derLen, size_t fieldSize) {
    const uint8_t* ptr = der;
    ECDSA_SIG* sig = d2i_ECDSA_SIG(nullptr, &ptr, static_cast<long>(derLen));
    if (!sig) {
        return {};
    }

    const BIGNUM* r = nullptr;
    const BIGNUM* s = nullptr;
    ECDSA_SIG_get0(sig, &r, &s);

    std::vector<uint8_t> raw(fieldSize * 2, 0);
    BN_bn2binpad(r, raw.data(), static_cast<int>(fieldSize));
    BN_bn2binpad(s, raw.data() + fieldSize, static_cast<int>(fieldSize));
    ECDSA_SIG_free(sig);
    return raw;
}

bool aesCtrCrypt(const std::vector<uint8_t>& key, const uint8_t* input, size_t inputLen, std::vector<uint8_t>& output) {
    if (key.size() < 16 || inputLen == 0) {
        return false;
    }

    std::array<uint8_t, 16> iv{};
    std::memcpy(iv.data(), key.data(), 12);
    iv[15] = 2;

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        return false;
    }

    output.assign(inputLen, 0);
    int outLen = 0;
    int totalLen = 0;
    bool ok = EVP_EncryptInit_ex(ctx, EVP_aes_256_ctr(), nullptr, key.data(), iv.data()) == 1
           && EVP_EncryptUpdate(ctx, output.data(), &outLen, input, static_cast<int>(inputLen)) == 1
           && (totalLen = outLen) >= 0
           && EVP_EncryptFinal_ex(ctx, output.data() + totalLen, &outLen) == 1;
    totalLen += outLen;
    output.resize(static_cast<size_t>(totalLen));
    EVP_CIPHER_CTX_free(ctx);
    return ok;
}

std::array<uint8_t, 8> makeTrailer(uint64_t counter, const uint8_t* payload, size_t payloadLen, const std::vector<uint8_t>& key) {
    std::array<uint8_t, 8> counterBytes{};
    for (int i = 0; i < 8; ++i) {
        counterBytes[static_cast<size_t>(i)] = static_cast<uint8_t>((counter >> (8 * i)) & 0xFF);
    }

    SHA256_CTX sha{};
    SHA256_Init(&sha);
    SHA256_Update(&sha, counterBytes.data(), counterBytes.size());
    SHA256_Update(&sha, payload, payloadLen);
    SHA256_Update(&sha, key.data(), key.size());

    std::array<uint8_t, 32> digest{};
    SHA256_Final(digest.data(), &sha);

    std::array<uint8_t, 8> trailer{};
    std::memcpy(trailer.data(), digest.data(), trailer.size());
    return trailer;
}

} // namespace

std::vector<uint8_t> BedrockEncryption::generateRandomBytes(size_t count) {
    std::vector<uint8_t> out(count);
    RAND_bytes(out.data(), static_cast<int>(count));
    return out;
}

bool BedrockEncryption::generateEcKeyPair(std::vector<uint8_t>& outPrivateDer, std::vector<uint8_t>& outPublicDer) {
    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_EC, nullptr);
    if (!ctx) {
        return false;
    }

    bool ok = EVP_PKEY_paramgen_init(ctx) > 0
           && EVP_PKEY_CTX_set_ec_paramgen_curve_nid(ctx, NID_secp384r1) > 0;

    EVP_PKEY* params = nullptr;
    ok = ok && EVP_PKEY_paramgen(ctx, &params) > 0;
    EVP_PKEY_CTX_free(ctx);
    if (!ok || !params) {
        EVP_PKEY_free(params);
        return false;
    }

    EVP_PKEY_CTX* keyCtx = EVP_PKEY_CTX_new(params, nullptr);
    EVP_PKEY* pkey = nullptr;
    ok = keyCtx
      && EVP_PKEY_keygen_init(keyCtx) > 0
      && EVP_PKEY_keygen(keyCtx, &pkey) > 0;

    if (keyCtx) {
        EVP_PKEY_CTX_free(keyCtx);
    }
    EVP_PKEY_free(params);

    if (!ok || !pkey) {
        EVP_PKEY_free(pkey);
        return false;
    }

    int privateLen = i2d_PrivateKey(pkey, nullptr);
    int publicLen = i2d_PUBKEY(pkey, nullptr);
    if (privateLen <= 0 || publicLen <= 0) {
        EVP_PKEY_free(pkey);
        return false;
    }

    outPrivateDer.resize(static_cast<size_t>(privateLen));
    outPublicDer.resize(static_cast<size_t>(publicLen));
    uint8_t* privatePtr = outPrivateDer.data();
    uint8_t* publicPtr = outPublicDer.data();
    i2d_PrivateKey(pkey, &privatePtr);
    i2d_PUBKEY(pkey, &publicPtr);
    EVP_PKEY_free(pkey);
    return true;
}

std::string BedrockEncryption::createHandshakeJwt(const std::vector<uint8_t>& serverPrivateDer,
                                                    const std::vector<uint8_t>& serverPublicDer,
                                                    const std::vector<uint8_t>& salt) {
    EVP_PKEY* privateKey = loadPrivateKey(serverPrivateDer);
    if (!privateKey) {
        return {};
    }

    const std::string headerJson = R"({"alg":"ES384","x5u":")"
        + base64Encode(serverPublicDer.data(), serverPublicDer.size()) + R"("})";
    const std::string payloadJson = R"({"salt":")"
        + base64Encode(salt.data(), salt.size()) + R"("})";

    const std::string headerPart = base64UrlEncode(reinterpret_cast<const uint8_t*>(headerJson.data()), headerJson.size());
    const std::string payloadPart = base64UrlEncode(reinterpret_cast<const uint8_t*>(payloadJson.data()), payloadJson.size());
    const std::string signingInput = headerPart + '.' + payloadPart;

    EVP_MD_CTX* mdCtx = EVP_MD_CTX_new();
    size_t sigLen = 0;
    std::vector<uint8_t> derSig(128);
    bool ok = mdCtx
           && EVP_DigestSignInit(mdCtx, nullptr, EVP_sha384(), nullptr, privateKey) == 1
           && EVP_DigestSign(mdCtx, nullptr, &sigLen,
                             reinterpret_cast<const uint8_t*>(signingInput.data()), signingInput.size()) == 1;
    if (ok) {
        derSig.resize(sigLen);
        ok = EVP_DigestSign(mdCtx, derSig.data(), &sigLen,
                            reinterpret_cast<const uint8_t*>(signingInput.data()), signingInput.size()) == 1;
        derSig.resize(sigLen);
    }

    if (mdCtx) {
        EVP_MD_CTX_free(mdCtx);
    }
    EVP_PKEY_free(privateKey);

    if (!ok) {
        return {};
    }

    const auto rawSig = derSignatureToRaw(derSig.data(), derSig.size(), 48);
    if (rawSig.empty()) {
        return {};
    }

    return signingInput + '.' + base64UrlEncode(rawSig.data(), rawSig.size());
}

std::vector<uint8_t> BedrockEncryption::deriveAesKey(const std::vector<uint8_t>& serverPrivateDer,
                                                     const std::string& clientPublicKeyBase64,
                                                     const std::vector<uint8_t>& salt) {
    EVP_PKEY* localKey = loadPrivateKey(serverPrivateDer);
    EVP_PKEY* remoteKey = loadPublicKeyFromBase64Spki(clientPublicKeyBase64);
    if (!localKey || !remoteKey) {
        EVP_PKEY_free(localKey);
        EVP_PKEY_free(remoteKey);
        return {};
    }

    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new(localKey, nullptr);
    if (!ctx || EVP_PKEY_derive_init(ctx) <= 0 || EVP_PKEY_derive_set_peer(ctx, remoteKey) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        EVP_PKEY_free(localKey);
        EVP_PKEY_free(remoteKey);
        return {};
    }

    size_t secretLen = 0;
    if (EVP_PKEY_derive(ctx, nullptr, &secretLen) <= 0 || secretLen == 0) {
        EVP_PKEY_CTX_free(ctx);
        EVP_PKEY_free(localKey);
        EVP_PKEY_free(remoteKey);
        return {};
    }

    std::vector<uint8_t> sharedSecret(secretLen);
    if (EVP_PKEY_derive(ctx, sharedSecret.data(), &secretLen) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        EVP_PKEY_free(localKey);
        EVP_PKEY_free(remoteKey);
        return {};
    }
    sharedSecret.resize(secretLen);

    EVP_PKEY_CTX_free(ctx);
    EVP_PKEY_free(localKey);
    EVP_PKEY_free(remoteKey);

    SHA256_CTX sha{};
    SHA256_Init(&sha);
    SHA256_Update(&sha, salt.data(), salt.size());
    SHA256_Update(&sha, sharedSecret.data(), sharedSecret.size());

    std::vector<uint8_t> key(32);
    SHA256_Final(key.data(), &sha);
    return key;
}

bool BedrockEncryption::encryptPayload(const uint8_t* data, size_t len, PeerCryptoState& state, std::vector<uint8_t>& out) {
    if (!state.encryptionEnabled || state.aesKey.size() != 32 || len == 0) {
        return false;
    }

    const auto trailer = makeTrailer(state.sendCounter, data, len, state.aesKey);
    std::vector<uint8_t> plain(len + trailer.size());
    std::memcpy(plain.data(), data, len);
    std::memcpy(plain.data() + len, trailer.data(), trailer.size());

    state.sendCounter++;
    return aesCtrCrypt(state.aesKey, plain.data(), plain.size(), out);
}

bool BedrockEncryption::decryptPayload(const uint8_t* data, size_t len, PeerCryptoState& state, std::vector<uint8_t>& out) {
    if (!state.encryptionEnabled || state.aesKey.size() != 32 || len <= 8) {
        return false;
    }

    std::vector<uint8_t> decrypted;
    if (!aesCtrCrypt(state.aesKey, data, len, decrypted)) {
        return false;
    }

    const size_t payloadLen = decrypted.size() - 8;
    const auto expectedTrailer = makeTrailer(state.recvCounter, decrypted.data(), payloadLen, state.aesKey);
    if (std::memcmp(expectedTrailer.data(), decrypted.data() + payloadLen, expectedTrailer.size()) != 0) {
        return false;
    }

    decrypted.resize(payloadLen);
    state.recvCounter++;
    out = std::move(decrypted);
    return true;
}

std::string BedrockEncryption::base64Encode(const uint8_t* data, size_t len) {
    BIO* b64 = BIO_new(BIO_f_base64());
    BIO* mem = BIO_new(BIO_s_mem());
    b64 = BIO_push(b64, mem);
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
    BIO_write(b64, data, static_cast<int>(len));
    BIO_flush(b64);

    char* encoded = nullptr;
    const long encodedLen = BIO_get_mem_data(mem, &encoded);
    std::string result(encoded, encodedLen > 0 ? static_cast<size_t>(encodedLen) : 0);
    BIO_free_all(b64);
    return result;
}

std::vector<uint8_t> BedrockEncryption::base64Decode(const std::string& encoded) {
    BIO* b64 = BIO_new(BIO_f_base64());
    BIO* mem = BIO_new_mem_buf(encoded.data(), static_cast<int>(encoded.size()));
    b64 = BIO_push(b64, mem);
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);

    std::vector<uint8_t> out((encoded.size() * 3) / 4 + 4);
    const int decodedLen = BIO_read(b64, out.data(), static_cast<int>(out.size()));
    BIO_free_all(b64);
    if (decodedLen <= 0) {
        return {};
    }
    out.resize(static_cast<size_t>(decodedLen));
    return out;
}

} // namespace fishnet::bedrock
