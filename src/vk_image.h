#pragma once

#include "vk_types.h"

namespace vkutil {
void transition_image(VkCommandBuffer cmd, VkImage image, VkImageLayout current_layout, VkImageLayout new_layout);
};