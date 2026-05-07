#pragma once
#include "engine/api.h"

struct EngineAPI {
    void (*init)(EngineState*, SurfaceDescriptor*) = nullptr;
    void (*resize)(EngineState*, uint32_t, uint32_t) = nullptr;
    void (*update)(EngineState*, float)            = nullptr;
    void (*shutdown)(EngineState*)                 = nullptr;

    // Accepts any callable (const char*) -> void* so hosts are not coupled
    // to a specific loading mechanism (dlopen, mock, test stub, etc.)
    template<typename Lookup>
    void bind(Lookup lookup) {
        init   = reinterpret_cast<decltype(init)>(lookup("engine_init"));
        resize = reinterpret_cast<decltype(resize)>(lookup("engine_resize"));
        update = reinterpret_cast<decltype(update)>(lookup("engine_update"));
        shutdown = reinterpret_cast<decltype(shutdown)>(lookup("engine_shutdown"));
    }

    explicit operator bool() const { return init && update; }
};
