// Stub for Linux port — android/binder_auto_utils.h
// Provides ndk::ScopedAStatus replacing Android Binder status
#pragma once
#include <string>
#include <cstdint>

#include <android/binder_enums.h>

// Android binder status codes
typedef int32_t binder_status_t;
static constexpr binder_status_t STATUS_OK = 0;
static constexpr binder_status_t STATUS_UNKNOWN_ERROR = (-2147483647 - 1);

namespace ndk {

class ScopedAStatus {
public:
    ScopedAStatus() : status_(STATUS_OK) {}
    explicit ScopedAStatus(binder_status_t status) : status_(status) {}

    static ScopedAStatus ok() { return ScopedAStatus(STATUS_OK); }

    static ScopedAStatus fromExceptionCode(int32_t code) {
        return ScopedAStatus(static_cast<binder_status_t>(code));
    }

    static ScopedAStatus fromExceptionCodeWithMessage(int32_t code,
                                                       const char* msg) {
        ScopedAStatus s(static_cast<binder_status_t>(code));
        s.message_ = msg ? msg : "";
        return s;
    }

    static ScopedAStatus fromServiceSpecificError(int32_t code) {
        return ScopedAStatus(static_cast<binder_status_t>(code));
    }

    static ScopedAStatus fromServiceSpecificErrorWithMessage(int32_t code,
                                                              const char* msg) {
        ScopedAStatus s(static_cast<binder_status_t>(code));
        s.message_ = msg ? msg : "";
        return s;
    }

    bool isOk() const { return status_ == STATUS_OK; }
    binder_status_t getStatus() const { return status_; }
    int32_t getExceptionCode() const { return static_cast<int32_t>(status_); }
    int32_t getServiceSpecificError() const { return static_cast<int32_t>(status_); }
    const char* getMessage() const { return message_.c_str(); }
    std::string getDescription() const { return message_; }

private:
    binder_status_t status_{STATUS_OK};
    std::string message_;
};

} // namespace ndk

// Exception codes used by Android binder
namespace android {
namespace binder {
namespace Status {
static constexpr int32_t EX_NONE              =  0;
static constexpr int32_t EX_SECURITY          = -1;
static constexpr int32_t EX_BAD_PARCELABLE    = -2;
static constexpr int32_t EX_ILLEGAL_ARGUMENT  = -3;
static constexpr int32_t EX_NULL_POINTER      = -4;
static constexpr int32_t EX_ILLEGAL_STATE     = -5;
static constexpr int32_t EX_NETWORK_MAIN_THREAD = -6;
static constexpr int32_t EX_UNSUPPORTED_OPERATION = -7;
static constexpr int32_t EX_SERVICE_SPECIFIC  = -8;
static constexpr int32_t EX_PARCELABLE        = -9;
static constexpr int32_t EX_TRANSACTION_FAILED = -129;
} // namespace Status
} // namespace binder
} // namespace android

