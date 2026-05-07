#define NS_PRIVATE_IMPLEMENTATION
#define CA_PRIVATE_IMPLEMENTATION
#define MTL_PRIVATE_IMPLEMENTATION
#include "renderer.h"

#include <cstddef>
#include <cmath>
#include <cstdio>

namespace {

struct Vertex {
    float position[2];
    float color[3];
};

void print_error(const char* context, NS::Error* error) {
    if (!error) {
        std::fprintf(stderr, "[renderer] %s\n", context);
        return;
    }

    NS::String* message = error->localizedDescription();
    std::fprintf(stderr, "[renderer] %s: %s\n", context,
                 message ? message->utf8String() : "unknown error");
}

} // namespace

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
    resize(surface->width, surface->height);

    if (!build_pipeline() || !build_geometry()) {
        shutdown();
        return false;
    }

    return true;
}

void Renderer::resize(uint32_t width, uint32_t height) {
    render_width_ = width;
    render_height_ = height;
}

void Renderer::render_frame(float t) {
    if (!layer_ || !queue_ || !pipeline_ || !vertex_buffer_) return;

    NS::AutoreleasePool* pool = NS::AutoreleasePool::alloc()->init();

    CA::MetalDrawable* drawable = layer_->nextDrawable();
    if (!drawable) { pool->release(); return; }

    // Keep the clear color moving by at least ~1 8-bit step most frames.
    // The previous 0.06-amplitude / 0.25-rad/s drift quantized to only ~30
    // displayable values in BGRA8Unorm, so it visibly held for many frames.
    float hue = t * 1.0f;
    float r = 0.35f + 0.30f * std::sin(hue);
    float g = 0.35f + 0.30f * std::sin(hue + 2.094f);
    float b = 0.40f + 0.30f * std::sin(hue + 4.189f);

    MTL::RenderPassDescriptor* pass = MTL::RenderPassDescriptor::renderPassDescriptor();
    auto* color = pass->colorAttachments()->object(0);
    color->setTexture(drawable->texture());
    color->setLoadAction(MTL::LoadActionClear);
    color->setStoreAction(MTL::StoreActionStore);
    color->setClearColor(MTL::ClearColor(r, g, b, 1.0));

    auto* cmd = queue_->commandBuffer();
    auto* enc = cmd->renderCommandEncoder(pass);
    enc->setViewport(MTL::Viewport{
        0.0,
        0.0,
        static_cast<double>(render_width_),
        static_cast<double>(render_height_),
        0.0,
        1.0,
    });
    enc->setRenderPipelineState(pipeline_);
    enc->setVertexBuffer(vertex_buffer_, 0, 0);
    enc->drawPrimitives(MTL::PrimitiveTypeTriangle, NS::UInteger(0), vertex_count_);
    enc->endEncoding();
    cmd->presentDrawable(drawable);
    cmd->commit();

    pool->release();
}

bool Renderer::build_pipeline() {
    NS::Error* error = nullptr;
    NS::String* shader_path = NS::String::string(ENGINE_SHADER_LIB_PATH, NS::UTF8StringEncoding);
    library_ = device_->newLibrary(shader_path, &error);
    if (!library_) {
        print_error("failed to load Metal library " ENGINE_SHADER_LIB_PATH, error);
        return false;
    }

    MTL::Function* vertex_fn = library_->newFunction(MTLSTR("vertex_main"));
    MTL::Function* fragment_fn = library_->newFunction(MTLSTR("fragment_main"));
    if (!vertex_fn || !fragment_fn) {
        std::fprintf(stderr, "[renderer] failed to find vertex_main/fragment_main in Metal library\n");
        if (vertex_fn) vertex_fn->release();
        if (fragment_fn) fragment_fn->release();
        return false;
    }

    MTL::RenderPipelineDescriptor* desc = MTL::RenderPipelineDescriptor::alloc()->init();
    desc->setVertexFunction(vertex_fn);
    desc->setFragmentFunction(fragment_fn);
    desc->colorAttachments()->object(0)->setPixelFormat(MTL::PixelFormatBGRA8Unorm);

    MTL::VertexDescriptor* vertex_desc = MTL::VertexDescriptor::alloc()->init();
    auto* position_attr = vertex_desc->attributes()->object(0);
    position_attr->setFormat(MTL::VertexFormatFloat2);
    position_attr->setOffset(offsetof(Vertex, position));
    position_attr->setBufferIndex(0);

    auto* color_attr = vertex_desc->attributes()->object(1);
    color_attr->setFormat(MTL::VertexFormatFloat3);
    color_attr->setOffset(offsetof(Vertex, color));
    color_attr->setBufferIndex(0);

    auto* vertex_layout = vertex_desc->layouts()->object(0);
    vertex_layout->setStride(sizeof(Vertex));
    vertex_layout->setStepFunction(MTL::VertexStepFunctionPerVertex);
    vertex_layout->setStepRate(1);

    desc->setVertexDescriptor(vertex_desc);

    pipeline_ = device_->newRenderPipelineState(desc, &error);

    vertex_desc->release();
    desc->release();
    vertex_fn->release();
    fragment_fn->release();

    if (!pipeline_) {
        print_error("failed to create render pipeline", error);
        return false;
    }

    return true;
}

bool Renderer::build_geometry() {
    static constexpr Vertex vertices[] = {
        {{ 0.0f,  0.65f}, {0.0f, 0.0f, 0.0f}},
        {{-0.7f, -0.55f}, {0.0f, 0.0f, 0.0f}},
        {{ 0.7f, -0.55f}, {0.0f, 0.0f, 0.0f}},
    };

    vertex_count_ = sizeof(vertices) / sizeof(vertices[0]);
    vertex_buffer_ = device_->newBuffer(vertices, sizeof(vertices), MTL::ResourceStorageModeShared);
    if (!vertex_buffer_) {
        std::fprintf(stderr, "[renderer] failed to create vertex buffer\n");
        return false;
    }

    return true;
}

void Renderer::shutdown() {
    if (vertex_buffer_) { vertex_buffer_->release(); vertex_buffer_ = nullptr; }
    if (pipeline_)      { pipeline_->release();      pipeline_      = nullptr; }
    if (library_)       { library_->release();       library_       = nullptr; }
    if (queue_)         { queue_->release();         queue_         = nullptr; }
    if (device_)        { device_->release();        device_        = nullptr; }
    if (layer_)         { layer_->release();         layer_         = nullptr; }
    vertex_count_ = 0;
    render_width_ = 0;
    render_height_ = 0;
}
