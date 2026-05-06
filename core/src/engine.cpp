#include "engine/api.h"
#include <cstdio>

static const char* VERSION = "v1"; // change this to demonstrate hot reload

extern "C" {

void engine_init(EngineState* state) {
    state->frame_count  = 0;
    state->elapsed_time = 0.0f;
    std::printf("[engine %s] init\n", VERSION);
    std::fflush(stdout);
}

void engine_update(EngineState* state, float dt) {
    state->frame_count++;
    state->elapsed_time += dt;
    std::printf("[engine %s] frame %llu  t=%.2fs\n",
                VERSION, state->frame_count, state->elapsed_time);
    std::fflush(stdout);
}

} // extern "C"
