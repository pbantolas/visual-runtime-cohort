#include "engine/api.h"

#define NS_PRIVATE_IMPLEMENTATION
#define CA_PRIVATE_IMPLEMENTATION
#define MTL_PRIVATE_IMPLEMENTATION
#include "Foundation/Foundation.hpp"
#include "QuartzCore/QuartzCore.hpp"
#include "Metal/Metal.hpp"

#include <cstdio>

static const char* VERSION = "v1"; // change this to demonstrate hot reload

struct MetalContext {
    CA::MetalLayer* layer = nullptr;
    MTL::Device* device = nullptr;
    MTL::CommandQueue* queue = nullptr;
};

static MetalContext g_metal;

static void destroy_metal() {
    if (g_metal.queue) {
        g_metal.queue->release();
        g_metal.queue = nullptr;
    }
    if (g_metal.device) {
        g_metal.device->release();
        g_metal.device = nullptr;
    }
    if (g_metal.layer) {
        g_metal.layer->release();
        g_metal.layer = nullptr;
    }
}

static bool setup_metal(SurfaceDescriptor* surface) {
    if (!surface || !surface->metal_layer) return false;

    destroy_metal();

    g_metal.layer = reinterpret_cast<CA::MetalLayer*>(surface->metal_layer);
    g_metal.layer->retain();

    g_metal.device = MTL::CreateSystemDefaultDevice();
    if (!g_metal.device) {
        std::fprintf(stderr, "[engine %s] failed to create Metal device\n", VERSION);
        destroy_metal();
        return false;
    }

    g_metal.queue = g_metal.device->newCommandQueue();
    if (!g_metal.queue) {
        std::fprintf(stderr, "[engine %s] failed to create Metal command queue\n", VERSION);
        destroy_metal();
        return false;
    }

    g_metal.layer->setDevice(g_metal.device);
    g_metal.layer->setPixelFormat(MTL::PixelFormatBGRA8Unorm);
    g_metal.layer->setFramebufferOnly(true);
    g_metal.layer->setDrawableSize(CGSizeMake(surface->width, surface->height));
    return true;
}

static void clear_black() {
    if (!g_metal.layer || !g_metal.queue) return;

    NS::AutoreleasePool* pool = NS::AutoreleasePool::alloc()->init();

    CA::MetalDrawable* drawable = g_metal.layer->nextDrawable();
    if (!drawable) {
        pool->release();
        return;
    }

    MTL::RenderPassDescriptor* pass = MTL::RenderPassDescriptor::renderPassDescriptor();
    MTL::RenderPassColorAttachmentDescriptor* color = pass->colorAttachments()->object(0);
    color->setTexture(drawable->texture());
    color->setLoadAction(MTL::LoadActionClear);
    color->setStoreAction(MTL::StoreActionStore);
    color->setClearColor(MTL::ClearColor(0.0, 0.0, 0.0, 1.0));

    MTL::CommandBuffer* command_buffer = g_metal.queue->commandBuffer();
    MTL::RenderCommandEncoder* encoder = command_buffer->renderCommandEncoder(pass);
    encoder->endEncoding();
    command_buffer->presentDrawable(drawable);
    command_buffer->commit();

    pool->release();
}

extern "C" {

void engine_init(EngineState* state, SurfaceDescriptor* surface) {
    state->frame_count  = 0;
    state->elapsed_time = 0.0f;
    setup_metal(surface);
    std::printf("[engine %s] init\n", VERSION);
    std::fflush(stdout);
}

void engine_update(EngineState* state, float dt) {
    state->frame_count++;
    state->elapsed_time += dt;
    clear_black();
}

void engine_shutdown(EngineState*) {
    // Reserved for renderer cleanup before the host unloads this dylib.
}

} // extern "C"
