// Stub for Linux port — android-base/format.h
#pragma once
#include <string>
#include <cstdio>
#include <cstdarg>
#include <cmath>
#include <sstream>
#include <iterator>

// namespace android {
// namespace base {
// inline std::string StringPrintf(const char* fmt, ...) {
//     char buf[4096];
//     va_list args;
//     va_start(args, fmt);
//     vsnprintf(buf, sizeof(buf), fmt, args);
//     va_end(args);
//     return std::string(buf);
// }
// } // namespace base
// } // namespace android

namespace fmt {

// Minimal parse context stub
struct format_parse_context {
    const char* p_{nullptr};
    constexpr const char* begin() const { return p_; }
    constexpr const char* end()   const { return p_; }
};

// Minimal format context stub
struct format_context {
    std::string buf;
    std::back_insert_iterator<std::string> out() {
        return std::back_inserter(buf);
    }
};

// Base formatter — uses auto returns to avoid type member issues
template<typename T, typename Char = char>
struct formatter {
    constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }
    auto format(const T&, format_context& ctx) const { return ctx.out(); }
};

// format_to and format — simplified stubs, no arg streaming
// Args are intentionally ignored to avoid operator<< dependency issues
template<typename OutputIt, typename... Args>
OutputIt format_to(OutputIt out, const std::string& fmt_str, const Args&...) {
    for (char c : fmt_str) *out++ = c;
    return out;
}

template<typename... Args>
inline std::string format(const std::string& fmt_str, const Args&...) {
    return fmt_str;  // simplified: return format string as-is
}

inline std::string format(const std::string& s) { return s; }

} // namespace fmt

