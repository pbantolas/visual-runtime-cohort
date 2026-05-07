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

extern "C" {
    void engine_init(EngineState* state, SurfaceDescriptor* surface);
    void engine_resize(EngineState* state, uint32_t width, uint32_t height);
    void engine_update(EngineState* state, float dt);
    void engine_shutdown(EngineState* state);
}
