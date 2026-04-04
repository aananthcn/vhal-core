// Stub for Linux port — android/binder_manager.h
// AServiceManager is not available — replaced by gRPC server startup
#pragma once
#include <android/binder_ibinder.h>
#include <cstdint>

inline binder_status_t AServiceManager_addService(AIBinder*, const char*) {
    return 0;
}
inline AIBinder* AServiceManager_getService(const char*) {
    return nullptr;
}
inline AIBinder* AServiceManager_waitForService(const char*) {
    return nullptr;
}
