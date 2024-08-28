#pragma once

#include <print>

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>

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

struct AllocatedBuffer {
    VkBuffer buffer;
    VmaAllocation allocation;
    VmaAllocationInfo info;
};

struct Vertex {
	glm::vec3 position;
	float uv_x;
	glm::vec3 normal;
	float uv_y;
	glm::vec4 color;
};

struct GPUMeshBuffers {
    AllocatedBuffer index_buffer;
    AllocatedBuffer vertex_buffer;
    VkDeviceAddress vertex_buffer_address;
};

struct GPUDrawPushConstants {
    glm::mat4 world_matrix;
    VkDeviceAddress vertex_buffer;
};