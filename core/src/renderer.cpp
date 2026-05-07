#define NS_PRIVATE_IMPLEMENTATION
#define CA_PRIVATE_IMPLEMENTATION
#define MTL_PRIVATE_IMPLEMENTATION
#include "renderer.h"

#include <cmath>
#include <cstdio>

bool Renderer::init(SurfaceDescriptor* surface) {
    if (!surface || !surface->metal_layer) return false;

    shutdown();

    layer_ = reinterpret_cast<CA::MetalLayer*>(surface->metal_layer);
    layer_->retain();

    device_ = MTL::CreateSystemDefaultDevice();
    if (!device_) {
        std::fprintf(stderr, "[renderer] failed to create Metal device\n");
        shutdown();
        return false;
    }

    queue_ = device_->newCommandQueue();
    if (!queue_) {
        std::fprintf(stderr, "[renderer] failed to create Metal command queue\n");
        shutdown();
        return false;
    }

    layer_->setDevice(device_);
    layer_->setPixelFormat(MTL::PixelFormatBGRA8Unorm);
    layer_->setFramebufferOnly(true);
    layer_->setDrawableSize(CGSizeMake(surface->width, surface->height));
    return true;
}

void Renderer::render_frame(float t) {
    if (!layer_ || !queue_) return;

    NS::AutoreleasePool* pool = NS::AutoreleasePool::alloc()->init();

    CA::MetalDrawable* drawable = layer_->nextDrawable();
    if (!drawable) { pool->release(); return; }

    // Three-phase sine at low amplitude — slow hue drift without full saturation
    float hue = t * 0.25f;
    float r = 0.08f + 0.06f * std::sin(hue);
    float g = 0.08f + 0.06f * std::sin(hue + 2.094f);
    float b = 0.10f + 0.06f * std::sin(hue + 4.189f);

    MTL::RenderPassDescriptor* pass = MTL::RenderPassDescriptor::renderPassDescriptor();
    auto* color = pass->colorAttachments()->object(0);
    color->setTexture(drawable->texture());
    color->setLoadAction(MTL::LoadActionClear);
    color->setStoreAction(MTL::StoreActionStore);
    color->setClearColor(MTL::ClearColor(r, g, b, 1.0));

    auto* cmd = queue_->commandBuffer();
    auto* enc = cmd->renderCommandEncoder(pass);
    enc->endEncoding();
    cmd->presentDrawable(drawable);
    cmd->commit();

    pool->release();
}

void Renderer::shutdown() {
    if (queue_)  { queue_->release();  queue_  = nullptr; }
    if (device_) { device_->release(); device_ = nullptr; }
    if (layer_)  { layer_->release();  layer_  = nullptr; }
}
