#include "renderer.h"

#include <vulkan/vulkan.h>
#if defined(__APPLE__)
#include <vulkan/vulkan_metal.h>
#endif

#include <cstring>
#include <cstdio>
#include <limits>
#include <vector>

namespace {

constexpr uint32_t invalid_queue_family =
    std::numeric_limits<uint32_t>::max();

struct QueueFamilies {
  uint32_t graphics = invalid_queue_family;
  uint32_t present = invalid_queue_family;
};

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

bool enumerate_instance_extensions(
    std::vector<VkExtensionProperties> &extensions) {
  uint32_t extension_count = 0;
  if (!check_vk(vkEnumerateInstanceExtensionProperties(
                    nullptr, &extension_count, nullptr),
                "failed to count Vulkan instance extensions")) {
    return false;
  }

  extensions.resize(extension_count);
  if (!check_vk(vkEnumerateInstanceExtensionProperties(
                    nullptr, &extension_count, extensions.data()),
                "failed to enumerate Vulkan instance extensions")) {
    return false;
  }

  return true;
}

bool required_extensions_available(
    const std::vector<VkExtensionProperties> &available,
    const char *const *required, uint32_t required_count,
    const char *extension_kind) {
  for (uint32_t i = 0; i < required_count; ++i) {
    if (!extension_available(available, required[i])) {
      std::fprintf(stderr, "[renderer] missing required Vulkan %s extension: %s\n",
                   extension_kind, required[i]);
      return false;
    }
  }

  return true;
}

bool enumerate_device_extensions(
    VkPhysicalDevice physical_device,
    std::vector<VkExtensionProperties> &extensions) {
  uint32_t extension_count = 0;
  if (!check_vk(vkEnumerateDeviceExtensionProperties(
                    physical_device, nullptr, &extension_count, nullptr),
                "failed to count Vulkan device extensions")) {
    return false;
  }

  extensions.resize(extension_count);
  if (!check_vk(vkEnumerateDeviceExtensionProperties(
                    physical_device, nullptr, &extension_count,
                    extensions.data()),
                "failed to enumerate Vulkan device extensions")) {
    return false;
  }

  return true;
}

QueueFamilies find_queue_families(VkPhysicalDevice physical_device,
                                  VkSurfaceKHR surface) {
  QueueFamilies families{};

  uint32_t family_count = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &family_count,
                                           nullptr);

  std::vector<VkQueueFamilyProperties> family_properties(family_count);
  vkGetPhysicalDeviceQueueFamilyProperties(
      physical_device, &family_count, family_properties.data());

  for (uint32_t i = 0; i < family_count; ++i) {
    const auto &family = family_properties[i];
    if (family.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
      families.graphics = i;
    }

    VkBool32 present_supported = VK_FALSE;
    if (check_vk(vkGetPhysicalDeviceSurfaceSupportKHR(
                     physical_device, i, surface, &present_supported),
                 "failed to query Vulkan present support") &&
        present_supported) {
      families.present = i;
    }

    if (families.graphics != invalid_queue_family &&
        families.present != invalid_queue_family) {
      break;
    }
  }

  return families;
}

