#include "vulkan_swapchain.hpp"

#include "vulkan_utils.hpp"

#include <algorithm>
#include <limits>

namespace visual_runtime::vulkan {

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

} // namespace visual_runtime::vulkan
