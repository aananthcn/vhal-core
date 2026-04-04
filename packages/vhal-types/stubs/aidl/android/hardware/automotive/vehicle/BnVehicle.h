// Stub for Linux port — BnVehicle.h
// Binder native vehicle interface — not used on gRPC path
#pragma once
#include <aidl/android/hardware/automotive/vehicle/IVehicle.h>

namespace aidl {
namespace android {
namespace hardware {
namespace automotive {
namespace vehicle {

// Binder-native stub — inherits IVehicle so subclasses can use override
class BnVehicle : public IVehicle {
public:
    virtual binder_status_t dump(int, const char**, uint32_t) { return STATUS_OK; }
    virtual binder_status_t getInterfaceVersion(int32_t* version) {
        if (version) *version = 4;
        return STATUS_OK;
    }
};

} // namespace vehicle
} // namespace automotive
} // namespace hardware
} // namespace android
} // namespace aidl
