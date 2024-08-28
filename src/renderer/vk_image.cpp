#include "vk_image.h"
#include "vk_initializers.h"

void vkutil::transition_image(VkCommandBuffer cmd, VkImage image, VkImageLayout current_layout, VkImageLayout new_layout)
{
    VkImageMemoryBarrier2 image_barrier {.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
    image_barrier.pNext = nullptr;

    // TODO: The ALL_COMMANDS stage mask in inefficient, can be made better/more precise later
    // https://github.com/KhronosGroup/Vulkan-Docs/wiki/Synchronization-Examples
    image_barrier.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    image_barrier.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
    image_barrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    image_barrier.dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT;

    image_barrier.oldLayout = current_layout;
    image_barrier.newLayout = new_layout;

    VkImageAspectFlags aspect_mask = (new_layout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL)
        ? VK_IMAGE_ASPECT_DEPTH_BIT
        : VK_IMAGE_ASPECT_COLOR_BIT;
    image_barrier.subresourceRange = vkinit::image_subresource_range(aspect_mask);
    image_barrier.image = image;

    VkDependencyInfo dep_info {};
    dep_info.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dep_info.pNext = nullptr;

    dep_info.imageMemoryBarrierCount = 1;
    dep_info.pImageMemoryBarriers = &image_barrier;

    vkCmdPipelineBarrier2(cmd, &dep_info);
}

void vkutil::copy_image_to_image(VkCommandBuffer cmd, VkImage source, VkImage destination, VkExtent2D src_size, VkExtent2D dst_size)
{
	VkImageBlit2 blit_region{ .sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2, .pNext = nullptr };

	blit_region.srcOffsets[1].x = src_size.width;
	blit_region.srcOffsets[1].y = src_size.height;
	blit_region.srcOffsets[1].z = 1;

	blit_region.dstOffsets[1].x = dst_size.width;
	blit_region.dstOffsets[1].y = dst_size.height;
	blit_region.dstOffsets[1].z = 1;

	blit_region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	blit_region.srcSubresource.baseArrayLayer = 0;
	blit_region.srcSubresource.layerCount = 1;
	blit_region.srcSubresource.mipLevel = 0;

	blit_region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	blit_region.dstSubresource.baseArrayLayer = 0;
	blit_region.dstSubresource.layerCount = 1;
	blit_region.dstSubresource.mipLevel = 0;

	VkBlitImageInfo2 blit_info{ .sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2, .pNext = nullptr };
	blit_info.dstImage = destination;
	blit_info.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	blit_info.srcImage = source;
	blit_info.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	blit_info.filter = VK_FILTER_LINEAR;
	blit_info.regionCount = 1;
	blit_info.pRegions = &blit_region;

	vkCmdBlitImage2(cmd, &blit_info);
}

void vkutil::generate_mipmaps(VkCommandBuffer cmd, VkImage image, VkExtent2D image_size) {
	const int mip_levels = int(std::floor(std::log2(std::max(image_size.width, image_size.height)))) + 1;

    for (int mip = 0; mip < mip_levels; mip++) {

        VkExtent2D half_size = image_size;
        half_size.width /= 2;
        half_size.height /= 2;

        VkImageMemoryBarrier2 image_barrier { .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2, .pNext = nullptr };
        image_barrier.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
        image_barrier.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
        image_barrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
        image_barrier.dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT;

        image_barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        image_barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

        VkImageAspectFlags aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT;
        image_barrier.subresourceRange = vkinit::image_subresource_range(aspect_mask);
        image_barrier.subresourceRange.levelCount = 1;
        image_barrier.subresourceRange.baseMipLevel = mip;
        image_barrier.image = image;

        VkDependencyInfo dep_info { .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO, .pNext = nullptr };
        dep_info.imageMemoryBarrierCount = 1;
        dep_info.pImageMemoryBarriers = &image_barrier;

        vkCmdPipelineBarrier2(cmd, &dep_info);

        if (mip < mip_levels - 1) {
            VkImageBlit2 blitRegion { .sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2, .pNext = nullptr };

            blitRegion.srcOffsets[1].x = image_size.width;
            blitRegion.srcOffsets[1].y = image_size.height;
            blitRegion.srcOffsets[1].z = 1;

            blitRegion.dstOffsets[1].x = half_size.width;
            blitRegion.dstOffsets[1].y = half_size.height;
            blitRegion.dstOffsets[1].z = 1;

            blitRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            blitRegion.srcSubresource.baseArrayLayer = 0;
            blitRegion.srcSubresource.layerCount = 1;
            blitRegion.srcSubresource.mipLevel = mip;

            blitRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            blitRegion.dstSubresource.baseArrayLayer = 0;
            blitRegion.dstSubresource.layerCount = 1;
            blitRegion.dstSubresource.mipLevel = mip + 1;

            VkBlitImageInfo2 blit_info {.sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2, .pNext = nullptr};
            blit_info.dstImage = image;
            blit_info.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            blit_info.srcImage = image;
            blit_info.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            blit_info.filter = VK_FILTER_LINEAR;
            blit_info.regionCount = 1;
            blit_info.pRegions = &blitRegion;

            vkCmdBlitImage2(cmd, &blit_info);

            image_size = half_size;
        }
    }

    // Transition all mip levels into the final read_only layout
    transition_image(cmd, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}