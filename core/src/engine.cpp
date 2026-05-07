#include "engine/api.h"
#include "renderer.h"

#include <cstdio>

static const char* VERSION = "v1"; // change this to demonstrate hot reload

static Renderer g_renderer;

extern "C" {

void engine_init(EngineState* state, SurfaceDescriptor* surface) {
    state->frame_count  = 0;
    state->elapsed_time = 0.0f;
    g_renderer.init(surface);
    std::printf("[engine %s] init\n", VERSION);
    std::fflush(stdout);
}

void engine_resize(EngineState*, uint32_t width, uint32_t height) {
    g_renderer.resize(width, height);
}

void engine_update(EngineState* state, float dt) {
    state->frame_count++;
    state->elapsed_time += dt;
    g_renderer.render_frame(state->elapsed_time);
}

void engine_shutdown(EngineState*) {
    g_renderer.shutdown();
}

} // extern "C"
