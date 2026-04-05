#include "NodeForwarder.h"

#include "ProtoMessageConverter.h"

#include <grpcpp/grpcpp.h>
// <grpcpp/grpcpp.h> pulls in <netdb.h> which defines TRY_AGAIN as a macro.
// StatusCode.pb.h (included via NodeForwarder.h) is already compiled by this
// point, so the macro does not conflict — but clean it up anyway.
#ifdef TRY_AGAIN
#  undef TRY_AGAIN
#endif

#include <android-base/logging.h>

namespace proto_msg_converter = ::android::hardware::automotive::vehicle::proto_msg_converter;

NodeForwarder::NodeForwarder(const GatewayNodeConfig& config)
    : mRemoteAddr(config.ipaddr)
    , mChannel(grpc::CreateChannel(config.ipaddr, grpc::InsecureChannelCredentials()))
    , mStub(proto::VehicleServer::NewStub(mChannel))
{
    for (const auto& msg : config.messages) {
        MessageFilter filter;
        filter.msgId = msg.msgId;
        for (const auto& prop : msg.properties) {
            filter.propIds.insert(prop.id);
            mAllPropIds.insert(prop.id);
        }
        mMessages.push_back(std::move(filter));
    }

    mRunning.store(true);
    mWorker = std::thread(&NodeForwarder::workerLoop, this);

    LOG(INFO) << "[NodeForwarder] started for " << mRemoteAddr
              << " — " << mMessages.size() << " message group(s), "
              << mAllPropIds.size() << " total property IDs";
}

NodeForwarder::~NodeForwarder() {
    {
        std::lock_guard<std::mutex> lock(mQueueMutex);
        mRunning.store(false);
    }
    mQueueCv.notify_one();
    if (mWorker.joinable()) {
        mWorker.join();
    }
}

void NodeForwarder::forward(const std::vector<aidlvhal::VehiclePropValue>& values) {
    // Fast path: skip entirely if no value belongs to this node.
    bool anyRelevant = false;
    for (const auto& v : values) {
        if (mAllPropIds.count(v.prop)) { anyRelevant = true; break; }
    }
    if (!anyRelevant) {
        LOG(INFO) << "[NodeForwarder] " << mRemoteAddr
                  << ": no matching props in this event, skipping";
        return;
    }

    // Partition changed values by message group; enqueue one batch per group
    // that has ≥1 matching value. Lock once for the whole partition.
    std::lock_guard<std::mutex> lock(mQueueMutex);
    for (const auto& msg : mMessages) {
        ForwardBatch batch;
        batch.msgId = msg.msgId;
        for (const auto& v : values) {
            if (msg.propIds.count(v.prop)) {
                batch.values.push_back(v);
            }
        }
        if (!batch.values.empty()) {
            LOG(INFO) << "[NodeForwarder] queuing [" << batch.msgId << "] → "
                      << mRemoteAddr << " (" << batch.values.size() << " prop(s))";
            mQueue.push(std::move(batch));
        }
    }
    mQueueCv.notify_one();
}

void NodeForwarder::workerLoop() {
    LOG(INFO) << "[NodeForwarder] worker thread started for " << mRemoteAddr;

    while (true) {
        std::unique_lock<std::mutex> lock(mQueueMutex);
        mQueueCv.wait(lock, [this] {
            return !mQueue.empty() || !mRunning.load();
        });

        if (!mRunning.load() && mQueue.empty()) {
            break;
        }

        auto batch = std::move(mQueue.front());
        mQueue.pop();
        lock.unlock();

        proto::VehiclePropValueRequests request;
        for (int i = 0; i < static_cast<int>(batch.values.size()); ++i) {
            auto* req = request.add_requests();
            req->set_request_id(i);
            proto_msg_converter::aidlToProto(batch.values[i], req->mutable_value());
        }

        grpc::ClientContext ctx;
        proto::SetValueResults results;
        LOG(INFO) << "[NodeForwarder] sending [" << batch.msgId << "] → "
                  << mRemoteAddr << " (" << batch.values.size() << " prop(s))";
        auto status = mStub->SetValues(&ctx, request, &results);
        if (!status.ok()) {
            LOG(WARNING) << "[NodeForwarder] SetValues [" << batch.msgId
                         << "] to " << mRemoteAddr
                         << " failed: " << status.error_message();
        } else {
            LOG(INFO) << "[NodeForwarder] SetValues [" << batch.msgId
                      << "] to " << mRemoteAddr << " OK";
        }
    }

    LOG(INFO) << "[NodeForwarder] worker thread exiting for " << mRemoteAddr;
}
