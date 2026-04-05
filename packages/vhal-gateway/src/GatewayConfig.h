#pragma once

#include <cstdint>
#include <string>
#include <vector>

// One property entry: a numeric ID (parsed from hex string) plus a human-readable label.
struct GatewayProperty {
    int32_t     id;    // e.g. 0x11400400 — parsed from the "0x..." string in JSON
    std::string desc;  // e.g. "GEAR_SELECTION" — informational only
};

// A named group of properties forwarded together in one SetValues call.
struct GatewayMessage {
    std::string                  msgId;       // e.g. "ivi-basic"
    std::vector<GatewayProperty> properties;
};

// One remote node entry: an IP address and the messages (property groups) to forward.
struct GatewayNodeConfig {
    std::string                 ipaddr;    // e.g. "192.168.1.20:50051"
    std::vector<GatewayMessage> messages;
};

struct GatewayConfig {
    std::string                   version;
    std::vector<GatewayNodeConfig> nodes;

    // Parses a gateway-configs.json at the given path.
    // Returns an empty string on success, or an error message on failure.
    static std::string loadFromFile(const std::string& path, GatewayConfig& out);
};
