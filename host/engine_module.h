#pragma once

#include "dynamic_library.h"
#include "engine_api.h"

#include <cstdint>
#include <utility>

struct EngineModule {
    EngineModule(const EngineModule&) = delete;
    EngineModule& operator=(const EngineModule&) = delete;
    EngineModule(EngineModule&& other) noexcept
        : lib_(std::move(other.lib_)),
          api_(other.api_),
          api_bound_(other.api_bound_),
          state_(other.state_),
          surface_(other.surface_),
          has_surface_(other.has_surface_),
          initialized_(other.initialized_) {
        other.initialized_ = false;
        other.api_bound_ = false;
    }
    ~EngineModule() {
        shutdown();
    }

    static EngineModule open(const char* lib_path) {
        return EngineModule(DynamicLibrary::open(lib_path));
    }

    void attachSurface(SurfaceDescriptor surface) {
        surface_ = surface;
        has_surface_ = true;
        init();
    }

    void resize(uint32_t width, uint32_t height) {
        surface_.width = width;
        surface_.height = height;
        if (initialized_ && api_.resize) api_.resize(&state_, width, height);
    }

    bool reloadIfChanged() {
        if (!lib_.changed()) return false;
        return reload();
    }

    bool reload() {
        shutdown();
        if (!lib_.reload()) return false;

        bindApi();
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

    uint64_t frameCount() const {
        return state_.frame_count;
    }

    explicit operator bool() const {
        return bool(lib_) && api_bound_;
    }

private:
    explicit EngineModule(DynamicLibrary lib) : lib_(std::move(lib)) {
        if (lib_) {
            bindApi();
            init();
        }
    }

    void bindApi() {
        api_ = {};
        api_bound_ = false;

        auto get_api = reinterpret_cast<EngineGetAPIFn>(lib_.sym("engine_get_api"));
        if (!get_api) return;

        const EngineAPI* loaded_api = get_api();
        if (!loaded_api) {
            std::fprintf(stderr, "[engine_module] engine_get_api returned null\n");
            return;
        }
        if (!engine_api_valid(*loaded_api)) return;

        api_ = *loaded_api;
        api_bound_ = true;
    }

    void init() const {
        if (!api_.init) return;
        api_.init(&state_, has_surface_ ? &surface_ : nullptr);
        initialized_ = true;
    }

    DynamicLibrary lib_;
    EngineAPI api_;
    bool api_bound_ = false;
    mutable EngineState state_{};
    mutable SurfaceDescriptor surface_{};
    bool has_surface_ = false;
    mutable bool initialized_ = false;
};
