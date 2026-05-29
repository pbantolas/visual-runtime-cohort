#include "vulkan_utils.hpp"

#include <cstdio>
#include <cstring>
#include <fstream>

namespace visual_runtime::vulkan {

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

} // namespace visual_runtime::vulkan
