#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct RuntimeBridge RuntimeBridge;

typedef struct RuntimeSurfaceDescriptor {
    void* metal_layer;
    uint32_t width;
    uint32_t height;
} RuntimeSurfaceDescriptor;

RuntimeBridge* runtime_open(const char* lib_path);
void runtime_destroy(RuntimeBridge* bridge);

void runtime_attach_surface(RuntimeBridge* bridge, const RuntimeSurfaceDescriptor* surface);
void runtime_resize(RuntimeBridge* bridge, uint32_t width, uint32_t height);
void runtime_tick(RuntimeBridge* bridge, float dt);
bool runtime_reload(RuntimeBridge* bridge);

#ifdef __cplusplus
}
#endif
