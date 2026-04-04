// Stub for Linux port — android-base/properties.h
// Android system properties are not available on Linux.
// Return defaults for all queries.
#pragma once
#include <string>

namespace android {
namespace base {

inline std::string GetProperty(const std::string& key,
                                const std::string& default_value) {
    return default_value;
}

inline bool GetBoolProperty(const std::string& key, bool default_value) {
    return default_value;
}

inline int32_t GetIntProperty(const std::string& key,
                               int32_t default_value,
                               int32_t min = std::numeric_limits<int32_t>::min(),
                               int32_t max = std::numeric_limits<int32_t>::max()) {
    return default_value;
}

inline int64_t GetInt64Property(const std::string& key,
                                 int64_t default_value,
                                 int64_t min = std::numeric_limits<int64_t>::min(),
                                 int64_t max = std::numeric_limits<int64_t>::max()) {
    return default_value;
}

inline bool SetProperty(const std::string& key, const std::string& value) {
    return true; // no-op
}

} // namespace base
} // namespace android
