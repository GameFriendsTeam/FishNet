#pragma once

/*
 * Bedrock MOTD Builder
 *
 * Constructs the server list ping response string in Bedrock format:
 * "MCPE;name;proto;ver;players;max;id;level;mode;modeNum;portV4;portV6;"
 * Users configure fields via setters, call build() to get the string.
 */

#include <fishnet/platform.h>
#include <string>
#include <sstream>
#include <cstdint>

namespace fishnet::bedrock {

struct FISHNET_API BedrockMotd {
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

    /// Build the MOTD string in Bedrock format
    std::string build() const {
        std::string escapedName;
        for (char c : serverName) {
            if (c == ';') escapedName += "\\;";
            else escapedName += c;
        }

        std::ostringstream oss;
        oss << "MCPE;"
            << escapedName << ";"
            << protocolVersion << ";"
            << gameVersion << ";"
            << currentPlayers << ";"
            << maxPlayers << ";"
            << serverId << ";"
            << levelName << ";"
            << gameMode << ";"
            << gameModeNumeric << ";"
            << portV4 << ";"
            << portV6 << ";";

        std::string result = oss.str();
        if (result.size() > 1024) {
            result.resize(1024);
        }
        return result;
    }
};

} // namespace fishnet::bedrock
