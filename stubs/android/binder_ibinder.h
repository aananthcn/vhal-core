// Stub for Linux port — android/binder_ibinder.h
#pragma once
#include <cstdint>
#include <memory>

typedef struct AIBinder AIBinder;
typedef int32_t binder_status_t;

inline void AIBinder_incStrong(AIBinder*) {}
inline void AIBinder_decStrong(AIBinder*) {}
