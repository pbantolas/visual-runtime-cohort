#include "renderer.h"
#include "vulkan_context.hpp"
#include "vulkan_swapchain.hpp"
#include "vulkan_utils.hpp"

#include <glm/mat4x4.hpp>

#include <vulkan/vulkan.h>

#include <array>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <vector>

namespace {

using engine::vulkan::check_vk;
using engine::vulkan::choose_present_mode;
using engine::vulkan::choose_surface_format;
using engine::vulkan::choose_swapchain_extent;
using engine::vulkan::find_memory_type;
using engine::vulkan::invalid_queue_family;
using engine::vulkan::print_vk_error;
using engine::vulkan::query_swapchain_support;
using engine::vulkan::read_binary_file;
using engine::vulkan::SwapchainSupport;
using engine::vulkan::VulkanContext;

struct Vertex {
  float position[2];
  float color[3];
};

struct FrameUniforms {
  glm::mat4 matrix{1.0f};
};

} // namespace

struct RendererBackend {
  bool init(SurfaceDescriptor *surface);
  void resize(uint32_t width, uint32_t height);
  void render_frame(float t);
  void shutdown();

private:
  bool create_swapchain();
  bool create_image_views();
  bool create_command_pool();
  bool create_command_buffers();
  bool create_sync_objects();
  bool create_frame_resources();
  bool frame_resources_ready() const;
  void recreate_frame_resources();
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

  VulkanContext context_;
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

  if (!context_.init(surface) || !create_command_pool() ||
      !create_sync_objects() || !create_descriptor_layout() ||
      !build_geometry() || !build_uniforms() || !create_descriptor_pool() ||
      !create_descriptor_set()) {
    shutdown();
    return false;
  }

  resize(surface->width, surface->height);

  return true;
}

void RendererBackend::resize(uint32_t width, uint32_t height) {
  render_width_ = width;
  render_height_ = height;
  update_frame_uniforms();

  if (context_.device() == VK_NULL_HANDLE || width == 0 || height == 0) {
    return;
  }

  if (!frame_resources_ready() || swapchain_extent_.width != width ||
      swapchain_extent_.height != height) {
    if (swapchain_ == VK_NULL_HANDLE) {
      create_frame_resources();
    } else {
      recreate_frame_resources();
    }
  }
}

