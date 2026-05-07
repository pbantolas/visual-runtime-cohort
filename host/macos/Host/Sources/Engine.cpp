#include "Engine.h"

#include "engine_runtime.h"

#include <cstdio>
#include <utility>

struct Engine {
    EngineRuntime runtime;

    explicit Engine(EngineRuntime engine_runtime)
        : runtime(std::move(engine_runtime)) {}
};

Engine* engine_open(const char* lib_path) {
    EngineRuntime runtime = EngineRuntime::open(lib_path);
    if (!runtime) return nullptr;

    return new Engine(std::move(runtime));
}

void engine_destroy(Engine* engine) {
    delete engine;
}

void engine_attach_surface(Engine* engine, const EngineSurfaceDescriptor* surface) {
    if (!engine || !surface) return;

    SurfaceDescriptor engine_surface{
        surface->metal_layer,
        surface->width,
        surface->height,
    };
    engine->runtime.attach_surface(engine_surface);
}

void engine_tick(Engine* engine, float dt) {
    if (!engine) return;

    if (engine->runtime.reload_if_changed()) {
        std::printf("[host] reloaded (frame %llu)\n", engine->runtime.frame_count());
    }
    engine->runtime.tick(dt);
}
