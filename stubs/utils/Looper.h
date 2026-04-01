// Stub for Linux port — utils/Looper.h
#pragma once
#include <utils/StrongPointer.h>
#include <utils/RefBase.h>
#include <cstdint>
#include <functional>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>

namespace android {

typedef int64_t nsecs_t;

struct Message {
    int what{0};
    explicit Message(int what = 0) : what(what) {}
};

class MessageHandler : public RefBase {
public:
    virtual void handleMessage(const Message& message) = 0;
};

class Looper : public RefBase {
public:
    static constexpr int POLL_WAKE    = -1;
    static constexpr int POLL_TIMEOUT = -3;
    static constexpr int EVENT_INPUT  =  1;

    explicit Looper(bool /*allowNonCallbacks*/ = false) {}

    // sp<Looper>::make factory support
    static sp<Looper> create(bool allowNonCallbacks = false) {
        return sp<Looper>(new Looper(allowNonCallbacks));
    }

    static void setForThread(const sp<Looper>&) {}
    static sp<Looper> getForThread() { return nullptr; }
    static sp<Looper> prepare(int /*opts*/) { return nullptr; }

    // Poll — blocks until wake() is called or timeout expires
    int pollOnce(int timeoutMillis) {
        std::unique_lock<std::mutex> lock(mutex_);
        if (timeoutMillis < 0) {
            cv_.wait(lock, [this]{ return woken_.load(); });
        } else {
            cv_.wait_for(lock,
                std::chrono::milliseconds(timeoutMillis),
                [this]{ return woken_.load(); });
        }
        woken_ = false;
        return POLL_WAKE;
    }

    void wake() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            woken_ = true;
        }
        cv_.notify_all();
    }

    void sendMessage(const sp<MessageHandler>& handler, const Message& msg) {
        if (handler) handler->handleMessage(msg);
    }

    void sendMessageAtTime(nsecs_t /*uptime*/, const sp<MessageHandler>& handler,
                        const Message& msg) {
        // Stub — deliver immediately, timing not enforced on Linux port
        if (handler) handler->handleMessage(msg);
        wake();
    }

    void sendMessageDelayed(nsecs_t /*delay*/, const sp<MessageHandler>& handler,
                            const Message& msg) {
        if (handler) handler->handleMessage(msg);
    }

    // removeMessages — no-op in stub (handler tracks its own state)
    void removeMessages(const sp<MessageHandler>&) {}
    void removeMessages(const sp<MessageHandler>&, int /*what*/) {}

private:
    std::mutex mutex_;
    std::condition_variable cv_;
    std::atomic<bool> woken_{false};
};

} // namespace android
