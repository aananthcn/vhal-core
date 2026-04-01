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

// format_to — write formatted output to an output iterator
template<typename OutputIt, typename... Args>
OutputIt format_to(OutputIt out, const std::string& fmt_str, const Args&... args) {
    std::string parts[sizeof...(args) + 1];
    int idx = 0;
    (void)std::initializer_list<int>{
        (parts[idx++] = [](auto v) -> std::string {
            std::ostringstream s; s << v; return s.str();
        }(args), 0)...
    };
    const char* p = fmt_str.c_str();
    int arg_idx = 0;
    while (*p) {
        if (*p == '{' && *(p+1) == '}') {
            if (arg_idx < (int)sizeof...(args))
                for (char c : parts[arg_idx++]) *out++ = c;
            p += 2;
        } else {
            *out++ = *p++;
        }
    }
    return out;
}

// format — returns std::string
template<typename... Args>
inline std::string format(const std::string& fmt_str, const Args&... args) {
    std::string result;
    format_to(std::back_inserter(result), fmt_str, args...);
    return result;
}

inline std::string format(const std::string& s) { return s; }

} // namespace fmt
