#include "vulkan_frame_resources.hpp"

#include "vulkan_swapchain.hpp"
#include "vulkan_utils.hpp"

#include <array>
#include <cstddef>
#include <cstdio>
#include <utility>

namespace visual_runtime::vulkan {

bool VulkanFrameResources::create_frame_resources(
    const VulkanContext &context, VkCommandPool command_pool, uint32_t width,
    uint32_t height, const std::function<bool()> &build_pipeline,
    const std::function<void()> &destroy_pipeline) {
  if (create_swapchain(context, width, height) && create_image_views(context) &&
      create_command_buffers(context, command_pool) && build_pipeline()) {
    return true;
  }

  destroy_pipeline();
  destroy_swapchain(context, command_pool);
  return false;
}

bool VulkanFrameResources::frame_resources_ready() const {
  return swapchain_ != VK_NULL_HANDLE && !swapchain_images_.empty() &&
         swapchain_image_views_.size() == swapchain_images_.size() &&
         command_buffers_.size() == swapchain_images_.size();
}

void VulkanFrameResources::recreate_frame_resources(
    const VulkanContext &context, VkCommandPool command_pool, uint32_t width,
    uint32_t height, const std::function<bool()> &build_pipeline,
    const std::function<void()> &destroy_pipeline) {
  if (width == 0 || height == 0) {
    return;
  }

  if (context.device() != VK_NULL_HANDLE) {
    vkDeviceWaitIdle(context.device());
  }
  destroy_pipeline();
  destroy_swapchain(context, command_pool);

  if (create_frame_resources(context, command_pool, width, height,
                             build_pipeline, destroy_pipeline)) {
    return;
  }

  std::fprintf(stderr,
               "[renderer] failed to recreate Vulkan frame resources\n");
}

void VulkanFrameResources::destroy_swapchain(const VulkanContext &context,
                                             VkCommandPool command_pool) {
  const VkDevice device = context.device();

  if (device != VK_NULL_HANDLE && command_pool != VK_NULL_HANDLE &&
      !command_buffers_.empty()) {
    vkFreeCommandBuffers(device, command_pool,
                         static_cast<uint32_t>(command_buffers_.size()),
                         command_buffers_.data());
  }
  command_buffers_.clear();

  if (device != VK_NULL_HANDLE) {
    for (VkImageView image_view : swapchain_image_views_) {
      vkDestroyImageView(device, image_view, nullptr);
    }
  }
  swapchain_image_views_.clear();
  swapchain_images_.clear();
  if (device != VK_NULL_HANDLE && swapchain_ != VK_NULL_HANDLE) {
    vkDestroySwapchainKHR(device, swapchain_, nullptr);
  }
  swapchain_ = VK_NULL_HANDLE;
  swapchain_format_ = VK_FORMAT_UNDEFINED;
  swapchain_extent_ = {};
}

bool VulkanFrameResources::create_swapchain(const VulkanContext &context,
                                            uint32_t width, uint32_t height) {
  SwapchainSupport support{};
  if (!query_swapchain_support(context.physical_device(), context.surface(),
                               support) ||
      support.formats.empty() || support.present_modes.empty()) {
    return false;
  }

  VkSurfaceFormatKHR surface_format = choose_surface_format(support.formats);
  VkPresentModeKHR present_mode = choose_present_mode(support.present_modes);
  VkExtent2D extent =
      choose_swapchain_extent(support.capabilities, width, height);

  uint32_t image_count = support.capabilities.minImageCount + 1;
  if (support.capabilities.maxImageCount > 0 &&
      image_count > support.capabilities.maxImageCount) {
    image_count = support.capabilities.maxImageCount;
  }

  VkSwapchainCreateInfoKHR create_info{};
  create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
  create_info.surface = context.surface();
  create_info.minImageCount = image_count;
  create_info.imageFormat = surface_format.format;
  create_info.imageColorSpace = surface_format.colorSpace;
  create_info.imageExtent = extent;
  create_info.imageArrayLayers = 1;
  create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

  std::array<uint32_t, 2> queue_family_indices{
      context.queue_families().graphics,
      context.queue_families().present,
  };
  if (context.queue_families().graphics != context.queue_families().present) {
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

  if (!check_vk(vkCreateSwapchainKHR(context.device(), &create_info, nullptr,
                                     &swapchain_),
                "failed to create Vulkan swapchain")) {
    return false;
  }

  if (!check_vk(vkGetSwapchainImagesKHR(context.device(), swapchain_,
                                        &image_count, nullptr),
                "failed to count Vulkan swapchain images")) {
    return false;
  }
  swapchain_images_.resize(image_count);
  if (!check_vk(vkGetSwapchainImagesKHR(context.device(), swapchain_,
                                        &image_count, swapchain_images_.data()),
                "failed to get Vulkan swapchain images")) {
    return false;
  }

  swapchain_format_ = surface_format.format;
  swapchain_extent_ = extent;
  return true;
}

bool VulkanFrameResources::create_image_views(const VulkanContext &context) {
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

    if (!check_vk(vkCreateImageView(context.device(), &create_info, nullptr,
                                    &swapchain_image_views_[i]),
                  "failed to create Vulkan swapchain image view")) {
      return false;
    }
  }

  return true;
}

bool VulkanFrameResources::create_command_buffers(const VulkanContext &context,
                                                  VkCommandPool command_pool) {
  std::vector<VkCommandBuffer> command_buffers(swapchain_images_.size());

  VkCommandBufferAllocateInfo allocate_info{};
  allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocate_info.commandPool = command_pool;
  allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocate_info.commandBufferCount =
      static_cast<uint32_t>(command_buffers.size());

  if (!check_vk(vkAllocateCommandBuffers(context.device(), &allocate_info,
                                         command_buffers.data()),
                "failed to allocate Vulkan command buffers")) {
    return false;
  }

  command_buffers_ = std::move(command_buffers);
  return true;
}

} // namespace visual_runtime::vulkan
