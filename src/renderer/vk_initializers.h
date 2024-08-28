#pragma once

#include "vk_types.h"

namespace vkinit {

VkCommandPoolCreateInfo command_pool_create_info(uint32_t queue_family_index, VkCommandPoolCreateFlags flags = 0);
VkCommandBufferAllocateInfo command_buffer_allocate_info(VkCommandPool pool, uint32_t count = 1);
VkFenceCreateInfo fence_create_info(VkFenceCreateFlags flags);
VkSemaphoreCreateInfo semaphore_create_info(VkSemaphoreCreateFlags flags = 0);
VkCommandBufferBeginInfo command_buffer_begin_info(VkCommandBufferUsageFlags flags = 0);
VkImageSubresourceRange image_subresource_range(VkImageAspectFlags aspect_mask);
VkSemaphoreSubmitInfo semaphore_submit_info(VkPipelineStageFlags2 stage_mask, VkSemaphore semaphore);
VkCommandBufferSubmitInfo command_buffer_submit_info(VkCommandBuffer cmd);
VkSubmitInfo2 submit_info(VkCommandBufferSubmitInfo* cmd, VkSemaphoreSubmitInfo* signal_semaphore_info, VkSemaphoreSubmitInfo* wait_semaphore_info);
VkImageCreateInfo image_create_info(VkFormat format, VkImageUsageFlags usageFlags, VkExtent3D extent);
VkImageViewCreateInfo image_view_create_info(VkFormat format, VkImage image, VkImageAspectFlags aspectFlags);
VkRenderingAttachmentInfo attachment_info(
    VkImageView view, VkClearValue* clear ,VkImageLayout layout);
VkRenderingAttachmentInfo depth_attachment_info(
    VkImageView view, VkImageLayout layout);
VkRenderingInfo rendering_info(VkExtent2D render_extent, VkRenderingAttachmentInfo* color_attachment,
    VkRenderingAttachmentInfo* depth_attachment);
VkPipelineLayoutCreateInfo pipeline_layout_create_info();
VkPipelineShaderStageCreateInfo pipeline_shader_stage_create_info(VkShaderStageFlagBits stage,
    VkShaderModule shader_module, const char* entry = "main");
VkPresentInfoKHR present_info();
};