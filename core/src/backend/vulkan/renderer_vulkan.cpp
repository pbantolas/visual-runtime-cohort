#include "renderer.h"

#include <vulkan/vulkan.h>
#if defined(__APPLE__)
#include <vulkan/vulkan_metal.h>
#endif

#include <cstring>
#include <cstdio>
#include <vector>

namespace {

const char *vk_result_name(VkResult result) {
  switch (result) {
  case VK_SUCCESS:
    return "VK_SUCCESS";
  case VK_NOT_READY:
    return "VK_NOT_READY";
  case VK_TIMEOUT:
    return "VK_TIMEOUT";
  case VK_EVENT_SET:
    return "VK_EVENT_SET";
  case VK_EVENT_RESET:
    return "VK_EVENT_RESET";
  case VK_INCOMPLETE:
    return "VK_INCOMPLETE";
  case VK_ERROR_OUT_OF_HOST_MEMORY:
    return "VK_ERROR_OUT_OF_HOST_MEMORY";
  case VK_ERROR_OUT_OF_DEVICE_MEMORY:
    return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
  case VK_ERROR_INITIALIZATION_FAILED:
    return "VK_ERROR_INITIALIZATION_FAILED";
  case VK_ERROR_DEVICE_LOST:
    return "VK_ERROR_DEVICE_LOST";
  case VK_ERROR_MEMORY_MAP_FAILED:
    return "VK_ERROR_MEMORY_MAP_FAILED";
  case VK_ERROR_LAYER_NOT_PRESENT:
    return "VK_ERROR_LAYER_NOT_PRESENT";
  case VK_ERROR_EXTENSION_NOT_PRESENT:
    return "VK_ERROR_EXTENSION_NOT_PRESENT";
  case VK_ERROR_FEATURE_NOT_PRESENT:
    return "VK_ERROR_FEATURE_NOT_PRESENT";
  case VK_ERROR_INCOMPATIBLE_DRIVER:
    return "VK_ERROR_INCOMPATIBLE_DRIVER";
  case VK_ERROR_TOO_MANY_OBJECTS:
    return "VK_ERROR_TOO_MANY_OBJECTS";
  case VK_ERROR_FORMAT_NOT_SUPPORTED:
    return "VK_ERROR_FORMAT_NOT_SUPPORTED";
  case VK_ERROR_SURFACE_LOST_KHR:
    return "VK_ERROR_SURFACE_LOST_KHR";
  case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR:
    return "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR";
  default:
    return "unknown VkResult";
  }
}

void print_vk_error(const char *context, VkResult result) {
  std::fprintf(stderr, "[renderer] %s: %s (%d)\n", context,
               vk_result_name(result), result);
}

bool check_vk(VkResult result, const char *context) {
  if (result == VK_SUCCESS) {
    return true;
  }

  print_vk_error(context, result);
  return false;
}

bool extension_available(const std::vector<VkExtensionProperties> &extensions,
                         const char *name) {
  for (const auto &extension : extensions) {
    if (std::strcmp(extension.extensionName, name) == 0) {
      return true;
    }
  }
  return false;
}

bool required_instance_extensions_available(const char *const *required,
                                            uint32_t required_count) {
  uint32_t extension_count = 0;
  if (!check_vk(vkEnumerateInstanceExtensionProperties(
                    nullptr, &extension_count, nullptr),
                "failed to count Vulkan instance extensions")) {
    return false;
  }

  std::vector<VkExtensionProperties> extensions(extension_count);
  if (!check_vk(vkEnumerateInstanceExtensionProperties(
                    nullptr, &extension_count, extensions.data()),
                "failed to enumerate Vulkan instance extensions")) {
    return false;
  }

  for (uint32_t i = 0; i < required_count; ++i) {
    if (!extension_available(extensions, required[i])) {
      std::fprintf(stderr,
                   "[renderer] missing required Vulkan instance extension: %s\n",
                   required[i]);
      return false;
    }
  }

  return true;
}

} // namespace

struct RendererBackend {
  bool init(SurfaceDescriptor *surface);
  void resize(uint32_t width, uint32_t height);
  void render_frame(float t);
  void shutdown();

private:
  bool create_instance();
  bool create_surface(void *native_handle);

  VkInstance instance_ = VK_NULL_HANDLE;
  VkSurfaceKHR surface_ = VK_NULL_HANDLE;
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

  if (!create_instance() || !create_surface(surface->native_handle)) {
    shutdown();
    return false;
  }

  resize(surface->width, surface->height);

  std::fprintf(stderr, "[renderer] Vulkan instance and surface initialized\n");
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
  if (surface_ != VK_NULL_HANDLE) {
    vkDestroySurfaceKHR(instance_, surface_, nullptr);
    surface_ = VK_NULL_HANDLE;
  }
  if (instance_ != VK_NULL_HANDLE) {
    vkDestroyInstance(instance_, nullptr);
    instance_ = VK_NULL_HANDLE;
  }
  render_width_ = 0;
  render_height_ = 0;
}

bool RendererBackend::create_instance() {
  std::vector<const char *> extensions{VK_KHR_SURFACE_EXTENSION_NAME};
#if defined(__APPLE__)
  extensions.push_back(VK_EXT_METAL_SURFACE_EXTENSION_NAME);
#endif

  if (!required_instance_extensions_available(extensions.data(),
                                              static_cast<uint32_t>(
                                                  extensions.size()))) {
    return false;
  }

  VkApplicationInfo app_info{};
  app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  app_info.pApplicationName = "Graphics Engine Cohort";
  app_info.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
  app_info.pEngineName = "Graphics Engine";
  app_info.engineVersion = VK_MAKE_VERSION(0, 1, 0);
  app_info.apiVersion = VK_API_VERSION_1_3;

  VkInstanceCreateInfo create_info{};
  create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  create_info.pApplicationInfo = &app_info;
  create_info.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
  create_info.ppEnabledExtensionNames = extensions.data();

  if (!check_vk(vkCreateInstance(&create_info, nullptr, &instance_),
                "failed to create Vulkan instance")) {
    return false;
  }

  return true;
}

bool RendererBackend::create_surface(void *native_handle) {
#if defined(__APPLE__)
  auto create_metal_surface =
      reinterpret_cast<PFN_vkCreateMetalSurfaceEXT>(
          vkGetInstanceProcAddr(instance_, "vkCreateMetalSurfaceEXT"));
  if (!create_metal_surface) {
    std::fprintf(stderr,
                 "[renderer] failed to load vkCreateMetalSurfaceEXT\n");
    return false;
  }

  VkMetalSurfaceCreateInfoEXT create_info{};
  create_info.sType = VK_STRUCTURE_TYPE_METAL_SURFACE_CREATE_INFO_EXT;
  create_info.pLayer = native_handle;

  if (!check_vk(create_metal_surface(instance_, &create_info, nullptr,
                                     &surface_),
                "failed to create Vulkan Metal surface")) {
    return false;
  }

  return true;
#else
  (void)native_handle;
  std::fprintf(stderr,
               "[renderer] Vulkan surface creation is not implemented for this "
               "platform yet\n");
  return false;
#endif
}
