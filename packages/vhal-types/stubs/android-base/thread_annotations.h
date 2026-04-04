// Stub for Linux port — Clang thread safety annotations are no-ops on Linux
#pragma once
#define GUARDED_BY(x)
#define PT_GUARDED_BY(x)
#define ACQUIRED_BEFORE(...)
#define ACQUIRED_AFTER(...)
#define REQUIRES(...)
#define REQUIRES_SHARED(...)
#define ACQUIRE(...)
#define ACQUIRE_SHARED(...)
#define RELEASE(...)
#define RELEASE_SHARED(...)
#define TRY_ACQUIRE(...)
#define TRY_ACQUIRE_SHARED(...)
#define EXCLUDES(...)
#define ASSERT_CAPABILITY(x)
#define ASSERT_SHARED_CAPABILITY(x)
#define RETURN_CAPABILITY(x)
#define NO_THREAD_SAFETY_ANALYSIS
#define CAPABILITY(x)
#define SCOPED_CAPABILITY
#define SHARED_CAPABILITY(x)


#include <atomic>
#include <functional>
#include <memory>
#include <condition_variable>


// ScopedLockAssertion — tells Clang thread-safety analysis that a lock
// is held in the current scope. No-op on Linux without Clang TSA.
namespace android {
namespace base {
struct ScopedLockAssertion {
    template<typename T>
    explicit ScopedLockAssertion(const T&) {}
};
} // namespace base
} // namespace android

// Pull in binder forward declarations needed by SubscriptionManager
#include <android/binder_ibinder.h>
