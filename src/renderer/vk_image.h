#pragma once

#include "vk_types.h"

namespace vkutil {
void transition_image(VkCommandBuffer cmd, VkImage image, VkImageLayout current_layout, VkImageLayout new_layout);
void copy_image_to_image(VkCommandBuffer cmd, VkImage source, VkImage destination, VkExtent2D src_size, VkExtent2D dst_size);
// TODO: generate them in a compute shader that generates multiple levels at once.
void generate_mipmaps(VkCommandBuffer cmd, VkImage image, VkExtent2D image_size);
};