#pragma once

#include "vk_material.h"
#include "vk_descriptors.h"

struct VkEngine;
struct GLTFMetallic_Roughness {
	MaterialPipeline opaque_pipeline;
	MaterialPipeline transparent_pipeline;

	VkDescriptorSetLayout material_layout;

	struct MaterialConstants {
		glm::vec4 color_factors;
		glm::vec4 metal_rough_factors;
		// Pad to 256 bytes
		glm::vec4 padding[14];
	};

	struct MaterialResources {
		AllocatedImage color_image;
		VkSampler color_sampler;

		AllocatedImage metal_rough_image;
		VkSampler metal_rough_sampler;

		VkBuffer data_buffer;
		uint32_t data_buffer_offset;
	};

	DescriptorWriter writer;

	void build_pipelines(VkEngine* engine);
	void clear_resources(VkDevice device);

	MaterialInstance write_material(VkDevice device, MaterialPass pass, const MaterialResources& resources, DescriptorAllocator& descriptor_allocator);
};

// TODO: This needs to be better later
struct FlatColorMaterial {
	MaterialPipeline pipeline;
	VkDescriptorSetLayout material_layout;

	struct MaterialConstants {
		glm::vec4 color_factors;
		glm::vec4 metal_rough_factors;
		// Pad to 256 bytes
		glm::vec4 padding[14];
	};

	struct MaterialResources {
		VkBuffer data_buffer;
		uint32_t data_buffer_offset;
	};

	DescriptorWriter writer;

	void build_pipelines(VkEngine* engine);
	void clear_resources(VkDevice device);

	MaterialInstance write_material(VkDevice device, const MaterialResources& resources, DescriptorAllocator& descriptor_allocator);
};