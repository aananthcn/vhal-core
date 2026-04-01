// Stub for Linux port — android-base/logging.h
#pragma once
#include <cstdio>
#include <sstream>

#ifndef LOG_TAG
#define LOG_TAG "vhal"
#endif

namespace android {
namespace base {
enum LogSeverity { VERBOSE, DEBUG, INFO, WARNING, ERROR, FATAL };
} // namespace base
} // namespace android

#define LOG(severity) \
    ::android::base::severity >= ::android::base::INFO && \
    (fprintf(stderr, #severity "/" LOG_TAG ": "), true) && \
    (std::ostringstream())

#define PLOG(severity) LOG(severity)
#define VLOG(tag)      LOG(VERBOSE)
#define CHECK(x)       ((x) ? (void)0 : (fprintf(stderr, "CHECK failed: " #x "\n"), abort()))
#define CHECK_EQ(a,b)  CHECK((a)==(b))
#define CHECK_NE(a,b)  CHECK((a)!=(b))
#define CHECK_LT(a,b)  CHECK((a)<(b))
#define CHECK_LE(a,b)  CHECK((a)<=(b))
#define CHECK_GT(a,b)  CHECK((a)>(b))
#define CHECK_GE(a,b)  CHECK((a)>=(b))
