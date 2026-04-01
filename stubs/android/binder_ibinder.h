// Stub for Linux port — android/binder_ibinder.h
#pragma once
#include <cstdint>
#include <memory>
#include <sys/types.h>

typedef struct AIBinder AIBinder;
typedef int32_t binder_status_t;
static constexpr binder_status_t STATUS_OK               = 0;
static constexpr binder_status_t STATUS_PERMISSION_DENIED = -1;

inline void AIBinder_incStrong(AIBinder*) {}
inline void AIBinder_decStrong(AIBinder*) {}
inline bool AIBinder_isAlive(const AIBinder*) { return true; }
inline uid_t AIBinder_getCallingUid() { return 0; }  // Always root on Linux port

// AIBinder_DeathRecipient — Binder death notification, stub for Linux port
typedef struct AIBinder_DeathRecipient AIBinder_DeathRecipient;

inline AIBinder_DeathRecipient* AIBinder_DeathRecipient_new(void (*)(void*)) {
    return nullptr;
}
inline void AIBinder_DeathRecipient_delete(AIBinder_DeathRecipient*) {}
inline void AIBinder_DeathRecipient_setOnUnlinked(AIBinder_DeathRecipient*, void (*)(void*)) {}
inline binder_status_t AIBinder_linkToDeath(AIBinder*, AIBinder_DeathRecipient*, void*) {
    return STATUS_OK;
}
inline binder_status_t AIBinder_unlinkToDeath(AIBinder*, AIBinder_DeathRecipient*, void*) {
    return STATUS_OK;
}

namespace ndk {
class ScopedAIBinder_DeathRecipient {
public:
    ScopedAIBinder_DeathRecipient() : ptr_(nullptr) {}
    explicit ScopedAIBinder_DeathRecipient(AIBinder_DeathRecipient* p) : ptr_(p) {}
    ~ScopedAIBinder_DeathRecipient() { AIBinder_DeathRecipient_delete(ptr_); }
    AIBinder_DeathRecipient* get() const { return ptr_; }
private:
    AIBinder_DeathRecipient* ptr_;
};
} // namespace ndk
