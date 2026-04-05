#pragma once

#include <GRPCVehicleHardware.h>
#include <VehicleHalTypes.h>

#include "GatewayConfig.h"
#include "NodeForwarder.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// VhalGateway polls property values from the local vhal-core at a fixed interval and
// distributes changed values to one NodeForwarder per configured remote node.
//
// Polling avoids subscription/callback complexity: no RecurrentTimer on the server,
// no stream back-pressure interfering with SetValues latency.
//
// Lifecycle:
//   VhalGateway gw("localhost:50051", config);
//   gw.start();   // blocks until shutdown
class VhalGateway {
public:
    VhalGateway(const std::string& localAddr, const GatewayConfig& config);
    ~VhalGateway() = default;

    VhalGateway(const VhalGateway&) = delete;
    VhalGateway& operator=(const VhalGateway&) = delete;

    // Connects to local vhal-core and blocks in the poll loop.
    void start();

private:
    // Poll all monitored properties once; forward any values that changed since
    // the last poll.
    void pollOnce();

    // How often to poll (milliseconds). 100 ms ≈ 10 Hz, fast enough for UI updates.
    static constexpr int kPollIntervalMs = 100;

    std::string                                                               mLocalAddr;
    std::unordered_set<int32_t>                                               mAllPropIds;
    std::vector<std::unique_ptr<NodeForwarder>>                               mForwarders;
    std::unique_ptr<android::hardware::automotive::vehicle::virtualization
                    ::GRPCVehicleHardware>                                    mLocalHardware;

    // Keyed by propId; stores the last forwarded value for change detection.
    std::unordered_map<int32_t,
        aidl::android::hardware::automotive::vehicle::VehiclePropValue>       mPreviousValues;

    // GetValueRequests built once from mAllPropIds; reused every poll cycle.
    std::vector<aidl::android::hardware::automotive::vehicle::GetValueRequest> mGetRequests;
};
