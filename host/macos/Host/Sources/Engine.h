#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Engine Engine;

typedef struct EngineSurfaceDescriptor {
    void* metal_layer;
    uint32_t width;
    uint32_t height;
} EngineSurfaceDescriptor;

Engine* engine_open(const char* lib_path);
void engine_destroy(Engine* engine);

void engine_attach_surface(Engine* engine, const EngineSurfaceDescriptor* surface);
void engine_tick(Engine* engine, float dt);

#ifdef __cplusplus
}
#endif
