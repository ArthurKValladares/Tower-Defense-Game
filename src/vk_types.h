#pragma once

#include <print>

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

#include "vk_string.h"

#define VK_CHECK(x)                                                               \
    do {                                                                          \
        VkResult err = x;                                                         \
        if (err) {                                                                \
             std::print("Detected Vulkan error: {}\n", vkutil::to_c_string(err)); \
            abort();                                                              \
        }                                                                         \
    } while (0)

struct AllocatedImage {
    VkImage image;
    VkImageView image_view;
    VmaAllocation allocation;
    VkExtent3D image_extent;
    VkFormat image_format;
};
