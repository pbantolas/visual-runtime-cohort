#pragma once
#include <cstdint>

struct EngineState {
    uint64_t frame_count;
    float    elapsed_time;
};

extern "C" {
    void engine_init(EngineState* state);
    void engine_update(EngineState* state, float dt);
}
