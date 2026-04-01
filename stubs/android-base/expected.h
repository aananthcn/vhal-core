// Stub for Linux port — android-base/expected.h
// android::base::expected<T,E> modelled after std::expected (C++23)
#pragma once
#include <string>
#include <utility>

namespace android {
namespace base {

template<typename E>
struct unexpected {
    E error;
    explicit unexpected(const E& e) : error(e) {}
    explicit unexpected(E&& e) : error(std::move(e)) {}
};

template<typename T, typename E>
class expected {
public:
    // Success
    expected(const T& val) : val_(val), ok_(true) {}
    expected(T&& val)      : val_(std::move(val)), ok_(true) {}

    // Failure
    expected(const unexpected<E>& u) : err_(u.error), ok_(false) {}
    expected(unexpected<E>&& u)      : err_(std::move(u.error)), ok_(false) {}

    bool has_value() const { return ok_; }
    bool ok()        const { return ok_; }
    explicit operator bool() const { return ok_; }

    T&       value()       { return val_; }
    const T& value() const { return val_; }
    T&       operator*()       { return val_; }
    const T& operator*() const { return val_; }
    T*       operator->()       { return &val_; }
    const T* operator->() const { return &val_; }

    E&       error()       { return err_; }
    const E& error() const { return err_; }

private:
    T    val_{};
    E    err_{};
    bool ok_{false};
};

// Specialisation: expected<void, E>
template<typename E>
class expected<void, E> {
public:
    expected() : ok_(true) {}
    expected(const unexpected<E>& u) : err_(u.error), ok_(false) {}
    expected(unexpected<E>&& u)      : err_(std::move(u.error)), ok_(false) {}

    bool has_value() const { return ok_; }
    bool ok()        const { return ok_; }
    explicit operator bool() const { return ok_; }
    E&       error()       { return err_; }
    const E& error() const { return err_; }

private:
    E    err_{};
    bool ok_{false};
};

} // namespace base
} // namespace android
