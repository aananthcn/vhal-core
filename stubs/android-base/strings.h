// Stub for Linux port — android-base/strings.h
#pragma once
#include <string>
#include <vector>
#include <sstream>
#include <algorithm>

namespace android {
namespace base {

inline std::vector<std::string> Split(const std::string& s,
                                       const std::string& delim) {
    std::vector<std::string> result;
    size_t start = 0, pos;
    while ((pos = s.find(delim, start)) != std::string::npos) {
        result.push_back(s.substr(start, pos - start));
        start = pos + delim.size();
    }
    result.push_back(s.substr(start));
    return result;
}

inline std::string Join(const std::vector<std::string>& v,
                         const std::string& sep) {
    std::string result;
    for (size_t i = 0; i < v.size(); ++i) {
        if (i) result += sep;
        result += v[i];
    }
    return result;
}

inline std::string Join(const std::vector<std::string>& v, char sep) {
    return Join(v, std::string(1, sep));
}

inline std::string Trim(const std::string& s) {
    size_t l = s.find_first_not_of(" \t\n\r");
    size_t r = s.find_last_not_of(" \t\n\r");
    return (l == std::string::npos) ? "" : s.substr(l, r - l + 1);
}

inline bool StartsWith(const std::string& s, const std::string& prefix) {
    return s.size() >= prefix.size() &&
           s.compare(0, prefix.size(), prefix) == 0;
}

inline bool EqualsIgnoreCase(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); i++) {
        if (tolower((unsigned char)a[i]) != tolower((unsigned char)b[i])) return false;
    }
    return true;
}

inline bool EndsWith(const std::string& s, const std::string& suffix) {
    return s.size() >= suffix.size() &&
           s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

inline std::string StringReplace(const std::string& s,
                                  const std::string& from,
                                  const std::string& to,
                                  bool all) {
    std::string result = s;
    size_t pos = 0;
    while ((pos = result.find(from, pos)) != std::string::npos) {
        result.replace(pos, from.size(), to);
        pos += to.size();
        if (!all) break;
    }
    return result;
}

} // namespace base
} // namespace android
