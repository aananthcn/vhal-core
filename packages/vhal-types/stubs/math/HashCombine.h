// Stub for Linux port — math/HashCombine.h
#pragma once
#include <cstddef>
#include <functional>

namespace android {

// Matches Android's hashCombine signature
template<typename T>
inline size_t hashCombine(size_t seed, const T& val) {
    seed ^= std::hash<T>{}(val) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    return seed;
}

} // namespace android
