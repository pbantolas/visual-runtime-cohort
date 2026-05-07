#include "RuntimeBridge.h"

#include "runtime.h"

#include <cstdio>
#include <utility>

struct RuntimeBridge {
    Runtime runtime;

    explicit RuntimeBridge(Runtime runtime)
        : runtime(std::move(runtime)) {}
};

RuntimeBridge* runtime_open(const char* lib_path) {
    Runtime runtime = Runtime::open(lib_path);
    if (!runtime) return nullptr;

    return new RuntimeBridge(std::move(runtime));
}

void runtime_destroy(RuntimeBridge* bridge) {
    delete bridge;
}

void runtime_attach_surface(RuntimeBridge* bridge, const RuntimeSurfaceDescriptor* surface) {
    if (!bridge || !surface) return;

    SurfaceDescriptor runtime_surface{
        surface->metal_layer,
        surface->width,
        surface->height,
    };
    bridge->runtime.attach_surface(runtime_surface);
}

void runtime_resize(RuntimeBridge* bridge, uint32_t width, uint32_t height) {
    if (!bridge) return;

    bridge->runtime.resize(width, height);
}

void runtime_tick(RuntimeBridge* bridge, float dt) {
    if (!bridge) return;

    if (bridge->runtime.reload_if_changed()) {
        std::printf("[host] reloaded (frame %llu)\n", bridge->runtime.frame_count());
    }
    bridge->runtime.tick(dt);
}

bool runtime_reload(RuntimeBridge* bridge) {
    if (!bridge) return false;

    bool ok = bridge->runtime.reload();
    if (ok) {
        std::printf("[host] manually reloaded (frame %llu)\n", bridge->runtime.frame_count());
    }
    return ok;
}
