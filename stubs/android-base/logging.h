// Stub for Linux port — android-base/logging.h
#pragma once
#include <cstdio>
#include <sstream>
#include <string>

#ifndef LOG_TAG
#define LOG_TAG "vhal"
#endif

namespace android {
namespace base {
enum LogSeverity { VERBOSE, DEBUG, INFO, WARNING, ERROR, FATAL };

// LogMessage — supports LOG(LEVEL) << "msg" pattern
struct LogMessage {
    bool active;
    std::ostringstream oss;
    const char* severity;
    LogMessage(bool a, const char* sev) : active(a), severity(sev) {}
    ~LogMessage() {
        if (active)
            fprintf(stderr, "%s/" LOG_TAG ": %s\n", severity, oss.str().c_str());
    }
    template<typename T>
    LogMessage& operator<<(const T& v) { if (active) oss << v; return *this; }
};

// CheckMessage — supports CHECK(cond) << "msg" pattern
struct CheckMessage {
    bool failed;
    std::ostringstream oss;
    CheckMessage(bool f) : failed(f) {}
    ~CheckMessage() {
        if (failed) {
            fprintf(stderr, "CHECK failed: %s\n", oss.str().c_str());
            abort();
        }
    }
    template<typename T>
    CheckMessage& operator<<(const T& v) { if (failed) oss << v; return *this; }
};

} // namespace base
} // namespace android

#define LOG(severity) ::android::base::LogMessage(true, #severity)
#define PLOG(severity) LOG(severity)
#define VLOG(tag)      ::android::base::LogMessage(false, "VERBOSE")
#define CHECK(x)       ::android::base::CheckMessage(!(x))
#define CHECK_EQ(a,b)  CHECK((a)==(b))
#define CHECK_NE(a,b)  CHECK((a)!=(b))
#define CHECK_LT(a,b)  CHECK((a)<(b))
#define CHECK_LE(a,b)  CHECK((a)<=(b))
#define CHECK_GT(a,b)  CHECK((a)>(b))
#define CHECK_GE(a,b)  CHECK((a)>=(b))
