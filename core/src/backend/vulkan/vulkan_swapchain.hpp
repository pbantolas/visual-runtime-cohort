#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>
#include <vector>

namespace visual_runtime::vulkan {

struct SwapchainSupport {
  VkSurfaceCapabilitiesKHR capabilities{};
  std::vector<VkSurfaceFormatKHR> formats;
  std::vector<VkPresentModeKHR> present_modes;
};

bool query_swapchain_support(VkPhysicalDevice physical_device,
                             VkSurfaceKHR surface, SwapchainSupport &support);
VkSurfaceFormatKHR
choose_surface_format(const std::vector<VkSurfaceFormatKHR> &formats);
VkPresentModeKHR
choose_present_mode(const std::vector<VkPresentModeKHR> &present_modes);
VkExtent2D choose_swapchain_extent(const VkSurfaceCapabilitiesKHR &capabilities,
                                   uint32_t width, uint32_t height);

} // namespace visual_runtime::vulkan
