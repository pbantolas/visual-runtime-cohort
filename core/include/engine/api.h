#pragma once
#include <cstdint>

struct EngineState {
    uint64_t frame_count;
    float    elapsed_time;
};

enum class SurfaceKind : uint32_t {
    None = 0,
    MacOSMetalLayer = 1,
    LinuxXcbWindow = 2,
    LinuxWaylandSurface = 3,
};

// Describes the native rendering surface passed to the engine on init.
// Handle fields are interpreted according to kind; null/zero for headless hosts.
struct SurfaceDescriptor {
    SurfaceKind kind;
    void*       display_handle;
    uintptr_t   surface_handle;
    uint32_t    width;
    uint32_t    height;
};

constexpr uint32_t ENGINE_API_VERSION = 4;

struct EngineAPI {
    uint32_t abi_version;
    uint32_t struct_size;
    const char* backend_name;

    void (*init)(EngineState*, SurfaceDescriptor*);
    void (*resize)(EngineState*, uint32_t, uint32_t);
    void (*update)(EngineState*, float);
    void (*shutdown)(EngineState*);
};

extern "C" {
    const EngineAPI* engine_get_api();
}
