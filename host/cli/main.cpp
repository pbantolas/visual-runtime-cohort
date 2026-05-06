#include "hot_reload.h"
#include "engine_api.h"

#include <chrono>
#include <csignal>
#include <cstdio>
#include <thread>

static volatile bool running = true;
static void on_signal(int) { running = false; }

int main() {
    std::signal(SIGINT, on_signal);
    std::setvbuf(stdout, nullptr, _IONBF, 0);

    HotLib lib = HotLib::open(ENGINE_LIB_PATH);
    if (!lib) return 1;

    EngineAPI api;
    api.bind([&](const char* name) { return lib.sym(name); });
    if (!api) return 1;

    EngineState state{};
    api.init(&state);

    using clock = std::chrono::steady_clock;
    auto last = clock::now();

    while (running) {
        if (lib.changed()) {
            std::printf("[host] reloading...\n");
            if (lib.reload()) {
                api.bind([&](const char* name) { return lib.sym(name); });
                std::printf("[host] reload ok (frame %llu)\n", state.frame_count);
            }
        }

        auto now = clock::now();
        float dt = std::chrono::duration<float>(now - last).count();
        last = now;

        api.update(&state, dt);

        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    std::printf("\n[host] exiting after %llu frames\n", state.frame_count);
}
