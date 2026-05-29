#define NS_PRIVATE_IMPLEMENTATION
#define CA_PRIVATE_IMPLEMENTATION
#define MTL_PRIVATE_IMPLEMENTATION
#include "renderer.h"

#include "Foundation/Foundation.hpp"
#include "Metal/Metal.hpp"
#include "QuartzCore/QuartzCore.hpp"

#include <cstddef>
#include <cstdio>
#include <glm/mat4x4.hpp>

namespace {

struct Vertex {
  float position[2];
  float color[3];
};

struct FrameUniforms {
  glm::mat4 matrix{1.0f};
};

void print_error(const char *context, NS::Error *error) {
  if (!error) {
    std::fprintf(stderr, "[renderer] %s\n", context);
    return;
  }

  NS::String *message = error->localizedDescription();
  std::fprintf(stderr, "[renderer] %s: %s\n", context,
               message ? message->utf8String() : "unknown error");
}

} // namespace

struct RendererBackend {
  bool init(SurfaceDescriptor *surface);
  void resize(uint32_t width, uint32_t height);
  void render_frame(float t);
  void shutdown();

private:
  bool build_pipeline();
  bool build_geometry();
  bool build_uniforms();
  void update_frame_uniforms();

  CA::MetalLayer *layer_ = nullptr;
  MTL::Device *device_ = nullptr;
  MTL::CommandQueue *queue_ = nullptr;
  MTL::Library *library_ = nullptr;
  MTL::RenderPipelineState *pipeline_ = nullptr;
  MTL::Buffer *vertex_buffer_ = nullptr;
  MTL::Buffer *frame_uniform_buffer_ = nullptr;
  NS::UInteger vertex_count_ = 0;
  uint32_t render_width_ = 0;
  uint32_t render_height_ = 0;
};

Renderer::Renderer() = default;
Renderer::~Renderer() = default;
Renderer::Renderer(Renderer &&) noexcept = default;
Renderer &Renderer::operator=(Renderer &&) noexcept = default;

bool Renderer::init(SurfaceDescriptor *surface) {
  if (!backend_) {
    backend_ = std::make_unique<RendererBackend>();
  }
  return backend_->init(surface);
}

void Renderer::resize(uint32_t width, uint32_t height) {
  if (backend_) {
    backend_->resize(width, height);
  }
}

void Renderer::render_frame(float t) {
  if (backend_) {
    backend_->render_frame(t);
  }
}

void Renderer::shutdown() {
  if (backend_) {
    backend_->shutdown();
    backend_.reset();
  }
}

bool RendererBackend::init(SurfaceDescriptor *surface) {
  if (!surface || surface->kind != SurfaceKind::MacOSMetalLayer ||
      surface->surface_handle == 0)
    return false;

  shutdown();

  layer_ = reinterpret_cast<CA::MetalLayer *>(surface->surface_handle);
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

  if (!build_uniforms()) {
    shutdown();
    return false;
  }

  resize(surface->width, surface->height);

  if (!build_pipeline() || !build_geometry()) {
    shutdown();
    return false;
  }

  return true;
}

void RendererBackend::resize(uint32_t width, uint32_t height) {
  if (render_width_ == width && render_height_ == height) {
    return;
  }

  render_width_ = width;
  render_height_ = height;
  update_frame_uniforms();
}

void RendererBackend::render_frame(float t) {
  if (!layer_ || !queue_ || !pipeline_ || !vertex_buffer_ ||
      !frame_uniform_buffer_)
    return;

  (void)t;

  NS::AutoreleasePool *pool = NS::AutoreleasePool::alloc()->init();

  CA::MetalDrawable *drawable = layer_->nextDrawable();
  if (!drawable) {
    pool->release();
    return;
  }

  MTL::RenderPassDescriptor *pass =
      MTL::RenderPassDescriptor::renderPassDescriptor();
  auto *color = pass->colorAttachments()->object(0);
  color->setTexture(drawable->texture());
  color->setLoadAction(MTL::LoadActionClear);
  color->setStoreAction(MTL::StoreActionStore);
  color->setClearColor(MTL::ClearColor(0, 0, 0, 1.0));

  auto *cmd = queue_->commandBuffer();
  auto *enc = cmd->renderCommandEncoder(pass);
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
  enc->setVertexBuffer(frame_uniform_buffer_, 0, 1);
  enc->drawPrimitives(MTL::PrimitiveTypeTriangle, NS::UInteger(0),
                      vertex_count_);
  enc->endEncoding();
  cmd->presentDrawable(drawable);
  cmd->commit();

  pool->release();
}

