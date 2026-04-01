// Stub for Linux port — wakeup_client.grpc.pb.h
// The real header is generated from wakeup_client.proto and power_controller.proto
// (outside this tree). FakeVehicleHardware uses the PowerController gRPC service
// stub to query vehicle power state from an external service.
#pragma once
#include <grpc++/grpc++.h>
#include <memory>
#include <string>

namespace android {
namespace hardware {
namespace automotive {
namespace remoteaccess {

// Request/response messages for PowerController gRPC service
struct IsVehicleInUseRequest {};
struct IsVehicleInUseResponse {
    bool isvehicleinuse() const { return false; }
};

struct GetApPowerBootupReasonRequest {};
struct GetApPowerBootupReasonResponse {
    int32_t bootup_reason() const { return 0; }
    int32_t bootupreason() const { return 0; }
};

// PowerController gRPC service stub
class PowerController final {
public:
    class Stub {
    public:
        grpc::Status IsVehicleInUse(grpc::ClientContext*, const IsVehicleInUseRequest&,
                                    IsVehicleInUseResponse*) {
            return grpc::Status(grpc::StatusCode::UNAVAILABLE, "stub");
        }
        grpc::Status GetApPowerBootupReason(grpc::ClientContext*,
                                            const GetApPowerBootupReasonRequest&,
                                            GetApPowerBootupReasonResponse*) {
            return grpc::Status(grpc::StatusCode::UNAVAILABLE, "stub");
        }
    };

    static std::unique_ptr<Stub> NewStub(const std::shared_ptr<grpc::Channel>&) {
        return std::make_unique<Stub>();
    }
};

} // namespace remoteaccess
} // namespace automotive
} // namespace hardware
} // namespace android
