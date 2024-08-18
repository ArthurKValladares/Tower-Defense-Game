#include "vk_pipelines.h"
#include "vk_initializers.h"

#include <vector>
#include <fstream>
#include <print>

bool vkutil::load_shader_module(const char* file_path,
    VkDevice device,
    VkShaderModule* out_shader_module)
{
    // Open the file with cursor at the end
    std::ifstream file(file_path, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        return false;
    }

    size_t file_size_bytes = (size_t) file.tellg();

    std::vector<uint32_t> buffer(file_size_bytes / sizeof(uint32_t));

    // Read file into buffer
    file.seekg(0);
    file.read((char*) buffer.data(), file_size_bytes);
    file.close();

    VkShaderModuleCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    create_info.pNext = nullptr;

    create_info.codeSize = buffer.size() * sizeof(uint32_t);
    create_info.pCode = buffer.data();

    VkShaderModule shader_module;
    if (vkCreateShaderModule(device, &create_info, nullptr, &shader_module) != VK_SUCCESS) {
        return false;
    }

    *out_shader_module = shader_module;

    return true;
}


void PipelineBuilder::clear()
{
    _input_assembly = { .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };

    _rasterization = { .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };

    _color_blend_attachment = {};

    _multisample = { .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };

    _pipeline_layout = {};

    _depth_stencil = { .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };

    _rendering = { .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };

    _shader_stages.clear();
}

VkPipeline PipelineBuilder::build_pipeline(VkDevice device)
{
    // Setup hard-coded pipeline states

    VkPipelineViewportStateCreateInfo viewport_state = {};
    viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state.pNext = nullptr;

    viewport_state.viewportCount = 1;
    viewport_state.scissorCount = 1;

    VkPipelineColorBlendStateCreateInfo color_blending = {};
    color_blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    color_blending.pNext = nullptr;

    color_blending.logicOpEnable = VK_FALSE;
    //color_blending.logicOp = VK_LOGIC_OP_COPY;
    color_blending.attachmentCount = 1;
    color_blending.pAttachments = &_color_blend_attachment;

    VkPipelineVertexInputStateCreateInfo vertex_input_info = {};
    vertex_input_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_input_info.pNext = nullptr;

    // Create Graphics Pipeline

    VkDynamicState state[] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };

    VkPipelineDynamicStateCreateInfo dynamic_info = {};
    dynamic_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic_info.pNext = nullptr;
    dynamic_info.pDynamicStates = &state[0];
    dynamic_info.dynamicStateCount = 2;

    // NOTE: Connect the RenderingInfo to the pnext chain
    VkGraphicsPipelineCreateInfo pipelineInfo = {};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.pNext = &_rendering;

    pipelineInfo.stageCount = (uint32_t) _shader_stages.size();
    pipelineInfo.pStages = _shader_stages.data();
    pipelineInfo.pVertexInputState = &vertex_input_info;
    pipelineInfo.pInputAssemblyState = &_input_assembly;
    pipelineInfo.pViewportState = &viewport_state;
    pipelineInfo.pRasterizationState = &_rasterization;
    pipelineInfo.pMultisampleState = &_multisample;
    pipelineInfo.pColorBlendState = &color_blending;
    pipelineInfo.pDepthStencilState = &_depth_stencil;
    pipelineInfo.layout = _pipeline_layout;
    pipelineInfo.pDynamicState = &dynamic_info;

    VkPipeline new_pipeline;
    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo,
            nullptr, &new_pipeline) != VK_SUCCESS) {
        std::println("failed to create pipeline");
        return VK_NULL_HANDLE;
    } else {
        return new_pipeline;
    }
}

void PipelineBuilder::set_shaders(VkShaderModule vertex_shader, VkShaderModule fragment_shader)
{
    _shader_stages.clear();

    _shader_stages.push_back(
        vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_VERTEX_BIT, vertex_shader));

    _shader_stages.push_back(
        vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_FRAGMENT_BIT, fragment_shader));
}

void PipelineBuilder::set_input_topology(VkPrimitiveTopology topology)
{
    _input_assembly.topology = topology;
    _input_assembly.primitiveRestartEnable = VK_FALSE;
}

void PipelineBuilder::set_polygon_mode(VkPolygonMode mode)
{
    _rasterization.polygonMode = mode;
    _rasterization.lineWidth = 1.f;
}

void PipelineBuilder::set_cull_mode(VkCullModeFlags cull_mode, VkFrontFace front_face)
{
    _rasterization.cullMode = cull_mode;
    _rasterization.frontFace = front_face;
}

void PipelineBuilder::set_multisampling_none()
{
    _multisample.sampleShadingEnable = VK_FALSE;
    _multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    _multisample.minSampleShading = 1.0f;
    _multisample.pSampleMask = nullptr;
    _multisample.alphaToCoverageEnable = VK_FALSE;
    _multisample.alphaToOneEnable = VK_FALSE;
}

void PipelineBuilder::disable_blending()
{
    _color_blend_attachment.colorWriteMask = 
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    _color_blend_attachment.blendEnable = VK_FALSE;
}

void PipelineBuilder::enable_blending_additive()
{
    _color_blend_attachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    _color_blend_attachment.blendEnable = VK_TRUE;
    _color_blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    _color_blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
    _color_blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
    _color_blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    _color_blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    _color_blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;
}

void PipelineBuilder::enable_blending_alphablend()
{
    _color_blend_attachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    _color_blend_attachment.blendEnable = VK_TRUE;
    _color_blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    _color_blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    _color_blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
    _color_blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    _color_blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    _color_blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;
}

void PipelineBuilder::disable_depthtest()
{
    _depth_stencil.depthTestEnable = VK_FALSE;
    _depth_stencil.depthWriteEnable = VK_FALSE;
    _depth_stencil.depthCompareOp = VK_COMPARE_OP_NEVER;
    _depth_stencil.depthBoundsTestEnable = VK_FALSE;
    _depth_stencil.stencilTestEnable = VK_FALSE;
    _depth_stencil.front = {};
    _depth_stencil.back = {};
    _depth_stencil.minDepthBounds = 0.f;
    _depth_stencil.maxDepthBounds = 1.f;
}

void PipelineBuilder::enable_depthtest(bool depth_write_enable, VkCompareOp op)
{
    _depth_stencil.depthTestEnable = VK_TRUE;
    _depth_stencil.depthWriteEnable = depth_write_enable;
    _depth_stencil.depthCompareOp = op;
    _depth_stencil.depthBoundsTestEnable = VK_FALSE;
    _depth_stencil.stencilTestEnable = VK_FALSE;
    _depth_stencil.front = {};
    _depth_stencil.back = {};
    _depth_stencil.minDepthBounds = 0.f;
    _depth_stencil.maxDepthBounds = 1.f;
}

void PipelineBuilder::set_color_attachment_format(VkFormat format)
{
    _color_attachment_format = format;

    _rendering.colorAttachmentCount = 1;
    _rendering.pColorAttachmentFormats = &_color_attachment_format;
}

void PipelineBuilder::set_depth_format(VkFormat format)
{
    _rendering.depthAttachmentFormat = format;
}