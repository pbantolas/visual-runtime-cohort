#pragma once

#include <cstdint>
#include <memory>
#include <string>

struct EngineModule;

// Keeps EngineModule forward-declared in this Swift-imported header; deletion is
// defined in EngineHost.cpp where the full EngineModule type is visible.
struct EngineModuleDeleter {
    void operator()(EngineModule* engineModule) const;
};

class EngineHost {
public:
    explicit EngineHost(std::string lib_path);
    EngineHost(const EngineHost&) = delete;
    EngineHost& operator=(const EngineHost&) = delete;
    EngineHost(EngineHost&&) noexcept = default;
    EngineHost& operator=(EngineHost&&) noexcept = default;

    bool valid() const;
    void attachSurface(void* native_surface, uint32_t width, uint32_t height);
    void resize(uint32_t width, uint32_t height);
    void tick(float dt);
    bool reload();

private:
    std::unique_ptr<EngineModule, EngineModuleDeleter> engineModule_;
};
