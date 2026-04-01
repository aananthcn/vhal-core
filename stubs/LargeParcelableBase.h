// Stub for Linux port — LargeParcelableBase.h
// Android large parcelable support is not needed on the gRPC transport path.
// gRPC handles arbitrarily large messages natively.
#pragma once
#include <cstdint>

namespace android {
namespace hardware {
namespace automotive {
namespace vehicle {

class LargeParcelableBase {
public:
    virtual ~LargeParcelableBase() = default;
};

} // namespace vehicle
} // namespace automotive
} // namespace hardware
} // namespace android
