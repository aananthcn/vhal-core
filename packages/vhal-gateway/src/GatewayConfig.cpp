#include "GatewayConfig.h"

#include <json/json.h>

#include <fstream>
#include <stdexcept>

std::string GatewayConfig::loadFromFile(const std::string& path, GatewayConfig& out) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return "Cannot open config file: " + path;
    }

    Json::Value root;
    Json::CharReaderBuilder builder;
    std::string errs;
    if (!Json::parseFromStream(builder, file, &root, &errs)) {
        return "JSON parse error in " + path + ": " + errs;
    }

    if (!root.isMember("version") || !root["version"].isString()) {
        return "Missing or invalid 'version' field";
    }
    out.version = root["version"].asString();

    if (!root.isMember("gatewayNodes") || !root["gatewayNodes"].isArray()) {
        return "Missing or invalid 'gatewayNodes' array";
    }

    for (const auto& nodeJson : root["gatewayNodes"]) {
        if (!nodeJson.isMember("ipaddr") || !nodeJson["ipaddr"].isString()) {
            return "A gatewayNode is missing 'ipaddr'";
        }
        if (!nodeJson.isMember("messages") || !nodeJson["messages"].isArray()) {
            return "A gatewayNode is missing 'messages'";
        }

        GatewayNodeConfig node;
        node.ipaddr = nodeJson["ipaddr"].asString();

        for (const auto& msgJson : nodeJson["messages"]) {
            if (!msgJson.isMember("msgId") || !msgJson["msgId"].isString()) {
                return "A message entry is missing 'msgId'";
            }
            if (!msgJson.isMember("properties") || !msgJson["properties"].isArray()) {
                return "A message entry is missing 'properties'";
            }

            GatewayMessage msg;
            msg.msgId = msgJson["msgId"].asString();

            for (const auto& propJson : msgJson["properties"]) {
                if (!propJson.isMember("id") || !propJson["id"].isString()) {
                    return "A property entry in message '" + msg.msgId + "' is missing 'id'";
                }
                if (!propJson.isMember("desc") || !propJson["desc"].isString()) {
                    return "A property entry in message '" + msg.msgId + "' is missing 'desc'";
                }

                GatewayProperty prop;
                prop.desc = propJson["desc"].asString();

                const std::string idStr = propJson["id"].asString();
                try {
                    // stoul with base 0 auto-detects the "0x" prefix.
                    prop.id = static_cast<int32_t>(std::stoul(idStr, nullptr, 0));
                } catch (const std::exception&) {
                    return "Invalid property id '" + idStr + "' in message '" + msg.msgId + "'";
                }

                msg.properties.push_back(std::move(prop));
            }

            node.messages.push_back(std::move(msg));
        }

        out.nodes.push_back(std::move(node));
    }

    return "";
}
