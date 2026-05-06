#pragma once
#include "engine/api.h"

struct EngineAPI {
    void (*init)(EngineState*)          = nullptr;
    void (*update)(EngineState*, float) = nullptr;

    // Accepts any callable (const char*) -> void* so hosts are not coupled
    // to a specific loading mechanism (dlopen, mock, test stub, etc.)
    template<typename Lookup>
    void bind(Lookup lookup) {
        init   = reinterpret_cast<decltype(init)>(lookup("engine_init"));
        update = reinterpret_cast<decltype(update)>(lookup("engine_update"));
    }

    explicit operator bool() const { return init && update; }
};
