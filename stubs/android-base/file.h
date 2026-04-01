// Stub for Linux port — android-base/file.h
#pragma once
#include <string>
#include <fstream>
#include <sstream>

namespace android {
namespace base {

inline bool ReadFileToString(const std::string& path, std::string* content,
                              bool follow_symlinks = false) {
    std::ifstream f(path);
    if (!f.is_open()) return false;
    std::ostringstream ss;
    ss << f.rdbuf();
    *content = ss.str();
    return true;
}

inline bool WriteStringToFile(const std::string& content, const std::string& path,
                               bool follow_symlinks = false) {
    std::ofstream f(path);
    if (!f.is_open()) return false;
    f << content;
    return f.good();
}

} // namespace base
} // namespace android
