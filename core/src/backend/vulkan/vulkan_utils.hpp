#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>
#include <limits>
#include <vector>

namespace engine::vulkan {

constexpr uint32_t invalid_queue_family = std::numeric_limits<uint32_t>::max();

const char *vk_result_name(VkResult result);
void print_vk_error(const char *context, VkResult result);
bool check_vk(VkResult result, const char *context);

bool extension_available(const std::vector<VkExtensionProperties> &extensions,
                         const char *name);
bool enumerate_instance_extensions(
    std::vector<VkExtensionProperties> &extensions);
bool enumerate_device_extensions(
    VkPhysicalDevice physical_device,
    std::vector<VkExtensionProperties> &extensions);
bool required_extensions_available(
    const std::vector<VkExtensionProperties> &available,
    const char *const *required, uint32_t required_count,
    const char *extension_kind);

uint32_t find_memory_type(VkPhysicalDevice physical_device,
                          uint32_t type_filter,
                          VkMemoryPropertyFlags properties);
bool read_binary_file(const char *path, std::vector<char> &bytes);

} // namespace engine::vulkan
