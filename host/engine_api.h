#pragma once
#include "engine/api.h"

#include <cstdio>

using EngineGetAPIFn = const EngineAPI* (*)();

inline bool engine_api_valid(const EngineAPI& api) {
    if (api.abi_version != ENGINE_API_VERSION) {
        std::fprintf(stderr, "[engine_api] incompatible ABI version: got %u, expected %u\n",
                     api.abi_version, ENGINE_API_VERSION);
        return false;
    }
    if (api.struct_size < sizeof(EngineAPI)) {
        std::fprintf(stderr, "[engine_api] incompatible API size: got %u, expected at least %zu\n",
                     api.struct_size, sizeof(EngineAPI));
        return false;
    }
    if (!api.init || !api.update) {
        std::fprintf(stderr, "[engine_api] missing required lifecycle functions\n");
        return false;
    }
    return true;
}
