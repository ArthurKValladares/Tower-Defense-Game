#include "vk_descriptors.h"

#include <cassert>

void DescriptorLayoutBuilder::add_binding(uint32_t binding, VkDescriptorType type)
{
    VkDescriptorSetLayoutBinding newbind {};
    newbind.binding = binding;
    newbind.descriptorCount = 1;
    newbind.descriptorType = type;

    bindings.push_back(newbind);
}

void DescriptorLayoutBuilder::clear()
{
    bindings.clear();
}

VkDescriptorSetLayout DescriptorLayoutBuilder::build(VkDevice device, VkShaderStageFlags shader_stages, void* pNext, VkDescriptorSetLayoutCreateFlags flags)
{
    for (auto& b : bindings) {
        b.stageFlags |= shader_stages;
    }

    VkDescriptorSetLayoutCreateInfo info = {.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    info.pNext = pNext;

    info.pBindings = bindings.data();
    info.bindingCount = (uint32_t)bindings.size();
    info.flags = flags;

    VkDescriptorSetLayout set;
    VK_CHECK(vkCreateDescriptorSetLayout(device, &info, nullptr, &set));

    return set;
}

void DescriptorAllocator::init(VkDevice device, uint32_t initial_sets, std::span<const PoolSizeRatio> pool_ratios)
{
    ratios.clear();
    
    for (auto r : pool_ratios) {
        ratios.push_back(r);
    }
	
    VkDescriptorPool new_pool = create_pool(device, initial_sets, pool_ratios);

    // Grow it for next allocation, since we will only need it 
    // if this one runs out
    sets_per_pool = initial_sets * 1.5;

    ready_pools.push_back(new_pool);
}

void DescriptorAllocator::clear_pools(VkDevice device)
{
    for (auto p : ready_pools) {
        VK_CHECK(vkResetDescriptorPool(device, p, 0));
    }
    // Clean full pulls and add them to `ready` set
    for (auto p : full_pools) {
        VK_CHECK(vkResetDescriptorPool(device, p, 0));
        ready_pools.push_back(p);
    }
    full_pools.clear();
}

void DescriptorAllocator::destroy_pools(VkDevice device)
{
    for (auto p : ready_pools) {
		vkDestroyDescriptorPool(device, p, nullptr);
	}
    ready_pools.clear();

	for (auto p : full_pools) {
		vkDestroyDescriptorPool(device,p,nullptr);
    }
    full_pools.clear();
}

VkDescriptorSet DescriptorAllocator::allocate(VkDevice device, VkDescriptorSetLayout layout, void* p_next)
{
    VkDescriptorPool pool_to_use = get_pool(device);

	VkDescriptorSetAllocateInfo alloc_info = {};
	alloc_info.pNext = p_next;
	alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	alloc_info.descriptorPool = pool_to_use;
	alloc_info.descriptorSetCount = 1;
	alloc_info.pSetLayouts = &layout;

	VkDescriptorSet descriptor_set;
	VkResult result = vkAllocateDescriptorSets(device, &alloc_info, &descriptor_set);

    if (result == VK_ERROR_OUT_OF_POOL_MEMORY || result == VK_ERROR_FRAGMENTED_POOL) {

        full_pools.push_back(pool_to_use);
    
        pool_to_use = get_pool(device);
        alloc_info.descriptorPool = pool_to_use;

       VK_CHECK( vkAllocateDescriptorSets(device, &alloc_info, &descriptor_set));
    }
  
    ready_pools.push_back(pool_to_use);
    return descriptor_set;
}

VkDescriptorPool DescriptorAllocator::get_pool(VkDevice device)
{       
    VkDescriptorPool new_pool;
    if (ready_pools.size() != 0) {
        new_pool = ready_pools.back();
        ready_pools.pop_back();
    }
    else {
	    new_pool = create_pool(device, sets_per_pool, ratios);

	    sets_per_pool = sets_per_pool * 1.5;
	    if (sets_per_pool > 4092) {
		    sets_per_pool = 4092;
	    }
    }   

    return new_pool;
}

VkDescriptorPool DescriptorAllocator::create_pool(VkDevice device, uint32_t set_count, std::span<const PoolSizeRatio> pool_ratios)
{
	std::vector<VkDescriptorPoolSize> pool_sizes;
	for (PoolSizeRatio ratio : pool_ratios) {
		pool_sizes.push_back(VkDescriptorPoolSize{
			.type = ratio.type,
			.descriptorCount = uint32_t(ratio.ratio * set_count)
		});
	}

	VkDescriptorPoolCreateInfo pool_info = {};
	pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	pool_info.flags = 0;
	pool_info.maxSets = set_count;
	pool_info.poolSizeCount = (uint32_t)pool_sizes.size();
	pool_info.pPoolSizes = pool_sizes.data();

	VkDescriptorPool new_pool;
	vkCreateDescriptorPool(device, &pool_info, nullptr, &new_pool);
    return new_pool;
}

void DescriptorWriter::write_buffer(int binding, VkBuffer buffer, size_t size, size_t offset, VkDescriptorType type)
{
	const VkDescriptorBufferInfo& info = buffer_infos.emplace_back(VkDescriptorBufferInfo{
		.buffer = buffer,
		.offset = offset,
		.range = size
    });

	VkWriteDescriptorSet write = {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};

	write.dstBinding = binding;
    // NOTE: left empty until we need to write it
	write.dstSet = VK_NULL_HANDLE;
	write.descriptorCount = 1;
	write.descriptorType = type;
	write.pBufferInfo = &info;

	writes.push_back(write);
}

void DescriptorWriter::write_image(int binding,VkImageView image, VkSampler sampler,  VkImageLayout layout, VkDescriptorType type)
{
    const VkDescriptorImageInfo& info = image_infos.emplace_back(VkDescriptorImageInfo{
		.sampler = sampler,
		.imageView = image,
		.imageLayout = layout
	});

	VkWriteDescriptorSet write = { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };

	write.dstBinding = binding;
    // NOTE: left empty until we need to write it
	write.dstSet = VK_NULL_HANDLE;
	write.descriptorCount = 1;
	write.descriptorType = type;
	write.pImageInfo = &info;

	writes.push_back(write);
}

void DescriptorWriter::clear()
{
    image_infos.clear();
    buffer_infos.clear();

    writes.clear();
}

void DescriptorWriter::update_set(VkDevice device, VkDescriptorSet set)
{
    for (VkWriteDescriptorSet& write : writes) {
        write.dstSet = set;
    }

    vkUpdateDescriptorSets(device, (uint32_t)writes.size(), writes.data(), 0, nullptr);
}