// Stub for Linux port — Android ParcelFileDescriptor not available
#pragma once
#include <cstdint>

namespace aidl {
namespace android {
namespace os {

struct ParcelFileDescriptor {
    int fd{-1};

    bool operator==(const ParcelFileDescriptor& other) const {
        return fd == other.fd;
    }
    bool operator!=(const ParcelFileDescriptor& other) const {
        return !(*this == other);
    }
};

} // namespace os
} // namespace android
} // namespace aidl
