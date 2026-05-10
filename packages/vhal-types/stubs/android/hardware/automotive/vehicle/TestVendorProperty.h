// Stub for Linux port — android/hardware/automotive/vehicle/TestVendorProperty.h
// Test vendor properties used by FakeVehicleHardware. Values are in the vendor property range.
#pragma once
#include <cstdint>

namespace android {
namespace hardware {
namespace automotive {
namespace vehicle {

// Vendor property ID range: 0x2103xxxx (VehiclePropertyGroup::VENDOR | VehicleArea::GLOBAL | INT32)
enum class TestVendorProperty : int32_t {
    ECHO_REVERSE_BYTES                    = 0x21030001,
    VENDOR_PROPERTY_FOR_ERROR_CODE_TESTING = 0x21030002,
    VENDOR_CLUSTER_SWITCH_UI              = 0x21030010,
    VENDOR_CLUSTER_DISPLAY_STATE          = 0x21030011,

    // Velan voice assistant trigger. int32_values[0]: 0 = OFF, 1 = ON.
    VOICE_ASSIST_TRIGGER                  = 0x21400001,
};

} // namespace vehicle
} // namespace automotive
} // namespace hardware
} // namespace android

using android::hardware::automotive::vehicle::TestVendorProperty;
