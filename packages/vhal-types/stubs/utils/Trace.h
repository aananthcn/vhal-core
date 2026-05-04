// Stub for Linux port — utils/Trace.h (Android systrace)
#pragma once

// All trace macros are no-ops on Linux
#define ATRACE_TAG_ALWAYS   (1 << 31)
#ifndef ATRACE_TAG
#define ATRACE_TAG          ATRACE_TAG_ALWAYS
#endif
#define ATRACE_CALL()
#define ATRACE_NAME(name)
#define ATRACE_INT(name, val)
#define ATRACE_BEGIN(name)
#define ATRACE_END()
#define ATRACE_ENABLED()    false
