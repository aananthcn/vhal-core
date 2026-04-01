// Stub for Linux port — android-base/parseint.h
#pragma once
#include <string>
#include <cstdlib>
#include <cerrno>
#include <limits>
#include <cstdint>

namespace android {
namespace base {

template<typename T>
inline bool ParseInt(const std::string& s, T* out,
                     T min = std::numeric_limits<T>::min(),
                     T max = std::numeric_limits<T>::max()) {
    errno = 0;
    char* end;
    long long val = strtoll(s.c_str(), &end, 10);
    if (errno != 0 || end == s.c_str() || *end != '\0') return false;
    if (val < static_cast<long long>(min) ||
        val > static_cast<long long>(max)) return false;
    *out = static_cast<T>(val);
    return true;
}

template<typename T>
inline bool ParseUint(const std::string& s, T* out,
                      T max = std::numeric_limits<T>::max()) {
    errno = 0;
    char* end;
    unsigned long long val = strtoull(s.c_str(), &end, 10);
    if (errno != 0 || end == s.c_str() || *end != '\0') return false;
    if (val > static_cast<unsigned long long>(max)) return false;
    *out = static_cast<T>(val);
    return true;
}

} // namespace base
} // namespace android
