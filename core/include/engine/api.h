#pragma once
#include <cstdint>

struct EngineState {
    uint64_t frame_count;
    float    elapsed_time;
};

// Describes the native rendering surface passed to the engine on init.
// metal_layer is a CAMetalLayer* on Apple platforms; null for headless hosts.
struct SurfaceDescriptor {
    void*    metal_layer;
    uint32_t width;
    uint32_t height;
};

constexpr uint32_t ENGINE_API_VERSION = 1;

struct EngineAPI {
    uint32_t abi_version;
    uint32_t struct_size;

    void (*init)(EngineState*, SurfaceDescriptor*);
    void (*resize)(EngineState*, uint32_t, uint32_t);
    void (*update)(EngineState*, float);
    void (*shutdown)(EngineState*);
};

extern "C" {
    const EngineAPI* engine_get_api();
}
