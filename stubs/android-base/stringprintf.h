// Stub for Linux port — android-base/stringprintf.h
#pragma once
#include <string>
#include <cstdio>
#include <cstdarg>

namespace android {
namespace base {

inline std::string StringPrintf(const char* fmt, ...) {
    char buf[4096];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    return std::string(buf);
}

} // namespace base
} // namespace android
