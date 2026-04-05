#define LOG_TAG "VhalGateway"

#include "GatewayConfig.h"
#include "VhalGateway.h"

#include <android-base/logging.h>

#include <string>

// Default paths — overridable at compile time via -DGATEWAY_CONFIG_ROOT=...
#ifndef GATEWAY_CONFIG_ROOT
#define GATEWAY_CONFIG_ROOT "/opt/car-ui/etc/vhal"
#endif

static constexpr const char* kDefaultConfigFile =
    GATEWAY_CONFIG_ROOT "/gateway-configs.json";

static constexpr const char* kDefaultLocalAddr = "localhost:50051";

int main(int argc, char* argv[]) {
    std::string localAddr  = kDefaultLocalAddr;
    std::string configFile = kDefaultConfigFile;

    if (argc > 1) { localAddr  = argv[1]; }
    if (argc > 2) { configFile = argv[2]; }

    LOG(INFO) << "[VhalGateway] local VHAL: " << localAddr;
    LOG(INFO) << "[VhalGateway] config:     " << configFile;

    GatewayConfig config;
    std::string err = GatewayConfig::loadFromFile(configFile, config);
    if (!err.empty()) {
        LOG(ERROR) << "[VhalGateway] Failed to load config: " << err;
        return 1;
    }

    LOG(INFO) << "[VhalGateway] config v" << config.version
              << " loaded — " << config.nodes.size() << " node(s)";

    VhalGateway gateway(localAddr, config);
    gateway.start();

    return 0;
}
