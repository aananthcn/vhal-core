// Stub for Linux port — LargeParcelableBase.h
// Android large parcelable support is not needed on the gRPC transport path.
#pragma once
#include <android/binder_auto_utils.h>
#include <android-base/result.h>
#include <android-base/expected.h>
#include <memory>

namespace android {
namespace automotive {
namespace car_binder_lib {

class LargeParcelableBase {
public:
    virtual ~LargeParcelableBase() = default;

    // BorrowedOwnedObject<T> — wraps a T that may be owned or borrowed
    // On gRPC path we always own it, so this is just a unique_ptr wrapper
    template<typename T>
    class BorrowedOwnedObject {
    public:
        BorrowedOwnedObject() = default;
        explicit BorrowedOwnedObject(std::unique_ptr<T> obj) : owned_(std::move(obj)) {}
        explicit BorrowedOwnedObject(const T* borrowed) : borrowed_(borrowed) {}

        const T* get() const {
            return owned_ ? owned_.get() : borrowed_;
        }
        // getObject() — alias for get(), matches Android API surface
        const T* getObject() const { return get(); }
        const T& operator*() const { return *get(); }
        const T* operator->() const { return get(); }
        bool has_value() const { return get() != nullptr; }

    private:
        std::unique_ptr<T> owned_;
        const T* borrowed_{nullptr};
    };

    // parcelableToStableLargeParcelable — no-op on gRPC path (no shared memory needed)
    template<typename T>
    static android::base::Result<std::unique_ptr<ndk::ScopedFileDescriptor>>
    parcelableToStableLargeParcelable(T& /*parcelable*/) {
        return nullptr;  // no large parcelable on gRPC path
    }

    // stableLargeParcelableToParcelable — returns borrowed reference on gRPC path
    template<typename T>
    static android::base::expected<BorrowedOwnedObject<T>, ndk::ScopedAStatus>
    stableLargeParcelableToParcelable(const T& parcelable) {
        return BorrowedOwnedObject<T>(&parcelable);
    }
};

} // namespace car_binder_lib
} // namespace automotive
} // namespace android


