#pragma once

#include <vector>
#include <deque>
#include <optional>
#include <span>

#include "vk_types.h"

struct DescriptorLayoutBuilder {

    std::vector<VkDescriptorSetLayoutBinding> bindings;

    void add_binding(uint32_t binding, VkDescriptorType type);
    void clear();
    // NOTE: We don't support per-binding shader stages, it is forced to be the same for binding in a set
    VkDescriptorSetLayout build(VkDevice device, VkShaderStageFlags shader_stages, void* pNext = nullptr, VkDescriptorSetLayoutCreateFlags flags = 0);
};

struct DescriptorAllocator {

    struct PoolSizeRatio{
		  VkDescriptorType type;
		  float ratio;
    };

    void init(VkDevice device, uint32_t initial_sets, std::span<PoolSizeRatio> pool_ratios);
    void clear_pools(VkDevice device);
    void destroy_pools(VkDevice device);

    VkDescriptorSet allocate(VkDevice device, VkDescriptorSetLayout layout, void* pNext = nullptr);
private:
  VkDescriptorPool get_pool(VkDevice device);
	VkDescriptorPool create_pool(VkDevice device, uint32_t set_count, std::span<PoolSizeRatio> pool_ratios);

	std::vector<PoolSizeRatio> ratios;
	std::vector<VkDescriptorPool> full_pools;
	std::vector<VkDescriptorPool> ready_pools;
	uint32_t sets_per_pool;
};

struct DescriptorWriter {
    // NOTES: deques are useful because they are guaranteed to keep pointer to elements
    // valids even as we add elements
    std::deque<VkDescriptorImageInfo> image_infos;
    std::deque<VkDescriptorBufferInfo> buffer_infos;

    std::vector<VkWriteDescriptorSet> writes;

    void write_image(int binding, VkImageView image, VkSampler sampler, VkImageLayout layout, VkDescriptorType type);
    void write_buffer(int binding, VkBuffer buffer, size_t size, size_t offset, VkDescriptorType type); 

    void clear();
    void update_set(VkDevice device, VkDescriptorSet set);
};