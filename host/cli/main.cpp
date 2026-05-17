#include "engine_module.h"

#include <chrono>
#include <csignal>
#include <cstdio>
#include <thread>

static volatile bool running = true;
static void on_signal(int) { running = false; }

int main() {
    std::signal(SIGINT, on_signal);
    std::setvbuf(stdout, nullptr, _IONBF, 0);

    EngineModule engine = EngineModule::open(ENGINE_LIB_PATH);
    if (!engine) return 1;

    using clock = std::chrono::steady_clock;
    auto last = clock::now();

    while (running) {
        if (engine.reloadIfChanged())
            std::printf("[host] reloaded (frame %llu)\n", engine.frameCount());

        auto now = clock::now();
        float dt = std::chrono::duration<float>(now - last).count();
        last = now;

        engine.tick(dt);

        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    std::printf("\n[host] exiting after %llu frames\n", engine.frameCount());
}
