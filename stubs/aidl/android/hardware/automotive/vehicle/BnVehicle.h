// Stub for Linux port — BnVehicle.h
// Binder native vehicle interface — not used on gRPC path
#pragma once
#include <aidl/android/hardware/automotive/vehicle/IVehicle.h>

namespace aidl {
namespace android {
namespace hardware {
namespace automotive {
namespace vehicle {

// Empty stub — gRPC replaces Binder transport
class BnVehicle {};

} // namespace vehicle
} // namespace automotive
} // namespace hardware
} // namespace android
} // namespace aidl
