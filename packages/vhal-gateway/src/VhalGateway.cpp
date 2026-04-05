#include "VhalGateway.h"

#include <IVehicleHardware.h>
#include <VehicleHalTypes.h>
#include <android-base/logging.h>

#include <chrono>
#include <thread>

namespace aidlvhal = ::aidl::android::hardware::automotive::vehicle;
using GRPCHardware =
    ::android::hardware::automotive::vehicle::virtualization::GRPCVehicleHardware;

VhalGateway::VhalGateway(const std::string& localAddr, const GatewayConfig& config)
    : mLocalAddr(localAddr)
{
    for (const auto& nodeConfig : config.nodes) {
        for (const auto& msg : nodeConfig.messages) {
            for (const auto& prop : msg.properties) {
                mAllPropIds.insert(prop.id);
            }
        }
        mForwarders.push_back(std::make_unique<NodeForwarder>(nodeConfig));
    }

    // Build the static GetValueRequest list once; reused on every poll cycle.
    int64_t requestId = 0;
    for (int32_t propId : mAllPropIds) {
        aidlvhal::GetValueRequest req;
        req.requestId  = requestId++;
        req.prop.prop  = propId;
        req.prop.areaId = 0;
        mGetRequests.push_back(req);
    }

    LOG(INFO) << "[VhalGateway] configured with " << mForwarders.size()
              << " remote node(s), " << mAllPropIds.size()
              << " unique prop IDs, local VHAL at " << mLocalAddr
              << ", poll interval " << kPollIntervalMs << " ms";
}

void VhalGateway::start() {
    mLocalHardware = std::make_unique<GRPCHardware>(mLocalAddr);

    LOG(INFO) << "[VhalGateway] connecting to local VHAL at " << mLocalAddr;
    mLocalHardware->waitForConnected(std::chrono::milliseconds(5000));
    LOG(INFO) << "[VhalGateway] connected, starting poll loop";

    while (true) {
        pollOnce();
        std::this_thread::sleep_for(std::chrono::milliseconds(kPollIntervalMs));
    }
}

void VhalGateway::pollOnce() {
    std::vector<aidlvhal::GetValueResult> results;

    auto status = mLocalHardware->getValues(
        std::make_shared<const android::hardware::automotive::vehicle::IVehicleHardware
                         ::GetValuesCallback>(
            [&results](std::vector<aidlvhal::GetValueResult> r) {
                results = std::move(r);
            }),
        mGetRequests);

    if (status != aidlvhal::StatusCode::OK) {
        LOG(WARNING) << "[VhalGateway] GetValues returned status "
                     << static_cast<int>(status) << ", skipping this cycle";
        return;
    }

    std::vector<aidlvhal::VehiclePropValue> changedValues;
    for (const auto& result : results) {
        if (result.status != aidlvhal::StatusCode::OK || !result.prop.has_value()) {
            continue;
        }
        const auto& val = result.prop.value();
        int32_t propId  = val.prop;

        auto it = mPreviousValues.find(propId);
        if (it == mPreviousValues.end() || it->second.value != val.value) {
            mPreviousValues[propId] = val;
            changedValues.push_back(val);
            LOG(INFO) << "[VhalGateway] change detected for prop 0x"
                      << std::hex << propId;
        }
    }

    if (changedValues.empty()) {
        return;
    }

    LOG(INFO) << "[VhalGateway] forwarding " << changedValues.size() << " changed prop(s)";
    for (auto& forwarder : mForwarders) {
        forwarder->forward(changedValues);
    }
}
