#pragma once
#include <cstdint>
#include <unistd.h>
#include <android/binder_auto_utils.h>
namespace aidl { namespace android { namespace os {
struct ParcelFileDescriptor {
    int fd{-1};
    ParcelFileDescriptor() = default;
    explicit ParcelFileDescriptor(int f) : fd(f) {}
    ParcelFileDescriptor& operator=(const ndk::ScopedFileDescriptor& sfd) {
        fd = sfd.get(); return *this;
    }
    void set(int f) { fd = f; }
    int get() const { return fd; }
    bool operator==(const ParcelFileDescriptor& o) const { return fd == o.fd; }
    bool operator!=(const ParcelFileDescriptor& o) const { return !(*this == o); }
    bool operator<(const ParcelFileDescriptor& o) const { return fd < o.fd; }
};
}}}
