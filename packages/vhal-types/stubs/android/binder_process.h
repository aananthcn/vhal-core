// Stub for Linux port — android/binder_process.h
// Binder thread pool is not used — replaced by gRPC server
#pragma once
#include <cstdint>

inline void ABinderProcess_setThreadPoolMaxThreadCount(uint32_t) {}
inline bool ABinderProcess_setThreadPoolMaxThreadCount_ret(uint32_t) { return true; }
inline void ABinderProcess_startThreadPool() {}
inline void ABinderProcess_joinThreadPool() {}
inline bool ABinderProcess_setupPolling(int*) { return false; }
inline bool ABinderProcess_handlePolledCommands() { return false; }
