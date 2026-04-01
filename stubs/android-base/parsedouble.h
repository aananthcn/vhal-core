// Stub for Linux port — android-base/parsedouble.h
#pragma once
#include <string>
#include <cstdlib>
#include <cerrno>
#include <limits>

namespace android {
namespace base {

inline bool ParseDouble(const std::string& s, double* out,
                        double min = std::numeric_limits<double>::lowest(),
                        double max = std::numeric_limits<double>::max()) {
    errno = 0;
    char* end;
    double val = strtod(s.c_str(), &end);
    if (errno != 0 || end == s.c_str() || *end != '\0') return false;
    if (val < min || val > max) return false;
    *out = val;
    return true;
}

inline bool ParseFloat(const std::string& s, float* out,
                       float min = std::numeric_limits<float>::lowest(),
                       float max = std::numeric_limits<float>::max()) {
    double d;
    if (!ParseDouble(s, &d, static_cast<double>(min), static_cast<double>(max))) return false;
    *out = static_cast<float>(d);
    return true;
}

} // namespace base
} // namespace android