bool RendererBackend::build_pipeline() {
  NS::Error *error = nullptr;
  NS::String *shader_path =
      NS::String::string(VRT_SHADER_LIB_PATH, NS::UTF8StringEncoding);
  library_ = device_->newLibrary(shader_path, &error);
  if (!library_) {
    print_error("failed to load Metal library " VRT_SHADER_LIB_PATH, error);
    return false;
  }

  MTL::Function *vertex_fn = library_->newFunction(MTLSTR("vertex_main"));
  MTL::Function *fragment_fn = library_->newFunction(MTLSTR("fragment_main"));
  if (!vertex_fn || !fragment_fn) {
    std::fprintf(stderr, "[renderer] failed to find vertex_main/fragment_main "
                         "in Metal library\n");
    if (vertex_fn)
      vertex_fn->release();
    if (fragment_fn)
      fragment_fn->release();
    return false;
  }

  MTL::RenderPipelineDescriptor *desc =
      MTL::RenderPipelineDescriptor::alloc()->init();
  desc->setVertexFunction(vertex_fn);
  desc->setFragmentFunction(fragment_fn);
  desc->colorAttachments()->object(0)->setPixelFormat(
      MTL::PixelFormatBGRA8Unorm);

  MTL::VertexDescriptor *vertex_desc = MTL::VertexDescriptor::alloc()->init();
  auto *position_attr = vertex_desc->attributes()->object(0);
  position_attr->setFormat(MTL::VertexFormatFloat2);
  position_attr->setOffset(offsetof(Vertex, position));
  position_attr->setBufferIndex(0);

  auto *color_attr = vertex_desc->attributes()->object(1);
  color_attr->setFormat(MTL::VertexFormatFloat3);
  color_attr->setOffset(offsetof(Vertex, color));
  color_attr->setBufferIndex(0);

  auto *vertex_layout = vertex_desc->layouts()->object(0);
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

bool RendererBackend::build_geometry() {
  static constexpr Vertex vertices[] = {
      {{0.0f, 0.65f}, {1.0f, 0.0f, 0.0f}},
      {{-0.7f, -0.55f}, {0.0f, 1.0f, 0.0f}},
      {{0.7f, -0.55f}, {0.0f, 0.0f, 1.0f}},
  };

  vertex_count_ = sizeof(vertices) / sizeof(vertices[0]);
  vertex_buffer_ = device_->newBuffer(vertices, sizeof(vertices),
                                      MTL::ResourceStorageModeShared);
  if (!vertex_buffer_) {
    std::fprintf(stderr, "[renderer] failed to create vertex buffer\n");
    return false;
  }

  return true;
}

bool RendererBackend::build_uniforms() {
  frame_uniform_buffer_ =
      device_->newBuffer(sizeof(FrameUniforms), MTL::ResourceStorageModeShared);
  if (!frame_uniform_buffer_) {
    std::fprintf(stderr, "[renderer] failed to create frame uniform buffer\n");
    return false;
  }

  update_frame_uniforms();
  return true;
}

void RendererBackend::update_frame_uniforms() {
  if (!frame_uniform_buffer_) {
    return;
  }

  FrameUniforms uniforms{};

  if (render_width_ > 0 && render_height_ > 0) {
    const float aspect =
        static_cast<float>(render_width_) / static_cast<float>(render_height_);

    if (aspect >= 1.0f) {
      uniforms.matrix[0][0] = 1.0f / aspect;
    } else {
      uniforms.matrix[1][1] = aspect;
    }
  }

  auto *contents =
      static_cast<FrameUniforms *>(frame_uniform_buffer_->contents());
  *contents = uniforms;
}

void RendererBackend::shutdown() {
  if (frame_uniform_buffer_) {
    frame_uniform_buffer_->release();
    frame_uniform_buffer_ = nullptr;
  }
  if (vertex_buffer_) {
    vertex_buffer_->release();
    vertex_buffer_ = nullptr;
  }
  if (pipeline_) {
    pipeline_->release();
    pipeline_ = nullptr;
  }
  if (library_) {
    library_->release();
    library_ = nullptr;
  }
  if (queue_) {
    queue_->release();
    queue_ = nullptr;
  }
  if (device_) {
    device_->release();
    device_ = nullptr;
  }
  if (layer_) {
    layer_->release();
    layer_ = nullptr;
  }
  vertex_count_ = 0;
  render_width_ = 0;
  render_height_ = 0;
}
