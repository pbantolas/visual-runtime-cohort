#include "engine/api.h"
#include "renderer.h"

#include <cstdio>

static const char* VERSION = "v1"; // change this to demonstrate hot reload

static Renderer g_renderer;

static void engine_init_impl(EngineState* state, SurfaceDescriptor* surface) {
    state->frame_count  = 0;
    state->elapsed_time = 0.0f;
    g_renderer.init(surface);
    std::printf("[engine %s] init\n", VERSION);
    std::fflush(stdout);
}

static void engine_resize_impl(EngineState*, uint32_t width, uint32_t height) {
    g_renderer.resize(width, height);
}

static void engine_update_impl(EngineState* state, float dt) {
    state->frame_count++;
    state->elapsed_time += dt;
    g_renderer.render_frame(state->elapsed_time);
}

static void engine_shutdown_impl(EngineState*) {
    g_renderer.shutdown();
}

extern "C" {

const EngineAPI* engine_get_api() {
    static const EngineAPI api{
        ENGINE_API_VERSION,
        sizeof(EngineAPI),
        ENGINE_BACKEND_NAME,
        engine_init_impl,
        engine_resize_impl,
        engine_update_impl,
        engine_shutdown_impl,
    };
    return &api;
}

} // extern "C"
