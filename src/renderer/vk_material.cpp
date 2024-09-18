#include "vk_material.h"

void MaterialPipeline::destroy(VkDevice device, bool destroy_layout) {
    if (destroy_layout) {
        vkDestroyPipelineLayout(device, layout, nullptr);
    }
	vkDestroyPipeline(device, pipeline, nullptr);
	vkDestroyPipeline(device, wireframe_pipeline, nullptr);
}