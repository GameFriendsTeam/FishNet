#pragma once

#include <fishnet/platform.h>
#include <string>

namespace fishnet::bedrock {

struct FISHNET_API ParsedLoginIdentity {
    std::string clientPublicKeyB64;
    std::string displayName;
    std::string xuid;
    bool onlineMode = false;
};

FISHNET_API bool parseLoginChain(const std::string& chainJson, ParsedLoginIdentity& out);

} // namespace fishnet::bedrock
