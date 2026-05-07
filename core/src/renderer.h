#pragma once

#include "engine/api.h"

#include "Foundation/Foundation.hpp"
#include "QuartzCore/QuartzCore.hpp"
#include "Metal/Metal.hpp"

struct Renderer {
    bool init(SurfaceDescriptor* surface);
    void resize(uint32_t width, uint32_t height);
    void render_frame(float t);
    void shutdown();

private:
    bool build_pipeline();
    bool build_geometry();

    CA::MetalLayer*            layer_          = nullptr;
    MTL::Device*               device_         = nullptr;
    MTL::CommandQueue*         queue_          = nullptr;
    MTL::Library*              library_        = nullptr;
    MTL::RenderPipelineState*  pipeline_       = nullptr;
    MTL::Buffer*               vertex_buffer_  = nullptr;
    NS::UInteger               vertex_count_   = 0;
    uint32_t                   render_width_   = 0;
    uint32_t                   render_height_  = 0;
};
