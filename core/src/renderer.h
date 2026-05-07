#pragma once

#include "engine/api.h"

#include "Foundation/Foundation.hpp"
#include "QuartzCore/QuartzCore.hpp"
#include "Metal/Metal.hpp"

struct Renderer {
    bool init(SurfaceDescriptor* surface);
    void render_frame(float t);
    void shutdown();

private:
    CA::MetalLayer*    layer_  = nullptr;
    MTL::Device*       device_ = nullptr;
    MTL::CommandQueue* queue_  = nullptr;
};
