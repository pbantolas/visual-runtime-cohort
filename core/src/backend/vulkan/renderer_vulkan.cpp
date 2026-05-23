#include "renderer.h"

#include <glm/mat4x4.hpp>

#include <vulkan/vulkan.h>
#if defined(__APPLE__)
#include <vulkan/vulkan_metal.h>
#endif

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <limits>
#include <vector>

namespace {

constexpr uint32_t invalid_queue_family = std::numeric_limits<uint32_t>::max();

struct QueueFamilies {
  uint32_t graphics = invalid_queue_family;
  uint32_t present = invalid_queue_family;
};

struct SwapchainSupport {
  VkSurfaceCapabilitiesKHR capabilities{};
  std::vector<VkSurfaceFormatKHR> formats;
  std::vector<VkPresentModeKHR> present_modes;
};

struct Vertex {
  float position[2];
  float color[3];
};

struct FrameUniforms {
  glm::mat4 matrix{1.0f};
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
      std::fprintf(stderr,
                   "[renderer] missing required Vulkan %s extension: %s\n",
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
  if (!check_vk(vkEnumerateDeviceExtensionProperties(physical_device, nullptr,
                                                     &extension_count, nullptr),
                "failed to count Vulkan device extensions")) {
    return false;
  }

  extensions.resize(extension_count);
  if (!check_vk(vkEnumerateDeviceExtensionProperties(physical_device, nullptr,
                                                     &extension_count,
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
  vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &family_count,
                                           family_properties.data());

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

bool query_swapchain_support(VkPhysicalDevice physical_device,
                             VkSurfaceKHR surface, SwapchainSupport &support) {
  if (!check_vk(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
                    physical_device, surface, &support.capabilities),
                "failed to query Vulkan surface capabilities")) {
    return false;
  }

  uint32_t format_count = 0;
  if (!check_vk(vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface,
                                                     &format_count, nullptr),
                "failed to count Vulkan surface formats")) {
    return false;
  }
  support.formats.resize(format_count);
  if (format_count > 0 && !check_vk(vkGetPhysicalDeviceSurfaceFormatsKHR(
                                        physical_device, surface, &format_count,
                                        support.formats.data()),
                                    "failed to query Vulkan surface formats")) {
    return false;
  }

  uint32_t present_mode_count = 0;
  if (!check_vk(vkGetPhysicalDeviceSurfacePresentModesKHR(
                    physical_device, surface, &present_mode_count, nullptr),
                "failed to count Vulkan present modes")) {
    return false;
  }
  support.present_modes.resize(present_mode_count);
  if (present_mode_count > 0 &&
      !check_vk(vkGetPhysicalDeviceSurfacePresentModesKHR(
                    physical_device, surface, &present_mode_count,
                    support.present_modes.data()),
                "failed to query Vulkan present modes")) {
    return false;
  }

  return true;
}

VkSurfaceFormatKHR
choose_surface_format(const std::vector<VkSurfaceFormatKHR> &formats) {
  for (const auto &format : formats) {
    if (format.format == VK_FORMAT_B8G8R8A8_UNORM &&
        format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
      return format;
    }
  }
  return formats.front();
}

VkPresentModeKHR
choose_present_mode(const std::vector<VkPresentModeKHR> &present_modes) {
  for (VkPresentModeKHR mode : present_modes) {
    if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
      return mode;
    }
  }
  return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D choose_swapchain_extent(const VkSurfaceCapabilitiesKHR &capabilities,
                                   uint32_t width, uint32_t height) {
  if (capabilities.currentExtent.width !=
      std::numeric_limits<uint32_t>::max()) {
    return capabilities.currentExtent;
  }

  VkExtent2D extent{width, height};
  extent.width = std::clamp(extent.width, capabilities.minImageExtent.width,
                            capabilities.maxImageExtent.width);
  extent.height = std::clamp(extent.height, capabilities.minImageExtent.height,
                             capabilities.maxImageExtent.height);
  return extent;
}

uint32_t find_memory_type(VkPhysicalDevice physical_device,
                          uint32_t type_filter,
                          VkMemoryPropertyFlags properties) {
  VkPhysicalDeviceMemoryProperties memory_properties{};
  vkGetPhysicalDeviceMemoryProperties(physical_device, &memory_properties);

  for (uint32_t i = 0; i < memory_properties.memoryTypeCount; ++i) {
    const bool type_matches = (type_filter & (1 << i)) != 0;
    const bool properties_match =
        (memory_properties.memoryTypes[i].propertyFlags & properties) ==
        properties;
    if (type_matches && properties_match) {
      return i;
    }
  }

  return invalid_queue_family;
}

bool read_binary_file(const char *path, std::vector<char> &bytes) {
  std::ifstream file(path, std::ios::ate | std::ios::binary);
  if (!file.is_open()) {
    std::fprintf(stderr, "[renderer] failed to open shader %s\n", path);
    return false;
  }

  const std::streamsize size = file.tellg();
  if (size <= 0) {
    std::fprintf(stderr, "[renderer] shader %s is empty\n", path);
    return false;
  }

  bytes.resize(static_cast<size_t>(size));
  file.seekg(0);
  if (!file.read(bytes.data(), size)) {
    std::fprintf(stderr, "[renderer] failed to read shader %s\n", path);
    return false;
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
  bool pick_physical_device();
  bool physical_device_suitable(VkPhysicalDevice physical_device,
                                QueueFamilies &families,
                                std::vector<const char *> &device_extensions);
  bool create_device();
  bool create_swapchain();
  bool create_image_views();
  bool create_command_pool();
  bool create_command_buffers();
  bool create_sync_objects();
  bool create_frame_resources();
  bool build_geometry();
  bool build_uniforms();
  bool create_descriptor_layout();
  bool create_descriptor_pool();
  bool create_descriptor_set();
  bool build_pipeline();
  bool create_shader_module(const char *path, VkShaderModule &shader_module);
  bool create_buffer(VkDeviceSize size, VkBufferUsageFlags usage,
                     VkMemoryPropertyFlags properties, VkBuffer &buffer,
                     VkDeviceMemory &memory);
  bool record_clear_commands(uint32_t image_index);
  void update_frame_uniforms();
  void destroy_pipeline();
  void destroy_swapchain();
  void destroy_buffer(VkBuffer &buffer, VkDeviceMemory &memory);

  VkInstance instance_ = VK_NULL_HANDLE;
  VkSurfaceKHR surface_ = VK_NULL_HANDLE;
  VkPhysicalDevice physical_device_ = VK_NULL_HANDLE;
  VkDevice device_ = VK_NULL_HANDLE;
  VkQueue graphics_queue_ = VK_NULL_HANDLE;
  VkQueue present_queue_ = VK_NULL_HANDLE;
  VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;
  VkFormat swapchain_format_ = VK_FORMAT_UNDEFINED;
  VkExtent2D swapchain_extent_{};
  std::vector<VkImage> swapchain_images_{};
  std::vector<VkImageView> swapchain_image_views_{};
  VkCommandPool command_pool_ = VK_NULL_HANDLE;
  std::vector<VkCommandBuffer> command_buffers_{};
  VkSemaphore image_available_ = VK_NULL_HANDLE;
  VkSemaphore render_finished_ = VK_NULL_HANDLE;
  VkFence frame_in_flight_ = VK_NULL_HANDLE;
  VkBuffer vertex_buffer_ = VK_NULL_HANDLE;
  VkDeviceMemory vertex_buffer_memory_ = VK_NULL_HANDLE;
  uint32_t vertex_count_ = 0;
  VkBuffer frame_uniform_buffer_ = VK_NULL_HANDLE;
  VkDeviceMemory frame_uniform_buffer_memory_ = VK_NULL_HANDLE;
  VkDescriptorSetLayout descriptor_set_layout_ = VK_NULL_HANDLE;
  VkDescriptorPool descriptor_pool_ = VK_NULL_HANDLE;
  VkDescriptorSet descriptor_set_ = VK_NULL_HANDLE;
  VkPipelineLayout pipeline_layout_ = VK_NULL_HANDLE;
  VkPipeline pipeline_ = VK_NULL_HANDLE;
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
      !pick_physical_device() || !create_device() || !create_command_pool() ||
      !create_sync_objects() || !create_descriptor_layout() ||
      !build_geometry() || !build_uniforms() || !create_descriptor_pool() ||
      !create_descriptor_set()) {
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
  update_frame_uniforms();

  if (device_ == VK_NULL_HANDLE || width == 0 || height == 0) {
    return;
  }

  if (swapchain_ == VK_NULL_HANDLE || swapchain_extent_.width != width ||
      swapchain_extent_.height != height) {
    vkDeviceWaitIdle(device_);
    destroy_pipeline();
    destroy_swapchain();
    if (!create_frame_resources()) {
      std::fprintf(stderr,
                   "[renderer] failed to recreate Vulkan frame resources\n");
    }
  }
}

void RendererBackend::render_frame(float t) {
  (void)t;
  if (render_width_ == 0 || render_height_ == 0 ||
      swapchain_ == VK_NULL_HANDLE || command_buffers_.empty() ||
      pipeline_ == VK_NULL_HANDLE) {
    return;
  }

  if (!check_vk(
          vkWaitForFences(device_, 1, &frame_in_flight_, VK_TRUE, UINT64_MAX),
          "failed to wait for Vulkan frame fence")) {
    return;
  }

  uint32_t image_index = 0;
  VkResult acquire_result =
      vkAcquireNextImageKHR(device_, swapchain_, UINT64_MAX, image_available_,
                            VK_NULL_HANDLE, &image_index);
  if (acquire_result != VK_SUCCESS && acquire_result != VK_SUBOPTIMAL_KHR) {
    print_vk_error("failed to acquire Vulkan swapchain image", acquire_result);
    return;
  }

  if (!record_clear_commands(image_index)) {
    return;
  }
  if (!check_vk(vkResetFences(device_, 1, &frame_in_flight_),
                "failed to reset Vulkan frame fence")) {
    return;
  }

  VkPipelineStageFlags wait_stage =
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  VkSubmitInfo submit_info{};
  submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submit_info.waitSemaphoreCount = 1;
  submit_info.pWaitSemaphores = &image_available_;
  submit_info.pWaitDstStageMask = &wait_stage;
  submit_info.commandBufferCount = 1;
  submit_info.pCommandBuffers = &command_buffers_[image_index];
  submit_info.signalSemaphoreCount = 1;
  submit_info.pSignalSemaphores = &render_finished_;

  if (!check_vk(
          vkQueueSubmit(graphics_queue_, 1, &submit_info, frame_in_flight_),
          "failed to submit Vulkan clear commands")) {
    return;
  }

  VkPresentInfoKHR present_info{};
  present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  present_info.waitSemaphoreCount = 1;
  present_info.pWaitSemaphores = &render_finished_;
  present_info.swapchainCount = 1;
  present_info.pSwapchains = &swapchain_;
  present_info.pImageIndices = &image_index;

  VkResult present_result = vkQueuePresentKHR(present_queue_, &present_info);
  if (present_result != VK_SUCCESS && present_result != VK_SUBOPTIMAL_KHR) {
    print_vk_error("failed to present Vulkan swapchain image", present_result);
  }
}

void RendererBackend::shutdown() {
  if (device_ != VK_NULL_HANDLE) {
    vkDeviceWaitIdle(device_);
    destroy_pipeline();
    if (descriptor_pool_ != VK_NULL_HANDLE) {
      vkDestroyDescriptorPool(device_, descriptor_pool_, nullptr);
      descriptor_pool_ = VK_NULL_HANDLE;
      descriptor_set_ = VK_NULL_HANDLE;
    }
    if (descriptor_set_layout_ != VK_NULL_HANDLE) {
      vkDestroyDescriptorSetLayout(device_, descriptor_set_layout_, nullptr);
      descriptor_set_layout_ = VK_NULL_HANDLE;
    }
    destroy_buffer(frame_uniform_buffer_, frame_uniform_buffer_memory_);
    destroy_buffer(vertex_buffer_, vertex_buffer_memory_);
    vertex_count_ = 0;
    destroy_swapchain();
    if (frame_in_flight_ != VK_NULL_HANDLE) {
      vkDestroyFence(device_, frame_in_flight_, nullptr);
      frame_in_flight_ = VK_NULL_HANDLE;
    }
    if (render_finished_ != VK_NULL_HANDLE) {
      vkDestroySemaphore(device_, render_finished_, nullptr);
      render_finished_ = VK_NULL_HANDLE;
    }
    if (image_available_ != VK_NULL_HANDLE) {
      vkDestroySemaphore(device_, image_available_, nullptr);
      image_available_ = VK_NULL_HANDLE;
    }
    if (command_pool_ != VK_NULL_HANDLE) {
      vkDestroyCommandPool(device_, command_pool_, nullptr);
      command_pool_ = VK_NULL_HANDLE;
    }
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
  bool enable_portability_enumeration = extension_available(
      available_extensions, VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
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
  auto create_metal_surface = reinterpret_cast<PFN_vkCreateMetalSurfaceEXT>(
      vkGetInstanceProcAddr(instance_, "vkCreateMetalSurfaceEXT"));
  if (!create_metal_surface) {
    std::fprintf(stderr, "[renderer] failed to load vkCreateMetalSurfaceEXT\n");
    return false;
  }

  VkMetalSurfaceCreateInfoEXT create_info{};
  create_info.sType = VK_STRUCTURE_TYPE_METAL_SURFACE_CREATE_INFO_EXT;
  create_info.pLayer = native_handle;

  if (!check_vk(
          create_metal_surface(instance_, &create_info, nullptr, &surface_),
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
  if (!check_vk(
          vkEnumeratePhysicalDevices(instance_, &device_count, devices.data()),
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

  SwapchainSupport swapchain_support{};
  if (!query_swapchain_support(physical_device, surface_, swapchain_support)) {
    return false;
  }
  if (swapchain_support.formats.empty() ||
      swapchain_support.present_modes.empty()) {
    std::fprintf(stderr,
                 "[renderer] rejecting Vulkan device %s: swapchain formats "
                 "and present modes are required\n",
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

  if (!check_vk(
          vkCreateDevice(physical_device_, &create_info, nullptr, &device_),
          "failed to create Vulkan device")) {
    return false;
  }

  vkGetDeviceQueue(device_, queue_families_.graphics, 0, &graphics_queue_);
  vkGetDeviceQueue(device_, queue_families_.present, 0, &present_queue_);
  return true;
}

bool RendererBackend::create_swapchain() {
  SwapchainSupport support{};
  if (!query_swapchain_support(physical_device_, surface_, support) ||
      support.formats.empty() || support.present_modes.empty()) {
    return false;
  }

  VkSurfaceFormatKHR surface_format = choose_surface_format(support.formats);
  VkPresentModeKHR present_mode = choose_present_mode(support.present_modes);
  VkExtent2D extent = choose_swapchain_extent(support.capabilities,
                                              render_width_, render_height_);

  uint32_t image_count = support.capabilities.minImageCount + 1;
  if (support.capabilities.maxImageCount > 0 &&
      image_count > support.capabilities.maxImageCount) {
    image_count = support.capabilities.maxImageCount;
  }

  VkSwapchainCreateInfoKHR create_info{};
  create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
  create_info.surface = surface_;
  create_info.minImageCount = image_count;
  create_info.imageFormat = surface_format.format;
  create_info.imageColorSpace = surface_format.colorSpace;
  create_info.imageExtent = extent;
  create_info.imageArrayLayers = 1;
  create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

  std::array<uint32_t, 2> queue_family_indices{
      queue_families_.graphics,
      queue_families_.present,
  };
  if (queue_families_.graphics != queue_families_.present) {
    create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
    create_info.queueFamilyIndexCount =
        static_cast<uint32_t>(queue_family_indices.size());
    create_info.pQueueFamilyIndices = queue_family_indices.data();
  } else {
    create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
  }

  create_info.preTransform = support.capabilities.currentTransform;
  create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  create_info.presentMode = present_mode;
  create_info.clipped = VK_TRUE;

  if (!check_vk(
          vkCreateSwapchainKHR(device_, &create_info, nullptr, &swapchain_),
          "failed to create Vulkan swapchain")) {
    return false;
  }

  if (!check_vk(
          vkGetSwapchainImagesKHR(device_, swapchain_, &image_count, nullptr),
          "failed to count Vulkan swapchain images")) {
    return false;
  }
  swapchain_images_.resize(image_count);
  if (!check_vk(vkGetSwapchainImagesKHR(device_, swapchain_, &image_count,
                                        swapchain_images_.data()),
                "failed to get Vulkan swapchain images")) {
    return false;
  }

  swapchain_format_ = surface_format.format;
  swapchain_extent_ = extent;
  return true;
}

bool RendererBackend::create_image_views() {
  swapchain_image_views_.resize(swapchain_images_.size());

  for (size_t i = 0; i < swapchain_images_.size(); ++i) {
    VkImageViewCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    create_info.image = swapchain_images_[i];
    create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    create_info.format = swapchain_format_;
    create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    create_info.subresourceRange.baseMipLevel = 0;
    create_info.subresourceRange.levelCount = 1;
    create_info.subresourceRange.baseArrayLayer = 0;
    create_info.subresourceRange.layerCount = 1;

    if (!check_vk(vkCreateImageView(device_, &create_info, nullptr,
                                    &swapchain_image_views_[i]),
                  "failed to create Vulkan swapchain image view")) {
      return false;
    }
  }

  return true;
}

bool RendererBackend::create_command_pool() {
  VkCommandPoolCreateInfo create_info{};
  create_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  create_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  create_info.queueFamilyIndex = queue_families_.graphics;

  return check_vk(
      vkCreateCommandPool(device_, &create_info, nullptr, &command_pool_),
      "failed to create Vulkan command pool");
}

bool RendererBackend::create_command_buffers() {
  command_buffers_.resize(swapchain_images_.size());

  VkCommandBufferAllocateInfo allocate_info{};
  allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocate_info.commandPool = command_pool_;
  allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocate_info.commandBufferCount =
      static_cast<uint32_t>(command_buffers_.size());

  return check_vk(vkAllocateCommandBuffers(device_, &allocate_info,
                                           command_buffers_.data()),
                  "failed to allocate Vulkan command buffers");
}

bool RendererBackend::create_sync_objects() {
  VkSemaphoreCreateInfo semaphore_info{};
  semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

  VkFenceCreateInfo fence_info{};
  fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

  return check_vk(vkCreateSemaphore(device_, &semaphore_info, nullptr,
                                    &image_available_),
                  "failed to create Vulkan image-available semaphore") &&
         check_vk(vkCreateSemaphore(device_, &semaphore_info, nullptr,
                                    &render_finished_),
                  "failed to create Vulkan render-finished semaphore") &&
         check_vk(
             vkCreateFence(device_, &fence_info, nullptr, &frame_in_flight_),
             "failed to create Vulkan frame fence");
}

bool RendererBackend::create_frame_resources() {
  return create_swapchain() && create_image_views() &&
         create_command_buffers() && build_pipeline();
}

bool RendererBackend::build_geometry() {
  static constexpr Vertex vertices[] = {
      {{0.0f, 0.65f}, {1.0f, 0.0f, 0.0f}},
      {{-0.7f, -0.55f}, {0.0f, 1.0f, 0.0f}},
      {{0.7f, -0.55f}, {0.0f, 0.0f, 1.0f}},
  };

  const VkDeviceSize buffer_size = sizeof(vertices);
  if (!create_buffer(buffer_size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                         VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     vertex_buffer_, vertex_buffer_memory_)) {
    return false;
  }

  void *data = nullptr;
  if (!check_vk(
          vkMapMemory(device_, vertex_buffer_memory_, 0, buffer_size, 0, &data),
          "failed to map Vulkan vertex buffer")) {
    return false;
  }
  std::memcpy(data, vertices, sizeof(vertices));
  vkUnmapMemory(device_, vertex_buffer_memory_);

  vertex_count_ = static_cast<uint32_t>(sizeof(vertices) / sizeof(vertices[0]));
  return true;
}

bool RendererBackend::build_uniforms() {
  if (!create_buffer(sizeof(FrameUniforms), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                         VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     frame_uniform_buffer_, frame_uniform_buffer_memory_)) {
    return false;
  }

  update_frame_uniforms();
  return true;
}

bool RendererBackend::create_descriptor_layout() {
  VkDescriptorSetLayoutBinding frame_uniform_binding{};
  frame_uniform_binding.binding = 0;
  frame_uniform_binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  frame_uniform_binding.descriptorCount = 1;
  frame_uniform_binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

  VkDescriptorSetLayoutCreateInfo create_info{};
  create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  create_info.bindingCount = 1;
  create_info.pBindings = &frame_uniform_binding;

  return check_vk(vkCreateDescriptorSetLayout(device_, &create_info, nullptr,
                                              &descriptor_set_layout_),
                  "failed to create Vulkan descriptor set layout");
}

bool RendererBackend::create_descriptor_pool() {
  VkDescriptorPoolSize pool_size{};
  pool_size.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  pool_size.descriptorCount = 1;

  VkDescriptorPoolCreateInfo create_info{};
  create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  create_info.poolSizeCount = 1;
  create_info.pPoolSizes = &pool_size;
  create_info.maxSets = 1;

  return check_vk(
      vkCreateDescriptorPool(device_, &create_info, nullptr, &descriptor_pool_),
      "failed to create Vulkan descriptor pool");
}

bool RendererBackend::create_descriptor_set() {
  VkDescriptorSetAllocateInfo allocate_info{};
  allocate_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocate_info.descriptorPool = descriptor_pool_;
  allocate_info.descriptorSetCount = 1;
  allocate_info.pSetLayouts = &descriptor_set_layout_;

  if (!check_vk(
          vkAllocateDescriptorSets(device_, &allocate_info, &descriptor_set_),
          "failed to allocate Vulkan descriptor set")) {
    return false;
  }

  VkDescriptorBufferInfo buffer_info{};
  buffer_info.buffer = frame_uniform_buffer_;
  buffer_info.offset = 0;
  buffer_info.range = sizeof(FrameUniforms);

  VkWriteDescriptorSet descriptor_write{};
  descriptor_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  descriptor_write.dstSet = descriptor_set_;
  descriptor_write.dstBinding = 0;
  descriptor_write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  descriptor_write.descriptorCount = 1;
  descriptor_write.pBufferInfo = &buffer_info;

  vkUpdateDescriptorSets(device_, 1, &descriptor_write, 0, nullptr);
  return true;
}

bool RendererBackend::build_pipeline() {
  VkShaderModule vertex_shader = VK_NULL_HANDLE;
  VkShaderModule fragment_shader = VK_NULL_HANDLE;
  if (!create_shader_module(ENGINE_VULKAN_VERTEX_SPV_PATH, vertex_shader) ||
      !create_shader_module(ENGINE_VULKAN_FRAGMENT_SPV_PATH, fragment_shader)) {
    if (vertex_shader != VK_NULL_HANDLE) {
      vkDestroyShaderModule(device_, vertex_shader, nullptr);
    }
    if (fragment_shader != VK_NULL_HANDLE) {
      vkDestroyShaderModule(device_, fragment_shader, nullptr);
    }
    return false;
  }

  VkPipelineShaderStageCreateInfo vertex_stage{};
  vertex_stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  vertex_stage.stage = VK_SHADER_STAGE_VERTEX_BIT;
  vertex_stage.module = vertex_shader;
  vertex_stage.pName = "main";

  VkPipelineShaderStageCreateInfo fragment_stage{};
  fragment_stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  fragment_stage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
  fragment_stage.module = fragment_shader;
  fragment_stage.pName = "main";

  std::array<VkPipelineShaderStageCreateInfo, 2> shader_stages{
      vertex_stage,
      fragment_stage,
  };

  VkVertexInputBindingDescription vertex_binding{};
  vertex_binding.binding = 0;
  vertex_binding.stride = sizeof(Vertex);
  vertex_binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

  std::array<VkVertexInputAttributeDescription, 2> vertex_attributes{};
  vertex_attributes[0].location = 0;
  vertex_attributes[0].binding = 0;
  vertex_attributes[0].format = VK_FORMAT_R32G32_SFLOAT;
  vertex_attributes[0].offset = offsetof(Vertex, position);
  vertex_attributes[1].location = 1;
  vertex_attributes[1].binding = 0;
  vertex_attributes[1].format = VK_FORMAT_R32G32B32_SFLOAT;
  vertex_attributes[1].offset = offsetof(Vertex, color);

  VkPipelineVertexInputStateCreateInfo vertex_input{};
  vertex_input.sType =
      VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  vertex_input.vertexBindingDescriptionCount = 1;
  vertex_input.pVertexBindingDescriptions = &vertex_binding;
  vertex_input.vertexAttributeDescriptionCount =
      static_cast<uint32_t>(vertex_attributes.size());
  vertex_input.pVertexAttributeDescriptions = vertex_attributes.data();

  VkPipelineInputAssemblyStateCreateInfo input_assembly{};
  input_assembly.sType =
      VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

  VkPipelineViewportStateCreateInfo viewport_state{};
  viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewport_state.viewportCount = 1;
  viewport_state.scissorCount = 1;

  VkPipelineRasterizationStateCreateInfo rasterizer{};
  rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
  rasterizer.cullMode = VK_CULL_MODE_NONE;
  rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
  rasterizer.lineWidth = 1.0f;

  VkPipelineMultisampleStateCreateInfo multisampling{};
  multisampling.sType =
      VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

  VkPipelineColorBlendAttachmentState color_blend_attachment{};
  color_blend_attachment.colorWriteMask =
      VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
      VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

  VkPipelineColorBlendStateCreateInfo color_blending{};
  color_blending.sType =
      VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  color_blending.attachmentCount = 1;
  color_blending.pAttachments = &color_blend_attachment;

  std::array<VkDynamicState, 2> dynamic_states{
      VK_DYNAMIC_STATE_VIEWPORT,
      VK_DYNAMIC_STATE_SCISSOR,
  };

  VkPipelineDynamicStateCreateInfo dynamic_state{};
  dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
  dynamic_state.dynamicStateCount =
      static_cast<uint32_t>(dynamic_states.size());
  dynamic_state.pDynamicStates = dynamic_states.data();

  VkPipelineLayoutCreateInfo layout_info{};
  layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  layout_info.setLayoutCount = 1;
  layout_info.pSetLayouts = &descriptor_set_layout_;

  if (!check_vk(vkCreatePipelineLayout(device_, &layout_info, nullptr,
                                       &pipeline_layout_),
                "failed to create Vulkan pipeline layout")) {
    vkDestroyShaderModule(device_, vertex_shader, nullptr);
    vkDestroyShaderModule(device_, fragment_shader, nullptr);
    return false;
  }

  VkPipelineRenderingCreateInfo rendering_info{};
  rendering_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
  rendering_info.colorAttachmentCount = 1;
  rendering_info.pColorAttachmentFormats = &swapchain_format_;

  VkGraphicsPipelineCreateInfo pipeline_info{};
  pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  pipeline_info.pNext = &rendering_info;
  pipeline_info.stageCount = static_cast<uint32_t>(shader_stages.size());
  pipeline_info.pStages = shader_stages.data();
  pipeline_info.pVertexInputState = &vertex_input;
  pipeline_info.pInputAssemblyState = &input_assembly;
  pipeline_info.pViewportState = &viewport_state;
  pipeline_info.pRasterizationState = &rasterizer;
  pipeline_info.pMultisampleState = &multisampling;
  pipeline_info.pColorBlendState = &color_blending;
  pipeline_info.pDynamicState = &dynamic_state;
  pipeline_info.layout = pipeline_layout_;

  const bool created =
      check_vk(vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1,
                                         &pipeline_info, nullptr, &pipeline_),
               "failed to create Vulkan graphics pipeline");

  vkDestroyShaderModule(device_, vertex_shader, nullptr);
  vkDestroyShaderModule(device_, fragment_shader, nullptr);
  return created;
}

bool RendererBackend::create_shader_module(const char *path,
                                           VkShaderModule &shader_module) {
  std::vector<char> bytes;
  if (!read_binary_file(path, bytes)) {
    return false;
  }

  VkShaderModuleCreateInfo create_info{};
  create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  create_info.codeSize = bytes.size();
  create_info.pCode = reinterpret_cast<const uint32_t *>(bytes.data());

  return check_vk(
      vkCreateShaderModule(device_, &create_info, nullptr, &shader_module),
      "failed to create Vulkan shader module");
}

bool RendererBackend::create_buffer(VkDeviceSize size, VkBufferUsageFlags usage,
                                    VkMemoryPropertyFlags properties,
                                    VkBuffer &buffer, VkDeviceMemory &memory) {
  VkBufferCreateInfo buffer_info{};
  buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  buffer_info.size = size;
  buffer_info.usage = usage;
  buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  if (!check_vk(vkCreateBuffer(device_, &buffer_info, nullptr, &buffer),
                "failed to create Vulkan buffer")) {
    return false;
  }

  VkMemoryRequirements memory_requirements{};
  vkGetBufferMemoryRequirements(device_, buffer, &memory_requirements);

  const uint32_t memory_type = find_memory_type(
      physical_device_, memory_requirements.memoryTypeBits, properties);
  if (memory_type == invalid_queue_family) {
    std::fprintf(stderr,
                 "[renderer] failed to find suitable Vulkan buffer memory\n");
    return false;
  }

  VkMemoryAllocateInfo allocate_info{};
  allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  allocate_info.allocationSize = memory_requirements.size;
  allocate_info.memoryTypeIndex = memory_type;

  if (!check_vk(vkAllocateMemory(device_, &allocate_info, nullptr, &memory),
                "failed to allocate Vulkan buffer memory")) {
    return false;
  }

  return check_vk(vkBindBufferMemory(device_, buffer, memory, 0),
                  "failed to bind Vulkan buffer memory");
}

bool RendererBackend::record_clear_commands(uint32_t image_index) {
  VkCommandBuffer command_buffer = command_buffers_[image_index];

  if (!check_vk(vkResetCommandBuffer(command_buffer, 0),
                "failed to reset Vulkan command buffer")) {
    return false;
  }

  VkCommandBufferBeginInfo begin_info{};
  begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  if (!check_vk(vkBeginCommandBuffer(command_buffer, &begin_info),
                "failed to begin Vulkan command buffer")) {
    return false;
  }

  VkImageMemoryBarrier2 before_clear{};
  before_clear.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
  before_clear.srcStageMask = VK_PIPELINE_STAGE_2_NONE;
  before_clear.srcAccessMask = VK_ACCESS_2_NONE;
  before_clear.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
  before_clear.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
  before_clear.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  before_clear.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  before_clear.image = swapchain_images_[image_index];
  before_clear.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  before_clear.subresourceRange.baseMipLevel = 0;
  before_clear.subresourceRange.levelCount = 1;
  before_clear.subresourceRange.baseArrayLayer = 0;
  before_clear.subresourceRange.layerCount = 1;

  VkDependencyInfo before_dependency{};
  before_dependency.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
  before_dependency.imageMemoryBarrierCount = 1;
  before_dependency.pImageMemoryBarriers = &before_clear;
  vkCmdPipelineBarrier2(command_buffer, &before_dependency);

  VkRenderingAttachmentInfo color_attachment{};
  color_attachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
  color_attachment.imageView = swapchain_image_views_[image_index];
  color_attachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  color_attachment.clearValue.color = {{0.02f, 0.10f, 0.18f, 1.0f}};

  VkRenderingInfo rendering_info{};
  rendering_info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
  rendering_info.renderArea.offset = {0, 0};
  rendering_info.renderArea.extent = swapchain_extent_;
  rendering_info.layerCount = 1;
  rendering_info.colorAttachmentCount = 1;
  rendering_info.pColorAttachments = &color_attachment;

  vkCmdBeginRendering(command_buffer, &rendering_info);

  VkViewport viewport{};
  viewport.x = 0.0f;
  viewport.y = 0.0f;
  viewport.width = static_cast<float>(swapchain_extent_.width);
  viewport.height = static_cast<float>(swapchain_extent_.height);
  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;
  vkCmdSetViewport(command_buffer, 0, 1, &viewport);

  VkRect2D scissor{};
  scissor.offset = {0, 0};
  scissor.extent = swapchain_extent_;
  vkCmdSetScissor(command_buffer, 0, 1, &scissor);

  VkDeviceSize vertex_offset = 0;
  vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);
  vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                          pipeline_layout_, 0, 1, &descriptor_set_, 0, nullptr);
  vkCmdBindVertexBuffers(command_buffer, 0, 1, &vertex_buffer_, &vertex_offset);
  vkCmdDraw(command_buffer, vertex_count_, 1, 0, 0);

  vkCmdEndRendering(command_buffer);

  VkImageMemoryBarrier2 before_present{};
  before_present.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
  before_present.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
  before_present.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
  before_present.dstStageMask = VK_PIPELINE_STAGE_2_NONE;
  before_present.dstAccessMask = VK_ACCESS_2_NONE;
  before_present.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  before_present.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
  before_present.image = swapchain_images_[image_index];
  before_present.subresourceRange = before_clear.subresourceRange;

  VkDependencyInfo present_dependency{};
  present_dependency.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
  present_dependency.imageMemoryBarrierCount = 1;
  present_dependency.pImageMemoryBarriers = &before_present;
  vkCmdPipelineBarrier2(command_buffer, &present_dependency);

  return check_vk(vkEndCommandBuffer(command_buffer),
                  "failed to record Vulkan command buffer");
}

void RendererBackend::update_frame_uniforms() {
  if (frame_uniform_buffer_memory_ == VK_NULL_HANDLE) {
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
  uniforms.matrix[1][1] *= -1.0f;

  void *data = nullptr;
  if (!check_vk(vkMapMemory(device_, frame_uniform_buffer_memory_, 0,
                            sizeof(FrameUniforms), 0, &data),
                "failed to map Vulkan frame uniform buffer")) {
    return;
  }
  std::memcpy(data, &uniforms, sizeof(uniforms));
  vkUnmapMemory(device_, frame_uniform_buffer_memory_);
}

void RendererBackend::destroy_pipeline() {
  if (pipeline_ != VK_NULL_HANDLE) {
    vkDestroyPipeline(device_, pipeline_, nullptr);
    pipeline_ = VK_NULL_HANDLE;
  }
  if (pipeline_layout_ != VK_NULL_HANDLE) {
    vkDestroyPipelineLayout(device_, pipeline_layout_, nullptr);
    pipeline_layout_ = VK_NULL_HANDLE;
  }
}

void RendererBackend::destroy_swapchain() {
  if (command_pool_ != VK_NULL_HANDLE && !command_buffers_.empty()) {
    vkFreeCommandBuffers(device_, command_pool_,
                         static_cast<uint32_t>(command_buffers_.size()),
                         command_buffers_.data());
    command_buffers_.clear();
  }
  for (VkImageView image_view : swapchain_image_views_) {
    vkDestroyImageView(device_, image_view, nullptr);
  }
  swapchain_image_views_.clear();
  swapchain_images_.clear();
  if (swapchain_ != VK_NULL_HANDLE) {
    vkDestroySwapchainKHR(device_, swapchain_, nullptr);
    swapchain_ = VK_NULL_HANDLE;
  }
  swapchain_format_ = VK_FORMAT_UNDEFINED;
  swapchain_extent_ = {};
}

void RendererBackend::destroy_buffer(VkBuffer &buffer, VkDeviceMemory &memory) {
  if (buffer != VK_NULL_HANDLE) {
    vkDestroyBuffer(device_, buffer, nullptr);
    buffer = VK_NULL_HANDLE;
  }
  if (memory != VK_NULL_HANDLE) {
    vkFreeMemory(device_, memory, nullptr);
    memory = VK_NULL_HANDLE;
  }
}
