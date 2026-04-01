// Stub for Linux port — android-base/expected.h
#pragma once
#include <string>
#include <utility>

namespace android {
namespace base {

template<typename T, typename E>
class Expected {
public:
    Expected(const T& val) : val_(val), ok_(true) {}
    Expected(T&& val) : val_(std::move(val)), ok_(true) {}

    struct unexpected_type {
        E error;
        explicit unexpected_type(const E& e) : error(e) {}
    };
    Expected(const unexpected_type& u) : err_(u.error), ok_(false) {}

    bool ok() const { return ok_; }
    explicit operator bool() const { return ok_; }
    T& value() { return val_; }
    const T& value() const { return val_; }
    T& operator*() { return val_; }
    const T& operator*() const { return val_; }
    T* operator->() { return &val_; }
    const T* operator->() const { return &val_; }
    const E& error() const { return err_; }

private:
    T val_{};
    E err_{};
    bool ok_{false};
};

template<typename E>
struct unexpected {
    E error;
    explicit unexpected(const E& e) : error(e) {}
};

} // namespace base
} // namespace android
