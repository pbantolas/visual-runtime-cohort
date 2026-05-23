#include "EngineHost.h"

#include "engine_module.h"

#include <cstdio>
#include <utility>

void EngineModuleDeleter::operator()(EngineModule* engineModule) const {
    delete engineModule;
}

EngineHost::EngineHost(std::string lib_path) {
    EngineModule engineModule = EngineModule::open(lib_path.c_str());
    if (!engineModule) return;

    engineModule_.reset(new EngineModule(std::move(engineModule)));
}

bool EngineHost::valid() const {
    return engineModule_ && static_cast<bool>(*engineModule_);
}

void EngineHost::attachSurface(void* native_surface, uint32_t width, uint32_t height) {
    if (!engineModule_) return;

    SurfaceDescriptor surface{
        SurfaceKind::MacOSMetalLayer,
        native_surface,
        width,
        height,
    };
    engineModule_->attachSurface(surface);
}

void EngineHost::resize(uint32_t width, uint32_t height) {
    if (!engineModule_) return;

    engineModule_->resize(width, height);
}

void EngineHost::tick(float dt) {
    if (!engineModule_) return;

    if (engineModule_->reloadIfChanged()) {
        std::printf("[host] reloaded (frame %llu)\n", engineModule_->frameCount());
    }
    engineModule_->tick(dt);
}

bool EngineHost::reload() {
    if (!engineModule_) return false;

    bool ok = engineModule_->reload();
    if (ok) {
        std::printf("[host] manually reloaded (frame %llu)\n", engineModule_->frameCount());
    }
    return ok;
}
