#pragma once

#include <cstdint>
#include <memory>
#include <string>

struct Runtime;

// Keeps Runtime forward-declared in this Swift-imported header; deletion is
// defined in HostRuntime.cpp where the full Runtime type is visible.
struct RuntimeDeleter {
    void operator()(Runtime* runtime) const;
};

class HostRuntime {
public:
    explicit HostRuntime(std::string lib_path);
    HostRuntime(const HostRuntime&) = delete;
    HostRuntime& operator=(const HostRuntime&) = delete;
    HostRuntime(HostRuntime&&) noexcept = default;
    HostRuntime& operator=(HostRuntime&&) noexcept = default;

    bool valid() const;
    void attachSurface(void* metal_layer, uint32_t width, uint32_t height);
    void resize(uint32_t width, uint32_t height);
    void tick(float dt);
    bool reload();

private:
    std::unique_ptr<Runtime, RuntimeDeleter> runtime_;
};
