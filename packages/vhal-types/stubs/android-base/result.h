// Stub for Linux port — android-base/result.h
// Provides android::base::Result<T,E>, Error<E>, OK()
#pragma once
#include <android/binder_auto_utils.h>
#include <memory>
#include <string>
#include <utility>

namespace android {
namespace base {

// ── ErrorMessage — returned by Result<T,void>::error() ───────────────────
// Allows result.error().message() call pattern
struct ErrorMessage {
    std::string msg;
    const std::string& message() const { return msg; }
    const char* c_str() const { return msg.c_str(); }
    operator std::string() const { return msg; }
    int code() const { return 0; }
};

// ── Error<E> ──────────────────────────────────────────────────────────────
template<typename E = void>
class Error {
public:
    Error() = default;
    explicit Error(const E& e) : error_(e) {}
    explicit Error(E&& e) : error_(std::move(e)) {}
    template<typename T> Error& operator<<(const T&) { return *this; }
    const E& error() const { return error_; }
private:
    E error_{};
};

template<>
class Error<void> {
public:
    Error() = default;
    Error& operator<<(const std::string& m) { msg_ += m; return *this; }
    Error& operator<<(const char* m)        { if(m) msg_ += m; return *this; }
    template<typename T> Error& operator<<(const T&) { return *this; }
    const std::string& message() const { return msg_; }
private:
    std::string msg_;
};

// ── Result<T, E> — primary (T!=void, E!=void) ─────────────────────────────
template<typename T, typename E = void>
class Result {
public:
    Result() : ok_(false) {}
    Result(const T& v) : val_(v), ok_(true) {}
    Result(T&& v)      : val_(std::move(v)), ok_(true) {}
    Result(const Error<E>& e) : err_(e.error()), ok_(false) {}
    // Direct construction from E (e.g. return someResult.error())
    Result(const E& e) : err_(e), ok_(false) {}
    Result(E&& e)      : err_(std::move(e)), ok_(false) {}
    // nullptr construction for Result<unique_ptr<T>,E>
    Result(std::nullptr_t) : ok_(true) {}

    bool ok()        const { return ok_; }
    bool has_value() const { return ok_; }
    explicit operator bool() const { return ok_; }
    T&       value()       { return val_; }
    const T& value() const { return val_; }
    T&       operator*()       { return val_; }
    const T& operator*() const { return val_; }
    T*       operator->()       { return &val_; }
    const T* operator->() const { return &val_; }
    const E& error() const { return err_; }
    std::string error_message() const { return "error"; }
private:
    T val_{}; E err_{}; bool ok_{false};
};

// Result<T, void>
template<typename T>
class Result<T, void> {
public:
    Result() : ok_(false) {}
    Result(std::nullptr_t) : ok_(true) {}
    Result(const T& v) : val_(v), ok_(true) {}
    Result(T&& v)      : val_(std::move(v)), ok_(true) {}
    Result(const Error<void>& e) : msg_(e.message()), ok_(false) {}
    // Allow propagating error from another Result: return otherResult.error()
    Result(const ErrorMessage& e) : msg_(e.message()), ok_(false) {}

    bool ok()        const { return ok_; }
    bool has_value() const { return ok_; }
    explicit operator bool() const { return ok_; }
    T&       value()       { return val_; }
    const T& value() const { return val_; }
    T&       operator*()       { return val_; }
    const T& operator*() const { return val_; }
    T*       operator->()       { return &val_; }
    const T* operator->() const { return &val_; }
    const std::string& error_message() const { return msg_; }
    ErrorMessage error() const { return ErrorMessage{msg_}; }
private:
    T val_{}; std::string msg_; bool ok_{false};
};

// Result<void, E>
template<typename E>
class Result<void, E> {
public:
    Result() : ok_(true) {}
    Result(const Error<E>& e) : err_(e.error()), ok_(false) {}
    // Direct construction from E (for propagating errors)
    Result(const E& e) : err_(e), ok_(false) {}
    Result(E&& e)      : err_(std::move(e)), ok_(false) {}
    bool ok() const { return ok_; }
    explicit operator bool() const { return ok_; }
    const E& error() const { return err_; }
    std::string error_message() const { return "error"; }
private:
    E err_{}; bool ok_{false};
};

// Result<void, void>
template<>
class Result<void, void> {
public:
    Result() : ok_(true) {}
    Result(const Error<void>& e) : msg_(e.message()), ok_(false) {}
    Result(const ErrorMessage& e) : msg_(e.message()), ok_(false) {}
    bool ok()        const { return ok_; }
    bool has_value() const { return ok_; }
    explicit operator bool() const { return ok_; }
    const std::string& error_message() const { return msg_; }
    ErrorMessage error() const { return ErrorMessage{msg_}; }
    // value() on void result is a no-op — some code calls .value() unconditionally
    void value() const {}
private:
    std::string msg_; bool ok_{false};
};

inline Result<void> OK() { return Result<void>(); }

} // namespace base
} // namespace android

namespace android {
namespace base {

template<typename... Args>
inline Error<void> Errorf(const char* fmt, const Args&...) {
    Error<void> e;
    e << std::string(fmt);
    return e;
}

} // namespace base
} // namespace android

using android::base::Errorf;

// Also visible inside android:: sub-namespaces
namespace android {
using base::Errorf;
}
