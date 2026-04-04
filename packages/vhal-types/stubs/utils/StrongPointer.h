// Stub for Linux port — utils/StrongPointer.h
// android::sp<T> as std::shared_ptr<T> with make() factory
#pragma once
#include <memory>

namespace android {

template<typename T>
class sp : public std::shared_ptr<T> {
public:
    using std::shared_ptr<T>::shared_ptr;

    // sp<T>::make(args...) — Android factory, equivalent to std::make_shared<T>(args...)
    template<typename... Args>
    static sp<T> make(Args&&... args) {
        return sp<T>(std::make_shared<T>(std::forward<Args>(args)...));
    }

    // Allow construction from std::shared_ptr
    sp(std::shared_ptr<T> p) : std::shared_ptr<T>(std::move(p)) {}

    // nullptr construction
    sp(std::nullptr_t) : std::shared_ptr<T>(nullptr) {}
    sp() : std::shared_ptr<T>() {}
};

template<typename T>
using wp = std::weak_ptr<T>;

} // namespace android