void RendererBackend::render_frame(float t) {
  (void)t;
  if (render_width_ == 0 || render_height_ == 0 || !frame_resources_ready()) {
    return;
  }

  if (!check_vk(vkWaitForFences(context_.device(), 1, &frame_in_flight_,
                                VK_TRUE, UINT64_MAX),
                "failed to wait for Vulkan frame fence")) {
    return;
  }

  uint32_t image_index = 0;
  VkResult acquire_result =
      vkAcquireNextImageKHR(context_.device(), swapchain_, UINT64_MAX,
                            image_available_, VK_NULL_HANDLE, &image_index);
  if (acquire_result == VK_ERROR_OUT_OF_DATE_KHR) {
    recreate_frame_resources();
    return;
  }
  if (acquire_result != VK_SUCCESS && acquire_result != VK_SUBOPTIMAL_KHR) {
    print_vk_error("failed to acquire Vulkan swapchain image", acquire_result);
    return;
  }
  const bool should_recreate_after_present =
      acquire_result == VK_SUBOPTIMAL_KHR;

  if (!record_clear_commands(image_index)) {
    return;
  }
  if (!check_vk(vkResetFences(context_.device(), 1, &frame_in_flight_),
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

  if (!check_vk(vkQueueSubmit(context_.graphics_queue(), 1, &submit_info,
                              frame_in_flight_),
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

  VkResult present_result =
      vkQueuePresentKHR(context_.present_queue(), &present_info);
  if (present_result == VK_ERROR_OUT_OF_DATE_KHR ||
      present_result == VK_SUBOPTIMAL_KHR || should_recreate_after_present) {
    recreate_frame_resources();
  } else if (present_result != VK_SUCCESS) {
    print_vk_error("failed to present Vulkan swapchain image", present_result);
  }
}

void RendererBackend::shutdown() {
  if (context_.device() != VK_NULL_HANDLE) {
    vkDeviceWaitIdle(context_.device());
    destroy_pipeline();
    if (descriptor_pool_ != VK_NULL_HANDLE) {
      vkDestroyDescriptorPool(context_.device(), descriptor_pool_, nullptr);
      descriptor_pool_ = VK_NULL_HANDLE;
      descriptor_set_ = VK_NULL_HANDLE;
    }
    if (descriptor_set_layout_ != VK_NULL_HANDLE) {
      vkDestroyDescriptorSetLayout(context_.device(), descriptor_set_layout_,
                                   nullptr);
      descriptor_set_layout_ = VK_NULL_HANDLE;
    }
    destroy_buffer(frame_uniform_buffer_, frame_uniform_buffer_memory_);
    destroy_buffer(vertex_buffer_, vertex_buffer_memory_);
    vertex_count_ = 0;
    destroy_swapchain();
    if (frame_in_flight_ != VK_NULL_HANDLE) {
      vkDestroyFence(context_.device(), frame_in_flight_, nullptr);
      frame_in_flight_ = VK_NULL_HANDLE;
    }
    if (render_finished_ != VK_NULL_HANDLE) {
      vkDestroySemaphore(context_.device(), render_finished_, nullptr);
      render_finished_ = VK_NULL_HANDLE;
    }
    if (image_available_ != VK_NULL_HANDLE) {
      vkDestroySemaphore(context_.device(), image_available_, nullptr);
      image_available_ = VK_NULL_HANDLE;
    }
    if (command_pool_ != VK_NULL_HANDLE) {
      vkDestroyCommandPool(context_.device(), command_pool_, nullptr);
      command_pool_ = VK_NULL_HANDLE;
    }
  }
  context_.shutdown();
  render_width_ = 0;
  render_height_ = 0;
}

bool RendererBackend::create_swapchain() {
  SwapchainSupport support{};
  if (!query_swapchain_support(context_.physical_device(), context_.surface(),
                               support) ||
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
  create_info.surface = context_.surface();
  create_info.minImageCount = image_count;
  create_info.imageFormat = surface_format.format;
  create_info.imageColorSpace = surface_format.colorSpace;
  create_info.imageExtent = extent;
  create_info.imageArrayLayers = 1;
  create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

  std::array<uint32_t, 2> queue_family_indices{
      context_.queue_families().graphics,
      context_.queue_families().present,
  };
  if (context_.queue_families().graphics != context_.queue_families().present) {
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

  if (!check_vk(vkCreateSwapchainKHR(context_.device(), &create_info, nullptr,
                                     &swapchain_),
                "failed to create Vulkan swapchain")) {
    return false;
  }

  if (!check_vk(vkGetSwapchainImagesKHR(context_.device(), swapchain_,
                                        &image_count, nullptr),
                "failed to count Vulkan swapchain images")) {
    return false;
  }
  swapchain_images_.resize(image_count);
  if (!check_vk(vkGetSwapchainImagesKHR(context_.device(), swapchain_,
                                        &image_count, swapchain_images_.data()),
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

    if (!check_vk(vkCreateImageView(context_.device(), &create_info, nullptr,
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
  create_info.queueFamilyIndex = context_.queue_families().graphics;

  return check_vk(vkCreateCommandPool(context_.device(), &create_info, nullptr,
                                      &command_pool_),
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

  return check_vk(vkAllocateCommandBuffers(context_.device(), &allocate_info,
                                           command_buffers_.data()),
                  "failed to allocate Vulkan command buffers");
}

bool RendererBackend::create_sync_objects() {
  VkSemaphoreCreateInfo semaphore_info{};
  semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

  VkFenceCreateInfo fence_info{};
  fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

  return check_vk(vkCreateSemaphore(context_.device(), &semaphore_info, nullptr,
                                    &image_available_),
                  "failed to create Vulkan image-available semaphore") &&
         check_vk(vkCreateSemaphore(context_.device(), &semaphore_info, nullptr,
                                    &render_finished_),
                  "failed to create Vulkan render-finished semaphore") &&
         check_vk(vkCreateFence(context_.device(), &fence_info, nullptr,
                                &frame_in_flight_),
                  "failed to create Vulkan frame fence");
}

bool RendererBackend::create_frame_resources() {
  if (create_swapchain() && create_image_views() && create_command_buffers() &&
      build_pipeline()) {
    return true;
  }

  destroy_pipeline();
  destroy_swapchain();
  return false;
}

bool RendererBackend::frame_resources_ready() const {
  return swapchain_ != VK_NULL_HANDLE && !swapchain_images_.empty() &&
         swapchain_image_views_.size() == swapchain_images_.size() &&
         command_buffers_.size() == swapchain_images_.size() &&
         pipeline_layout_ != VK_NULL_HANDLE && pipeline_ != VK_NULL_HANDLE;
}

void RendererBackend::recreate_frame_resources() {
  if (render_width_ == 0 || render_height_ == 0) {
    return;
  }

  if (context_.device() != VK_NULL_HANDLE) {
    vkDeviceWaitIdle(context_.device());
  }
  destroy_pipeline();
  destroy_swapchain();

  if (create_frame_resources()) {
    return;
  }

  std::fprintf(stderr,
               "[renderer] failed to recreate Vulkan frame resources\n");
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
  if (!check_vk(vkMapMemory(context_.device(), vertex_buffer_memory_, 0,
                            buffer_size, 0, &data),
                "failed to map Vulkan vertex buffer")) {
    return false;
  }
  std::memcpy(data, vertices, sizeof(vertices));
  vkUnmapMemory(context_.device(), vertex_buffer_memory_);

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

  return check_vk(vkCreateDescriptorSetLayout(context_.device(), &create_info,
                                              nullptr, &descriptor_set_layout_),
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

  return check_vk(vkCreateDescriptorPool(context_.device(), &create_info,
                                         nullptr, &descriptor_pool_),
                  "failed to create Vulkan descriptor pool");
}

bool RendererBackend::create_descriptor_set() {
  VkDescriptorSetAllocateInfo allocate_info{};
  allocate_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocate_info.descriptorPool = descriptor_pool_;
  allocate_info.descriptorSetCount = 1;
  allocate_info.pSetLayouts = &descriptor_set_layout_;

  if (!check_vk(vkAllocateDescriptorSets(context_.device(), &allocate_info,
                                         &descriptor_set_),
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

  vkUpdateDescriptorSets(context_.device(), 1, &descriptor_write, 0, nullptr);
  return true;
}

bool RendererBackend::build_pipeline() {
  VkShaderModule vertex_shader = VK_NULL_HANDLE;
  VkShaderModule fragment_shader = VK_NULL_HANDLE;
  if (!create_shader_module(ENGINE_VULKAN_VERTEX_SPV_PATH, vertex_shader) ||
      !create_shader_module(ENGINE_VULKAN_FRAGMENT_SPV_PATH, fragment_shader)) {
    if (vertex_shader != VK_NULL_HANDLE) {
      vkDestroyShaderModule(context_.device(), vertex_shader, nullptr);
    }
    if (fragment_shader != VK_NULL_HANDLE) {
      vkDestroyShaderModule(context_.device(), fragment_shader, nullptr);
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

  if (!check_vk(vkCreatePipelineLayout(context_.device(), &layout_info, nullptr,
                                       &pipeline_layout_),
                "failed to create Vulkan pipeline layout")) {
    vkDestroyShaderModule(context_.device(), vertex_shader, nullptr);
    vkDestroyShaderModule(context_.device(), fragment_shader, nullptr);
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
      check_vk(vkCreateGraphicsPipelines(context_.device(), VK_NULL_HANDLE, 1,
                                         &pipeline_info, nullptr, &pipeline_),
               "failed to create Vulkan graphics pipeline");
  if (!created) {
    destroy_pipeline();
  }

  vkDestroyShaderModule(context_.device(), vertex_shader, nullptr);
  vkDestroyShaderModule(context_.device(), fragment_shader, nullptr);
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

  return check_vk(vkCreateShaderModule(context_.device(), &create_info, nullptr,
                                       &shader_module),
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

  if (!check_vk(
          vkCreateBuffer(context_.device(), &buffer_info, nullptr, &buffer),
          "failed to create Vulkan buffer")) {
    return false;
  }

  VkMemoryRequirements memory_requirements{};
  vkGetBufferMemoryRequirements(context_.device(), buffer,
                                &memory_requirements);

  const uint32_t memory_type =
      find_memory_type(context_.physical_device(),
                       memory_requirements.memoryTypeBits, properties);
  if (memory_type == invalid_queue_family) {
    std::fprintf(stderr,
                 "[renderer] failed to find suitable Vulkan buffer memory\n");
    return false;
  }

  VkMemoryAllocateInfo allocate_info{};
  allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  allocate_info.allocationSize = memory_requirements.size;
  allocate_info.memoryTypeIndex = memory_type;

  if (!check_vk(
          vkAllocateMemory(context_.device(), &allocate_info, nullptr, &memory),
          "failed to allocate Vulkan buffer memory")) {
    return false;
  }

  return check_vk(vkBindBufferMemory(context_.device(), buffer, memory, 0),
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
  if (!check_vk(vkMapMemory(context_.device(), frame_uniform_buffer_memory_, 0,
                            sizeof(FrameUniforms), 0, &data),
                "failed to map Vulkan frame uniform buffer")) {
    return;
  }
  std::memcpy(data, &uniforms, sizeof(uniforms));
  vkUnmapMemory(context_.device(), frame_uniform_buffer_memory_);
}

void RendererBackend::destroy_pipeline() {
  if (pipeline_ != VK_NULL_HANDLE) {
    vkDestroyPipeline(context_.device(), pipeline_, nullptr);
    pipeline_ = VK_NULL_HANDLE;
  }
  if (pipeline_layout_ != VK_NULL_HANDLE) {
    vkDestroyPipelineLayout(context_.device(), pipeline_layout_, nullptr);
    pipeline_layout_ = VK_NULL_HANDLE;
  }
}

void RendererBackend::destroy_swapchain() {
  if (command_pool_ != VK_NULL_HANDLE && !command_buffers_.empty()) {
    vkFreeCommandBuffers(context_.device(), command_pool_,
                         static_cast<uint32_t>(command_buffers_.size()),
                         command_buffers_.data());
    command_buffers_.clear();
  }
  for (VkImageView image_view : swapchain_image_views_) {
    vkDestroyImageView(context_.device(), image_view, nullptr);
  }
  swapchain_image_views_.clear();
  swapchain_images_.clear();
  if (swapchain_ != VK_NULL_HANDLE) {
    vkDestroySwapchainKHR(context_.device(), swapchain_, nullptr);
    swapchain_ = VK_NULL_HANDLE;
  }
  swapchain_format_ = VK_FORMAT_UNDEFINED;
  swapchain_extent_ = {};
}

void RendererBackend::destroy_buffer(VkBuffer &buffer, VkDeviceMemory &memory) {
  if (buffer != VK_NULL_HANDLE) {
    vkDestroyBuffer(context_.device(), buffer, nullptr);
    buffer = VK_NULL_HANDLE;
  }
  if (memory != VK_NULL_HANDLE) {
    vkFreeMemory(context_.device(), memory, nullptr);
    memory = VK_NULL_HANDLE;
  }
}
