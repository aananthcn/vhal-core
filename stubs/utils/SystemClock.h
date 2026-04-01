// Stub for Linux port — Android SystemClock replaced with POSIX clock
#pragma once
#include <cstdint>
#include <time.h>

namespace android {
    inline int64_t uptimeMillis() {
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
    }
    
    inline int64_t elapsedRealtimeNano()
    {
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        return (int64_t)ts.tv_sec * 1000000000 + ts.tv_nsec;
    }
} // namespace android

inline int64_t uptimeNanos() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000000000LL + ts.tv_nsec;
}
