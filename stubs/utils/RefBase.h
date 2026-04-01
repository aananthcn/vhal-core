// Stub for Linux port — Android RefBase not needed with std::shared_ptr
#pragma once
namespace android {
class RefBase {
public:
    virtual ~RefBase() = default;
};
} // namespace android
