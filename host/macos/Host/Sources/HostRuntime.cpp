#include "HostRuntime.h"

#include "runtime.h"

#include <cstdio>
#include <utility>

void RuntimeDeleter::operator()(Runtime* runtime) const {
    delete runtime;
}

HostRuntime::HostRuntime(std::string lib_path) {
    Runtime runtime = Runtime::open(lib_path.c_str());
    if (!runtime) return;

    runtime_.reset(new Runtime(std::move(runtime)));
}

bool HostRuntime::valid() const {
    return runtime_ && static_cast<bool>(*runtime_);
}

void HostRuntime::attachSurface(void* metal_layer, uint32_t width, uint32_t height) {
    if (!runtime_) return;

    SurfaceDescriptor surface{
        metal_layer,
        width,
        height,
    };
    runtime_->attach_surface(surface);
}

void HostRuntime::resize(uint32_t width, uint32_t height) {
    if (!runtime_) return;

    runtime_->resize(width, height);
}

void HostRuntime::tick(float dt) {
    if (!runtime_) return;

    if (runtime_->reload_if_changed()) {
        std::printf("[host] reloaded (frame %llu)\n", runtime_->frame_count());
    }
    runtime_->tick(dt);
}

bool HostRuntime::reload() {
    if (!runtime_) return false;

    bool ok = runtime_->reload();
    if (ok) {
        std::printf("[host] manually reloaded (frame %llu)\n", runtime_->frame_count());
    }
    return ok;
}
