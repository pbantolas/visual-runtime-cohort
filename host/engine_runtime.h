#pragma once

#include "hot_reload.h"
#include "engine_api.h"

#include <cstdint>
#include <utility>

struct EngineRuntime {
    EngineRuntime(const EngineRuntime&) = delete;
    EngineRuntime& operator=(const EngineRuntime&) = delete;
    EngineRuntime(EngineRuntime&& other) noexcept
        : lib_(std::move(other.lib_)),
          api_(other.api_),
          state_(other.state_),
          surface_(other.surface_),
          has_surface_(other.has_surface_),
          initialized_(other.initialized_) {
        other.initialized_ = false;
    }
    ~EngineRuntime() {
        shutdown();
    }

    static EngineRuntime open(const char* lib_path) {
        return EngineRuntime(HotLib::open(lib_path));
    }

    void attach_surface(SurfaceDescriptor surface) {
        surface_ = surface;
        has_surface_ = true;
        init();
    }

    bool reload_if_changed() {
        if (!lib_.changed()) return false;

        shutdown();
        if (!lib_.reload()) return false;

        bind_api();
        init();
        return true;
    }

    void tick(float dt) const {
        if (api_.update) api_.update(&state_, dt);
    }

    void shutdown() const {
        if (!initialized_) return;
        if (api_.shutdown) api_.shutdown(&state_);
        initialized_ = false;
    }

    uint64_t frame_count() const {
        return state_.frame_count;
    }

    explicit operator bool() const {
        return bool(lib_) && bool(api_);
    }

private:
    explicit EngineRuntime(HotLib lib) : lib_(std::move(lib)) {
        if (lib_) {
            bind_api();
            init();
        }
    }

    void bind_api() {
        api_.bind([this](const char* name) { return lib_.sym(name); });
    }

    void init() const {
        if (!api_.init) return;
        api_.init(&state_, has_surface_ ? &surface_ : nullptr);
        initialized_ = true;
    }

    HotLib lib_;
    EngineAPI api_;
    mutable EngineState state_{};
    mutable SurfaceDescriptor surface_{};
    bool has_surface_ = false;
    mutable bool initialized_ = false;
};
