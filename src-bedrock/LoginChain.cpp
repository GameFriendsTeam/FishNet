#include <fishnet/bedrock/LoginChain.h>
#include <fishnet/bedrock/BedrockEncryption.h>

#include <cctype>
#include <string>

namespace fishnet::bedrock {
namespace {

std::string extractJsonStringField(const std::string& json, const std::string& field) {
    const std::string key = "\"" + field + "\"";
    size_t pos = json.find(key);
    if (pos == std::string::npos) {
        return {};
    }

    pos = json.find(':', pos + key.size());
    if (pos == std::string::npos) {
        return {};
    }
    ++pos;
    while (pos < json.size() && std::isspace(static_cast<unsigned char>(json[pos]))) {
        ++pos;
    }
    if (pos >= json.size() || json[pos] != '"') {
        return {};
    }
    ++pos;

    std::string value;
    while (pos < json.size()) {
        const char c = json[pos++];
        if (c == '"') {
            break;
        }
        if (c == '\\' && pos < json.size()) {
            value.push_back(json[pos++]);
        } else {
            value.push_back(c);
        }
    }
    return value;
}

std::string getJwtPayloadJson(const std::string& jwt) {
    const size_t first = jwt.find('.');
    if (first == std::string::npos) {
        return {};
    }
    const size_t second = jwt.find('.', first + 1);
    if (second == std::string::npos) {
        return {};
    }

    const std::string payloadPart = jwt.substr(first + 1, second - first - 1);
    const auto decoded = BedrockEncryption::base64Decode(
        [&payloadPart]() {
            std::string normalized = payloadPart;
            for (char& c : normalized) {
                if (c == '-') c = '+';
                else if (c == '_') c = '/';
            }
            while (normalized.size() % 4 != 0) {
                normalized.push_back('=');
            }
            return normalized;
        }());

    return std::string(decoded.begin(), decoded.end());
}

bool parseLegacyChain(const std::string& certificateJson, ParsedLoginIdentity& out) {
    const size_t chainPos = certificateJson.find("\"chain\"");
    if (chainPos == std::string::npos) {
        return false;
    }

    size_t pos = certificateJson.find('[', chainPos);
    if (pos == std::string::npos) {
        return false;
    }

    size_t end = certificateJson.find(']', pos);
    if (end == std::string::npos) {
        return false;
    }

    std::string lastJwt;
    pos = certificateJson.find('"', pos);
    while (pos != std::string::npos && pos < end) {
        const size_t tokenStart = pos + 1;
        const size_t tokenEnd = certificateJson.find('"', tokenStart);
        if (tokenEnd == std::string::npos || tokenEnd > end) {
            break;
        }
        lastJwt = certificateJson.substr(tokenStart, tokenEnd - tokenStart);
        pos = certificateJson.find('"', tokenEnd + 1);
    }

    if (lastJwt.empty()) {
        return false;
    }

    const std::string payload = getJwtPayloadJson(lastJwt);
    out.clientPublicKeyB64 = extractJsonStringField(payload, "identityPublicKey");
    out.displayName = extractJsonStringField(payload, "displayName");
    if (const auto extra = extractJsonStringField(payload, "extraData"); !extra.empty()) {
        if (out.displayName.empty()) {
            out.displayName = extractJsonStringField(extra, "displayName");
        }
        out.xuid = extractJsonStringField(extra, "XUID");
    }
    out.onlineMode = !out.clientPublicKeyB64.empty();
    return !out.clientPublicKeyB64.empty();
}

} // namespace

bool parseLoginChain(const std::string& chainJson, ParsedLoginIdentity& out) {
    out = {};

    const std::string token = extractJsonStringField(chainJson, "Token");
    if (!token.empty()) {
        const std::string payload = getJwtPayloadJson(token);
        out.clientPublicKeyB64 = extractJsonStringField(payload, "cpk");
        out.displayName = extractJsonStringField(payload, "xname");
        out.xuid = extractJsonStringField(payload, "xid");
        out.onlineMode = true;
        return !out.clientPublicKeyB64.empty();
    }

    const std::string certificate = extractJsonStringField(chainJson, "Certificate");
    if (!certificate.empty()) {
        return parseLegacyChain(certificate, out);
    }

    return false;
}

} // namespace fishnet::bedrock
