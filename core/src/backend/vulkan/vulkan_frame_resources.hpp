#pragma once

#include "vulkan_context.hpp"

#include <vulkan/vulkan.h>

#include <cstdint>
#include <functional>
#include <vector>

namespace engine::vulkan {

class VulkanFrameResources {
public:
  bool create_frame_resources(const VulkanContext &context,
                              VkCommandPool command_pool, uint32_t width,
                              uint32_t height,
                              const std::function<bool()> &build_pipeline,
                              const std::function<void()> &destroy_pipeline);
  bool frame_resources_ready() const;
  void recreate_frame_resources(const VulkanContext &context,
                                VkCommandPool command_pool, uint32_t width,
                                uint32_t height,
                                const std::function<bool()> &build_pipeline,
                                const std::function<void()> &destroy_pipeline);
  void destroy_swapchain(const VulkanContext &context,
                         VkCommandPool command_pool);

  VkSwapchainKHR swapchain() const { return swapchain_; }
  VkFormat format() const { return swapchain_format_; }
  VkExtent2D extent() const { return swapchain_extent_; }
  const std::vector<VkImage> &images() const { return swapchain_images_; }
  const std::vector<VkImageView> &image_views() const {
    return swapchain_image_views_;
  }
  const std::vector<VkCommandBuffer> &command_buffers() const {
    return command_buffers_;
  }

private:
  bool create_swapchain(const VulkanContext &context, uint32_t width,
                        uint32_t height);
  bool create_image_views(const VulkanContext &context);
  bool create_command_buffers(const VulkanContext &context,
                              VkCommandPool command_pool);

  VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;
  VkFormat swapchain_format_ = VK_FORMAT_UNDEFINED;
  VkExtent2D swapchain_extent_{};
  std::vector<VkImage> swapchain_images_{};
  std::vector<VkImageView> swapchain_image_views_{};
  std::vector<VkCommandBuffer> command_buffers_{};
};

} // namespace engine::vulkan
