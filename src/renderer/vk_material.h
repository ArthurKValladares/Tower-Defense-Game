#pragma once

#include "vk_types.h"

enum class MaterialPass :uint8_t {
    MainColor,
    Transparent,
    Other
};

struct MaterialPipeline {
	VkPipeline pipeline;
    VkPipeline wireframe_pipeline;
	VkPipelineLayout layout;
};

struct MaterialInstance {
    MaterialPipeline* pipeline;
    VkDescriptorSet material_set;
    MaterialPass pass_type;
};