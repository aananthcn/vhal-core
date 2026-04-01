// Stub for Linux port — android/binder_enums.h
#pragma once
#include <type_traits>
#include <cstdint>
#include <array>
#include <iterator>

namespace ndk {

// ── enum_range<T> — iterable range over all enum values ───────────────────
// Returns an empty range by default.
// Code using this for getLastIndex() will get 0 — acceptable for Linux port.
template<typename T>
class EnumRange {
public:
    struct Iterator {
        using value_type        = T;
        using difference_type   = std::ptrdiff_t;
        using pointer           = const T*;
        using reference         = const T&;
        using iterator_category = std::forward_iterator_tag;

        const T* ptr{nullptr};
        Iterator(const T* p = nullptr) : ptr(p) {}
        const T& operator*()  const { return *ptr; }
        Iterator& operator++() { ++ptr; return *this; }
        Iterator  operator++(int) { auto tmp = *this; ++ptr; return tmp; }
        bool operator==(const Iterator& o) const { return ptr == o.ptr; }
        bool operator!=(const Iterator& o) const { return ptr != o.ptr; }
    };

    EnumRange(const T* data, std::size_t size)
        : data_(data), size_(size) {}

    Iterator begin() const { return Iterator(data_); }
    Iterator end()   const { return Iterator(data_ + size_); }
    std::size_t size() const { return size_; }

private:
    const T*    data_{nullptr};
    std::size_t size_{0};
};

// Default: empty range
template<typename T>
inline EnumRange<T> enum_range() {
    return EnumRange<T>(nullptr, 0);
}

// ── internal::enum_values<T> — compile-time array of all enum values ──────
namespace internal {

template<typename T>
constexpr std::array<T, 0> enum_values{};

} // namespace internal
} // namespace ndk

