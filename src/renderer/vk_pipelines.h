#pragma once

#include <vector>

#include "vk_types.h"

namespace vkutil {

bool load_shader_module(const char* file_path,
    VkDevice device,
    VkShaderModule* out_shader_module);
    
};

struct PipelineBuilder {
    std::vector<VkPipelineShaderStageCreateInfo> _shader_stages;
   
    VkPipelineInputAssemblyStateCreateInfo _input_assembly;
    VkPipelineRasterizationStateCreateInfo _rasterization;
    VkPipelineColorBlendAttachmentState _color_blend_attachment;
    VkPipelineMultisampleStateCreateInfo _multisample;
    VkPipelineLayout _pipeline_layout;
    VkPipelineDepthStencilStateCreateInfo _depth_stencil;
    VkPipelineRenderingCreateInfo _rendering;
    VkFormat _color_attachment_format;

	PipelineBuilder() { 
        clear(); 
    }

    void clear();

    void set_shaders(VkShaderModule vertex_shader, VkShaderModule fragment_shader);
    void set_input_topology(VkPrimitiveTopology topology);
    void set_polygon_mode(VkPolygonMode mode);
    void set_cull_mode(VkCullModeFlags cull_mode, VkFrontFace front_face);
    void set_color_attachment_format(VkFormat format);
    void set_depth_format(VkFormat format);

    void set_multisampling_none();

    void disable_blending();
    void enable_blending_additive();
    void enable_blending_alphablend();

    void disable_depthtest();
    void enable_depthtest(bool depth_write_enable, VkCompareOp op);

    VkPipeline build_pipeline(VkDevice device);
};