bool queue_families_complete(const QueueFamilies &families) {
  return families.graphics != invalid_queue_family &&
         families.present != invalid_queue_family;
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
  bool pick_physical_device();
  bool physical_device_suitable(VkPhysicalDevice physical_device,
                                QueueFamilies &families,
                                std::vector<const char *> &device_extensions);
  bool create_device();

  VkInstance instance_ = VK_NULL_HANDLE;
  VkSurfaceKHR surface_ = VK_NULL_HANDLE;
  VkPhysicalDevice physical_device_ = VK_NULL_HANDLE;
  VkDevice device_ = VK_NULL_HANDLE;
  VkQueue graphics_queue_ = VK_NULL_HANDLE;
  VkQueue present_queue_ = VK_NULL_HANDLE;
  QueueFamilies queue_families_{};
  std::vector<const char *> device_extensions_{};
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

  if (!create_instance() || !create_surface(surface->native_handle) ||
      !pick_physical_device() || !create_device()) {
    shutdown();
    return false;
  }

  resize(surface->width, surface->height);

  std::fprintf(stderr, "[renderer] Vulkan device and surface initialized\n");
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
  if (device_ != VK_NULL_HANDLE) {
    vkDeviceWaitIdle(device_);
    vkDestroyDevice(device_, nullptr);
    device_ = VK_NULL_HANDLE;
  }
  graphics_queue_ = VK_NULL_HANDLE;
  present_queue_ = VK_NULL_HANDLE;
  physical_device_ = VK_NULL_HANDLE;
  queue_families_ = {};
  device_extensions_.clear();
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
  std::vector<VkExtensionProperties> available_extensions;
  if (!enumerate_instance_extensions(available_extensions)) {
    return false;
  }

  std::vector<const char *> extensions{VK_KHR_SURFACE_EXTENSION_NAME};
#if defined(__APPLE__)
  extensions.push_back(VK_EXT_METAL_SURFACE_EXTENSION_NAME);
#if defined(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME)
  bool enable_portability_enumeration =
      extension_available(available_extensions,
                          VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
  if (enable_portability_enumeration) {
    extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
  }
#else
  bool enable_portability_enumeration = false;
#endif
#else
  bool enable_portability_enumeration = false;
#endif

  if (!required_extensions_available(available_extensions, extensions.data(),
                                     static_cast<uint32_t>(extensions.size()),
                                     "instance")) {
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
  if (enable_portability_enumeration) {
    create_info.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
  }

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

bool RendererBackend::pick_physical_device() {
  uint32_t device_count = 0;
  if (!check_vk(vkEnumeratePhysicalDevices(instance_, &device_count, nullptr),
                "failed to count Vulkan physical devices")) {
    return false;
  }
  if (device_count == 0) {
    std::fprintf(stderr, "[renderer] no Vulkan physical devices found\n");
    return false;
  }

  std::vector<VkPhysicalDevice> devices(device_count);
  if (!check_vk(vkEnumeratePhysicalDevices(instance_, &device_count,
                                           devices.data()),
                "failed to enumerate Vulkan physical devices")) {
    return false;
  }

  for (VkPhysicalDevice device : devices) {
    QueueFamilies families{};
    std::vector<const char *> extensions;
    if (physical_device_suitable(device, families, extensions)) {
      physical_device_ = device;
      queue_families_ = families;
      device_extensions_ = std::move(extensions);

      VkPhysicalDeviceProperties properties{};
      vkGetPhysicalDeviceProperties(physical_device_, &properties);
      std::fprintf(stderr, "[renderer] selected Vulkan device: %s\n",
                   properties.deviceName);
      return true;
    }
  }

  std::fprintf(stderr, "[renderer] no suitable Vulkan physical device found\n");
  return false;
}

bool RendererBackend::physical_device_suitable(
    VkPhysicalDevice physical_device, QueueFamilies &families,
    std::vector<const char *> &device_extensions) {
  VkPhysicalDeviceProperties properties{};
  vkGetPhysicalDeviceProperties(physical_device, &properties);
  if (properties.apiVersion < VK_API_VERSION_1_3) {
    std::fprintf(stderr,
                 "[renderer] rejecting Vulkan device %s: Vulkan 1.3 is "
                 "required\n",
                 properties.deviceName);
    return false;
  }

  VkPhysicalDeviceVulkan13Features vulkan13_features{};
  vulkan13_features.sType =
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;

  VkPhysicalDeviceFeatures2 features{};
  features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
  features.pNext = &vulkan13_features;
  vkGetPhysicalDeviceFeatures2(physical_device, &features);
  if (!vulkan13_features.dynamicRendering ||
      !vulkan13_features.synchronization2) {
    std::fprintf(stderr,
                 "[renderer] rejecting Vulkan device %s: dynamic rendering "
                 "and synchronization2 are required\n",
                 properties.deviceName);
    return false;
  }

  std::vector<VkExtensionProperties> available_extensions;
  if (!enumerate_device_extensions(physical_device, available_extensions)) {
    return false;
  }

  device_extensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
#if defined(VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME)
  if (extension_available(available_extensions,
                          VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME)) {
    device_extensions.push_back(VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME);
  }
#endif

  if (!required_extensions_available(
          available_extensions, device_extensions.data(),
          static_cast<uint32_t>(device_extensions.size()), "device")) {
    std::fprintf(stderr,
                 "[renderer] rejecting Vulkan device %s: required device "
                 "extensions are missing\n",
                 properties.deviceName);
    return false;
  }

  families = find_queue_families(physical_device, surface_);
  if (!queue_families_complete(families)) {
    std::fprintf(stderr,
                 "[renderer] rejecting Vulkan device %s: graphics and present "
                 "queues are required\n",
                 properties.deviceName);
    return false;
  }

  return true;
}

bool RendererBackend::create_device() {
  std::vector<VkDeviceQueueCreateInfo> queue_infos;
  std::vector<uint32_t> unique_queue_families{queue_families_.graphics};
  if (queue_families_.present != queue_families_.graphics) {
    unique_queue_families.push_back(queue_families_.present);
  }

  float queue_priority = 1.0f;
  for (uint32_t queue_family : unique_queue_families) {
    VkDeviceQueueCreateInfo queue_info{};
    queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_info.queueFamilyIndex = queue_family;
    queue_info.queueCount = 1;
    queue_info.pQueuePriorities = &queue_priority;
    queue_infos.push_back(queue_info);
  }

  VkPhysicalDeviceVulkan13Features vulkan13_features{};
  vulkan13_features.sType =
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
  vulkan13_features.dynamicRendering = VK_TRUE;
  vulkan13_features.synchronization2 = VK_TRUE;

  VkPhysicalDeviceFeatures2 features{};
  features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
  features.pNext = &vulkan13_features;

  VkDeviceCreateInfo create_info{};
  create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  create_info.pNext = &features;
  create_info.queueCreateInfoCount = static_cast<uint32_t>(queue_infos.size());
  create_info.pQueueCreateInfos = queue_infos.data();
  create_info.enabledExtensionCount =
      static_cast<uint32_t>(device_extensions_.size());
  create_info.ppEnabledExtensionNames = device_extensions_.data();

  if (!check_vk(vkCreateDevice(physical_device_, &create_info, nullptr,
                               &device_),
                "failed to create Vulkan device")) {
    return false;
  }

  vkGetDeviceQueue(device_, queue_families_.graphics, 0, &graphics_queue_);
  vkGetDeviceQueue(device_, queue_families_.present, 0, &present_queue_);
  return true;
}
