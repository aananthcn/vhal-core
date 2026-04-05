#pragma once

// VehicleServer.grpc.pb.h must come before any grpc networking header so that
// StatusCode.pb.h (included transitively) is compiled before <netdb.h> gets a
// chance to #define TRY_AGAIN — which would break the StatusCode enum value.
#include "VehicleServer.grpc.pb.h"

#include <VehicleHalTypes.h>

#include "GatewayConfig.h"

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace grpc { class Channel; }

namespace proto    = ::android::hardware::automotive::vehicle::proto;
namespace aidlvhal = ::aidl::android::hardware::automotive::vehicle;

// Forwards grouped VHAL property changes to one remote node.
//
// Each GatewayMessage in the node config is a named group of property IDs.
// When a change event arrives, changed values are partitioned by message group
// and each group with ≥1 changed value is enqueued as a separate SetValues call.
// This keeps logically distinct messages separate on the wire.
//
// One NodeForwarder is created per gatewayNode entry. Its worker thread owns
// all remote I/O so the local property-change callback thread never blocks.
class NodeForwarder {
public:
    explicit NodeForwarder(const GatewayNodeConfig& config);
    ~NodeForwarder();

    NodeForwarder(const NodeForwarder&) = delete;
    NodeForwarder& operator=(const NodeForwarder&) = delete;

    // Called from the property-change callback thread.
    // Partitions values by message group, enqueues one batch per group that
    // has ≥1 matching value. Returns immediately.
    void forward(const std::vector<aidlvhal::VehiclePropValue>& values);

    const std::string& remoteAddr() const { return mRemoteAddr; }

private:
    // One entry in the worker queue: a named message group + its changed values.
    struct ForwardBatch {
        std::string                            msgId;
        std::vector<aidlvhal::VehiclePropValue> values;
    };

    struct MessageFilter {
        std::string                  msgId;
        std::unordered_set<int32_t>  propIds;
    };

    void workerLoop();

    std::string                                mRemoteAddr;
    std::vector<MessageFilter>                 mMessages;
    std::unordered_set<int32_t>                mAllPropIds;  // union — fast early-exit check
    std::shared_ptr<grpc::Channel>             mChannel;
    std::unique_ptr<proto::VehicleServer::Stub> mStub;

    std::mutex                    mQueueMutex;
    std::condition_variable       mQueueCv;
    std::queue<ForwardBatch>      mQueue;
    std::atomic<bool>             mRunning{false};
    std::thread                   mWorker;
};
