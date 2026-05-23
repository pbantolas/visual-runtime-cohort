#include "renderer.h"

#include <vulkan/vulkan.h>

#include <cstdio>

struct RendererBackend {
  bool init(SurfaceDescriptor *surface);
  void resize(uint32_t width, uint32_t height);
  void render_frame(float t);
  void shutdown();

private:
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
      !surface->native_handle) {
    return false;
  }

  shutdown();
  resize(surface->width, surface->height);

  std::fprintf(stderr,
               "[renderer] Vulkan backend skeleton initialized "
               "(header version %u)\n",
               VK_HEADER_VERSION);
  return true;
}

void RendererBackend::resize(uint32_t width, uint32_t height) {
  render_width_ = width;
  render_height_ = height;
}

void RendererBackend::render_frame(float t) {
  (void)t;
  if (render_width_ == 0 || render_height_ == 0) {
    return;
  }
}

void RendererBackend::shutdown() {
  render_width_ = 0;
  render_height_ = 0;
}
