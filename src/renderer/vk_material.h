#pragma once

#include "vk_types.h"

enum class MaterialPass :uint8_t {
    MainColor,
    Transparent,
    Other
};

struct MaterialPipeline {
    // TODO: Create function, with a layout optional param
	VkPipeline pipeline;
    VkPipeline wireframe_pipeline;
	VkPipelineLayout layout;

    void destroy(VkDevice device, bool destroy_layout = true);
};

struct MaterialInstance {
    MaterialPipeline* pipeline;
    VkDescriptorSet material_set;
    MaterialPass pass_type;
};