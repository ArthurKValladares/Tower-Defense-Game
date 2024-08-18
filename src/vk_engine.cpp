#include "defs.h"
#include "vk_engine.h"
#include "vk_initializers.h"
#include "vk_image.h"
#include "vk_pipelines.h"

#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <VkBootstrap.h>

#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_vulkan.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/transform.hpp>

#include <print>
#include <thread>
#include <chrono>
#include <array>

#define INIT_ERROR_STRING "Engine init failed with code: {}\n"
// TODO: Make a compiler flag
#define USE_VALIDATION_LAYERS true

namespace {
    VkExtent2D get_default_window_dims() {
        // TODO: platform-specfic code to get reasonable defaults.
        return {1700 , 900};
    }
};

std::optional<EngineInitError> VkEngine::create_swapchain(uint32_t width, uint32_t height) {
    vkb::SwapchainBuilder swapchainBuilder{ _chosen_gpu, _device,_surface };

	_swapchain_image_format = VK_FORMAT_B8G8R8A8_UNORM;

	vkb::Result<vkb::Swapchain> vkb_swapchain_result = swapchainBuilder
		.set_desired_format(VkSurfaceFormatKHR{ .format = _swapchain_image_format, .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR })
		.set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
		.set_desired_extent(width, height)
		.add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
		.build();

    if (!vkb_swapchain_result.has_value()) {
        std::print(INIT_ERROR_STRING, "Could not initialize Swapchain device with vk-bootstrap");
        return EngineInitError::Vk_SwapchainInitFailed;
    }
    vkb::Swapchain vkb_swapchain = vkb_swapchain_result.value();

	_swapchain_extent = vkb_swapchain.extent;
	_swapchain = vkb_swapchain.swapchain;
    if (!vkb_swapchain.get_images().has_value()) {
        std::print(INIT_ERROR_STRING, "Could not get Swapchain Images with vk-bootstrap");
        return EngineInitError::Vk_SwapchainImagesInitFailed;
    }
	_swapchain_images = vkb_swapchain.get_images().value();
    if (!vkb_swapchain.get_images().has_value()) {
        std::print(INIT_ERROR_STRING, "Could not get Swapchain Images Views with vk-bootstrap");
        return EngineInitError::Vk_SwapchainImageViewsInitFailed;
    }
	_swapchain_image_views = vkb_swapchain.get_image_views().value();

    return std::nullopt;
}

void VkEngine::destroy_swapchain() {
    vkDestroySwapchainKHR(_device, _swapchain, nullptr);

	for (int i = 0; i < _swapchain_image_views.size(); i++) {
		vkDestroyImageView(_device, _swapchain_image_views[i], nullptr);
	}
}

std::optional<EngineInitError> VkEngine::init_vulkan() {
    // Create Instance
    vkb::InstanceBuilder builder;
	vkb::Result<vkb::Instance> vkb_instance_result = builder.set_app_name(APP_NAME)
		.request_validation_layers(USE_VALIDATION_LAYERS)
		.use_default_debug_messenger()
		.require_api_version(1, 3, 0)
		.build();

    if (!vkb_instance_result.has_value()) {
        std::print(INIT_ERROR_STRING, "Could not initialize instance with vk-bootstrap");
        return EngineInitError::VK_InstanceInitFailed;
    }
    vkb::Instance vkb_instance = vkb_instance_result.value();

    _instance = vkb_instance.instance;
    _debug_messenger = vkb_instance.debug_messenger;
	
    // Create Surface
    if (SDL_Vulkan_CreateSurface(_window, _instance, nullptr, &_surface)) {
        std::print(INIT_ERROR_STRING, SDL_GetError());
        return EngineInitError::SDL_VulkanInitFailed;
    }

    // Create Device
	VkPhysicalDeviceVulkan13Features features{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES };
	features.dynamicRendering = true;
	features.synchronization2 = true;

	VkPhysicalDeviceVulkan12Features features12{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES };
	features12.bufferDeviceAddress = true;
	features12.descriptorIndexing = true;

	vkb::PhysicalDeviceSelector selector{ vkb_instance };
	vkb::Result<vkb::PhysicalDevice> vkb_physical_device_result = selector
		.set_minimum_version(1, 3)
		.set_required_features_13(features)
		.set_required_features_12(features12)
		.set_surface(_surface)
		.select();

    if (!vkb_physical_device_result.has_value()) {
        std::print(INIT_ERROR_STRING, "Could not initialize physical device with vk-bootstrap");
        return EngineInitError::Vk_DeviceInitFailed;
    }
    vkb::PhysicalDevice vkb_physical_device = vkb_physical_device_result.value();

	vkb::DeviceBuilder device_builder{ vkb_physical_device };
	vkb::Result<vkb::Device> vkb_device_result = device_builder.build();

    if (!vkb_device_result.has_value()) {
        std::print(INIT_ERROR_STRING, "Could not initialize device with vk-bootstrap");
        return EngineInitError::Vk_DeviceInitFailed;
    }
    vkb::Device vkb_device = vkb_device_result.value();

	_device = vkb_device.device;
	_chosen_gpu = vkb_physical_device.physical_device;

    // Create Allocator
    VmaAllocatorCreateInfo allocatorInfo = {};
    allocatorInfo.physicalDevice = _chosen_gpu;
    allocatorInfo.device = _device;
    allocatorInfo.instance = _instance;
    allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
    vmaCreateAllocator(&allocatorInfo, &_allocator);

    _main_deletion_queue.push_function([=]() {
        vmaDestroyAllocator(_allocator);
    });

    // Create Graphics Queue
    vkb::Result<VkQueue> graphics_queue_result = vkb_device.get_queue(vkb::QueueType::graphics);
    if (!graphics_queue_result.has_value()) {
        std::print(INIT_ERROR_STRING, "Could not initialize graphics queue with vk-bootstrap");
        return EngineInitError::Vk_GraphicsQueueInitFailed;
    }
    _graphics_queue = graphics_queue_result.value();

	vkb::Result<uint32_t> graphics_queue_family_result = vkb_device.get_queue_index(vkb::QueueType::graphics);
    if (!graphics_queue_family_result.has_value()) {
        std::print(INIT_ERROR_STRING, "Could not initialize graphics queue family with vk-bootstrap");
        return EngineInitError::Vk_GraphicsQueueFamilyInitFailed;
    }
    _graphics_queue_family = graphics_queue_family_result.value();

    return std::nullopt;
}

std::optional<EngineInitError> VkEngine::init_swapchain() {
    const std::optional<EngineInitError> create_swapchain_result = create_swapchain(_window_extent.width, _window_extent.height);
    if (create_swapchain_result.has_value()) {
        return create_swapchain_result;
    }

    // Create Draw Image
	VkExtent3D draw_image_extent = {
		_window_extent.width,
		_window_extent.height,
		1
	};

	// NOTE: We use a hard-coded 64bit image format for the draw image for the extra precision
	_draw_image.image_format = VK_FORMAT_R16G16B16A16_SFLOAT;
	_draw_image.image_extent = draw_image_extent;

	VkImageUsageFlags draw_image_usages{};
	draw_image_usages |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	draw_image_usages |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	draw_image_usages |= VK_IMAGE_USAGE_STORAGE_BIT;
	draw_image_usages |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	VkImageCreateInfo draw_img_info = vkinit::image_create_info(_draw_image.image_format, draw_image_usages, draw_image_extent);

	VmaAllocationCreateInfo draw_img_alloc_info = {};
	draw_img_alloc_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;
	draw_img_alloc_info.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	if(vmaCreateImage(_allocator, &draw_img_info, &draw_img_alloc_info, &_draw_image.image, &_draw_image.allocation, nullptr)) {
        std::print(INIT_ERROR_STRING, "Could not create draw Image");
        return EngineInitError::Vk_CreateDrawImageFailed;
    }

	VkImageViewCreateInfo draw_img_view_info = vkinit::image_view_create_info(_draw_image.image_format, _draw_image.image, VK_IMAGE_ASPECT_COLOR_BIT);
	if (vkCreateImageView(_device, &draw_img_view_info, nullptr, &_draw_image.image_view)) {
        std::print(INIT_ERROR_STRING, "Could not create draw ImageView");
        return EngineInitError::Vk_CreateDrawImageViewFailed;
    }

    // Create Depth Image
    _depth_image.image_format = VK_FORMAT_D32_SFLOAT;
	_depth_image.image_extent = draw_image_extent;

	VkImageUsageFlags depth_image_usages{};
	depth_image_usages |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

	VkImageCreateInfo depth_img_info =
        vkinit::image_create_info(_depth_image.image_format, depth_image_usages, draw_image_extent);

	vmaCreateImage(_allocator, &depth_img_info, &draw_img_alloc_info, &_depth_image.image, &_depth_image.allocation, nullptr);

	VkImageViewCreateInfo depth_view_info = vkinit::image_view_create_info(_depth_image.image_format, _depth_image.image, VK_IMAGE_ASPECT_DEPTH_BIT);

	VK_CHECK(vkCreateImageView(_device, &depth_view_info, nullptr, &_depth_image.image_view));

    // Cleanup
    _main_deletion_queue.push_function([=]() {
		vkDestroyImageView(_device, _draw_image.image_view, nullptr);
		vmaDestroyImage(_allocator, _draw_image.image, _draw_image.allocation);

        vkDestroyImageView(_device, _depth_image.image_view, nullptr);
		vmaDestroyImage(_allocator, _depth_image.image, _depth_image.allocation);
	});

    return std::nullopt;
}

std::optional<EngineInitError> VkEngine::init_commands() {
    
	VkCommandPoolCreateInfo commandPoolInfo =  vkinit::command_pool_create_info(_graphics_queue_family, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
	
	for (int i = 0; i < FRAME_OVERLAP; i++) {

        if (vkCreateCommandPool(_device, &commandPoolInfo, nullptr, &_frames[i]._command_pool)) {
            std::print(INIT_ERROR_STRING, "Could not create CommandPool");
            return EngineInitError::Vk_CreateCommandPoolFailed;
        }

		VkCommandBufferAllocateInfo cmdAllocInfo = vkinit::command_buffer_allocate_info(_frames[i]._command_pool);

		if(vkAllocateCommandBuffers(_device, &cmdAllocInfo, &_frames[i]._main_command_buffer)) {
            std::print(INIT_ERROR_STRING, "Could not create CommandBuffer");
            return EngineInitError::Vk_CreateCommandBufferFailed;
        }
	}

    // Immediate structures
    VK_CHECK(vkCreateCommandPool(_device, &commandPoolInfo, nullptr, &_imm_command_pool));

	VkCommandBufferAllocateInfo cmd_alloc_info = vkinit::command_buffer_allocate_info(_imm_command_pool, 1);
	VK_CHECK(vkAllocateCommandBuffers(_device, &cmd_alloc_info, &_imm_command_buffer));

	_main_deletion_queue.push_function([=]() { 
	    vkDestroyCommandPool(_device, _imm_command_pool, nullptr);
	});

    return std::nullopt;
}

std::optional<EngineInitError> VkEngine::init_sync_structures() {
	// Fence starts signalled so we can wait on it on the first frame
	VkFenceCreateInfo fence_create_info = vkinit::fence_create_info(VK_FENCE_CREATE_SIGNALED_BIT);
	VkSemaphoreCreateInfo semaphore_create_info = vkinit::semaphore_create_info();

	for (int i = 0; i < FRAME_OVERLAP; i++) {
		if(vkCreateFence(_device, &fence_create_info, nullptr, &_frames[i]._render_fence)) {
            std::print(INIT_ERROR_STRING, "Could not create Fence");
            return EngineInitError::Vk_CreateFenceFailed;
        }

		if (vkCreateSemaphore(_device, &semaphore_create_info, nullptr, &_frames[i]._swapchain_ready_semaphore)) {
            std::print(INIT_ERROR_STRING, "Could not swapchain Semaphore");
            return EngineInitError::Vk_CreateSemaphoreFailed;
        }
		if (vkCreateSemaphore(_device, &semaphore_create_info, nullptr, &_frames[i]._render_finished_semaphore)) {
            std::print(INIT_ERROR_STRING, "Could not render Semaphore");
            return EngineInitError::Vk_CreateSemaphoreFailed;
        }
	}

    // Immediate structures
    VK_CHECK(vkCreateFence(_device, &fence_create_info, nullptr, &_imm_fence));

	_main_deletion_queue.push_function([=]() {
        vkDestroyFence(_device, _imm_fence, nullptr);
    });

    return std::nullopt;
}

void VkEngine::init_descriptors() {
    // Create a descriptor pool that holds 10 storage image descriptors
	std::vector<DescriptorAllocator::PoolSizeRatio> sizes =
	{
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1 }
	};
	_global_descriptor_allocator.init_pool(_device, 10, sizes);

    // Create the descriptor layout for our compute shader
    // i.e. 1 descriptor of storage image at binding 0
    DescriptorLayoutBuilder builder;
    builder.add_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
    _draw_image_descriptor_layout = builder.build(_device, VK_SHADER_STAGE_COMPUTE_BIT);

    // Allocate set for that layout
    _draw_image_descriptors = _global_descriptor_allocator.allocate(_device, _draw_image_descriptor_layout);	

    // Write to the descriptor set we allocated (i.e. set our draw image as the required storage image)
	VkDescriptorImageInfo img_info{};
	img_info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	img_info.imageView = _draw_image.image_view;
	
	VkWriteDescriptorSet draw_image_write = {};
	draw_image_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	draw_image_write.pNext = nullptr;
	
	draw_image_write.dstBinding = 0;
	draw_image_write.dstSet = _draw_image_descriptors;
	draw_image_write.descriptorCount = 1;
	draw_image_write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	draw_image_write.pImageInfo = &img_info;

	vkUpdateDescriptorSets(_device, 1, &draw_image_write, 0, nullptr);

    // Cleanup
	_main_deletion_queue.push_function([=]() {
		_global_descriptor_allocator.destroy_pool(_device);

		vkDestroyDescriptorSetLayout(_device, _draw_image_descriptor_layout, nullptr);
	});
}

void VkEngine::init_background_pipelines() {
    // Create pipeline layout
    VkPipelineLayoutCreateInfo compute_layout{};
	compute_layout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	compute_layout.pNext = nullptr;
	compute_layout.pSetLayouts = &_draw_image_descriptor_layout;
	compute_layout.setLayoutCount = 1;

    VkPushConstantRange push_constant{};
    push_constant.offset = 0;
    push_constant.size = sizeof(ComputePushConstants) ;
    push_constant.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    compute_layout.pPushConstantRanges = &push_constant;
    compute_layout.pushConstantRangeCount = 1;

    VkPipelineLayout compute_pipeline_layout;
	VK_CHECK(vkCreatePipelineLayout(_device, &compute_layout, nullptr, &compute_pipeline_layout));

    // Create pipelines
    ComputeEffect gradient;
    gradient.layout = compute_pipeline_layout;
    gradient.name = "gradient";
    gradient.data = {};
    gradient.data.data1 = glm::vec4(1, 0, 0, 1);
    gradient.data.data2 = glm::vec4(0, 0, 1, 1);

    ComputeEffect sky;
    sky.layout = compute_pipeline_layout;
    sky.name = "sky";
    sky.data = {};
    sky.data.data1 = glm::vec4(0.1, 0.2, 0.4, 0.97);

    VkShaderModule gradient_compute_shader;
    VkShaderModule sky_compute_shader;

    if (!vkutil::load_shader_module("../shaders/gradient.comp.spv", _device, &gradient_compute_shader))
    {
        std::print("Error when building the compute shader \n");
        abort();
    }
    if (!vkutil::load_shader_module("../shaders/sky.comp.spv", _device, &sky_compute_shader))
    {
        std::print("Error when building the compute shader \n");
        abort();
    }

	VkPipelineShaderStageCreateInfo stage_info{};
	stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stage_info.pNext = nullptr;
	stage_info.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    stage_info.module = gradient_compute_shader;
	stage_info.pName = "main";

	VkComputePipelineCreateInfo compute_pipeline_createInfo{};
	compute_pipeline_createInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	compute_pipeline_createInfo.pNext = nullptr;
	compute_pipeline_createInfo.layout = compute_pipeline_layout;
	compute_pipeline_createInfo.stage = stage_info;

    // The ony difference between pipelines is the shader module
	VK_CHECK(vkCreateComputePipelines(_device, VK_NULL_HANDLE, 1, &compute_pipeline_createInfo, nullptr, &gradient.pipeline));

    compute_pipeline_createInfo.stage.module = sky_compute_shader;
	VK_CHECK(vkCreateComputePipelines(_device, VK_NULL_HANDLE, 1, &compute_pipeline_createInfo, nullptr, &sky.pipeline));

    // Add the compute effects into the array
    _compute_effects.push_back(gradient);
    _compute_effects.push_back(sky);

    // Cleanup
    vkDestroyShaderModule(_device, gradient_compute_shader, nullptr);
    vkDestroyShaderModule(_device, sky_compute_shader, nullptr);

	_main_deletion_queue.push_function([=]() {
		vkDestroyPipelineLayout(_device, compute_pipeline_layout, nullptr);

		vkDestroyPipeline(_device, gradient.pipeline, nullptr);
        vkDestroyPipeline(_device, sky.pipeline, nullptr);
	});
}

void VkEngine::init_mesh_pipeline() {
    // Load shaders
    VkShaderModule mesh_frag_shader;
	if (!vkutil::load_shader_module("../shaders/mesh.frag.spv", _device, &mesh_frag_shader)) {
		std::print("Error when building the triangle fragment shader module");
	}

	VkShaderModule mesh_vertex_shader;
	if (!vkutil::load_shader_module("../shaders/mesh.vert.spv", _device, &mesh_vertex_shader)) {
		std::print("Error when building the triangle vertex shader module");
	}

    // Create layout
    VkPushConstantRange buffer_range{};
	buffer_range.offset = 0;
	buffer_range.size = sizeof(GPUDrawPushConstants);
	buffer_range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

	VkPipelineLayoutCreateInfo pipeline_layout_info = vkinit::pipeline_layout_create_info();
    pipeline_layout_info.pPushConstantRanges = &buffer_range;
	pipeline_layout_info.pushConstantRangeCount = 1;
	VK_CHECK(vkCreatePipelineLayout(_device, &pipeline_layout_info, nullptr, &_mesh_pipeline_layout));

    // Create pipeline
    PipelineBuilder pipelineBuilder;

	pipelineBuilder._pipeline_layout = _mesh_pipeline_layout;
	pipelineBuilder.set_shaders(mesh_vertex_shader, mesh_frag_shader);
	pipelineBuilder.set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
	pipelineBuilder.set_polygon_mode(VK_POLYGON_MODE_FILL);
	pipelineBuilder.set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
	pipelineBuilder.set_multisampling_none();
	pipelineBuilder.disable_blending();
	pipelineBuilder.enable_depthtest(true, VK_COMPARE_OP_GREATER_OR_EQUAL);
	pipelineBuilder.set_color_attachment_format(_draw_image.image_format);
	pipelineBuilder.set_depth_format(_depth_image.image_format);

	_mesh_pipeline = pipelineBuilder.build_pipeline(_device);

	// Cleanup
	vkDestroyShaderModule(_device, mesh_frag_shader, nullptr);
	vkDestroyShaderModule(_device, mesh_vertex_shader, nullptr);

	_main_deletion_queue.push_function([&]() {
		vkDestroyPipelineLayout(_device, _mesh_pipeline_layout, nullptr);
		vkDestroyPipeline(_device, _mesh_pipeline, nullptr);
	});
}

void VkEngine::init_pipelines() {
    init_background_pipelines();
    init_mesh_pipeline();
}

void VkEngine::init_imgui()
{
    // Create Imgui-exclusice Descriptor pool
	// NOTE: This pool is huge, but copied from demo
	VkDescriptorPoolSize pool_sizes[] = { 
        { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
		{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
		{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
    };

	VkDescriptorPoolCreateInfo pool_info = {};
	pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
	pool_info.maxSets = 1000;
	pool_info.poolSizeCount = (uint32_t)std::size(pool_sizes);
	pool_info.pPoolSizes = pool_sizes;

	VkDescriptorPool imgui_pool;
	VK_CHECK(vkCreateDescriptorPool(_device, &pool_info, nullptr, &imgui_pool));

	// Initialize Imgui library
	ImGui::CreateContext();

	ImGui_ImplSDL3_InitForVulkan(_window);

	ImGui_ImplVulkan_InitInfo vulkan_init_info = {};
	vulkan_init_info.Instance = _instance;
	vulkan_init_info.PhysicalDevice = _chosen_gpu;
	vulkan_init_info.Device = _device;
	vulkan_init_info.Queue = _graphics_queue;
	vulkan_init_info.DescriptorPool = imgui_pool;
	vulkan_init_info.MinImageCount = 3;
	vulkan_init_info.ImageCount = 3;
	vulkan_init_info.UseDynamicRendering = true;

	vulkan_init_info.PipelineRenderingCreateInfo = {.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
	vulkan_init_info.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
    // NOTE: We draw imgui directly into the swapchain. Maybe it should also render to texture later?
	vulkan_init_info.PipelineRenderingCreateInfo.pColorAttachmentFormats = &_swapchain_image_format;

	vulkan_init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

	ImGui_ImplVulkan_Init(&vulkan_init_info);

	ImGui_ImplVulkan_CreateFontsTexture();

	// Cleanup
	_main_deletion_queue.push_function([=]() {
		ImGui_ImplVulkan_Shutdown();
		vkDestroyDescriptorPool(_device, imgui_pool, nullptr);
	});
}

std::optional<EngineInitError> VkEngine::init() {

    if (SDL_Init(SDL_INIT_VIDEO)) {
        std::print(INIT_ERROR_STRING, SDL_GetError());
        return EngineInitError::SDL_InitFailed;
    }

    const SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_VULKAN);

    _window_extent = get_default_window_dims();
    _window = SDL_CreateWindow(APP_NAME, _window_extent.width, _window_extent.height, window_flags);
    if (_window == NULL) {
        std::print(INIT_ERROR_STRING, SDL_GetError());
        return EngineInitError::SDL_CreateWindowFailed;
    }

    init_vulkan();
    init_swapchain();
    init_commands();
    init_sync_structures();
    init_descriptors();
    init_pipelines();
    init_imgui();
    init_mesh_data();

    _is_initialized = true;

    return std::nullopt;
}

void VkEngine::cleanup() {
    if (_is_initialized) {
        vkDeviceWaitIdle(_device);

        for (int i = 0; i < FRAME_OVERLAP; i++) {
			vkDestroyCommandPool(_device, _frames[i]._command_pool, nullptr);

            vkDestroyFence(_device, _frames[i]._render_fence, nullptr);
		    vkDestroySemaphore(_device, _frames[i]._render_finished_semaphore, nullptr);
		    vkDestroySemaphore(_device ,_frames[i]._swapchain_ready_semaphore, nullptr);
		}

        _main_deletion_queue.flush();

        destroy_swapchain();

		vkDestroySurfaceKHR(_instance, _surface, nullptr);
		vkDestroyDevice(_device, nullptr);
		vkb::destroy_debug_utils_messenger(_instance, _debug_messenger);
		vkDestroyInstance(_instance, nullptr);

        SDL_DestroyWindow(_window);
    }
}

void VkEngine::immediate_submit(std::function<void(VkCommandBuffer cmd)>&& function)
{
    VK_CHECK(vkWaitForFences(_device, 1, &_imm_fence, true, 9999999999));
	VK_CHECK(vkResetFences(_device, 1, &_imm_fence));
	
	VkCommandBuffer cmd = _imm_command_buffer;
	VkCommandBufferBeginInfo cmd_begin_info = vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    VK_CHECK(vkResetCommandBuffer(_imm_command_buffer, 0));
	VK_CHECK(vkBeginCommandBuffer(cmd, &cmd_begin_info));
    {
	    function(cmd);
    }
	VK_CHECK(vkEndCommandBuffer(cmd));

	VkCommandBufferSubmitInfo cmd_info = vkinit::command_buffer_submit_info(cmd);
	VkSubmitInfo2 submit = vkinit::submit_info(&cmd_info, nullptr, nullptr);
	VK_CHECK(vkQueueSubmit2(_graphics_queue, 1, &submit, _imm_fence));
}

AllocatedBuffer VkEngine::create_buffer(size_t alloc_size, VkBufferUsageFlags usage, VmaMemoryUsage memory_usage)
{
	VkBufferCreateInfo bufferInfo = {.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
	bufferInfo.pNext = nullptr;
	bufferInfo.size = alloc_size;

	bufferInfo.usage = usage;

	VmaAllocationCreateInfo vmaallocInfo = {};
	vmaallocInfo.usage = memory_usage;
	vmaallocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;
	AllocatedBuffer newBuffer;

	VK_CHECK(vmaCreateBuffer(_allocator, &bufferInfo, &vmaallocInfo, &newBuffer.buffer, &newBuffer.allocation,
		&newBuffer.info));

	return newBuffer;
}

void VkEngine::destroy_buffer(const AllocatedBuffer& buffer)
{
    vmaDestroyBuffer(_allocator, buffer.buffer, buffer.allocation);
}

void VkEngine::init_mesh_data() {
    _test_meshes = load_gltf_meshes(this,"../assets/basicmesh.glb").value();

	// Cleanup
	_main_deletion_queue.push_function([&]() {
        for (auto& mesh : _test_meshes) {
	        destroy_buffer(mesh->mesh_buffers.index_buffer);
	        destroy_buffer(mesh->mesh_buffers.vertex_buffer);
        }
	});
}

GPUMeshBuffers VkEngine::upload_mesh(std::span<uint32_t> indices, std::span<Vertex> vertices)
{
    // Create Mesh buffers
	const size_t vertex_buffer_size = vertices.size() * sizeof(Vertex);
	const size_t index_buffer_size = indices.size() * sizeof(uint32_t);

	GPUMeshBuffers mesh_buffers;

	mesh_buffers.vertex_buffer = create_buffer(
        vertex_buffer_size, 
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		VMA_MEMORY_USAGE_GPU_ONLY
    );

	VkBufferDeviceAddressInfo device_adress_info{ 
        .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .buffer = mesh_buffers.vertex_buffer.buffer
    };
	mesh_buffers.vertex_buffer_address = vkGetBufferDeviceAddress(_device, &device_adress_info);

	mesh_buffers.index_buffer = create_buffer(
        index_buffer_size,
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VMA_MEMORY_USAGE_GPU_ONLY
    );

    // Copy data to mesh buffers
    AllocatedBuffer staging = create_buffer(
        vertex_buffer_size + index_buffer_size,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VMA_MEMORY_USAGE_CPU_ONLY
    );

	void* data = staging.allocation->GetMappedData();

	memcpy(data, vertices.data(), vertex_buffer_size);
	memcpy((char*)data + vertex_buffer_size, indices.data(), index_buffer_size);

    // NOTE: Not very efficient, we are waiting for the GPU command to fully execute before continuing with our CPU side logic.
    //Should be put on a background thread, whose sole job is to execute uploads like this one, and deleting/reusing the staging buffers.
	immediate_submit([&](VkCommandBuffer cmd) {
		VkBufferCopy vertex_copy{ 0 };
		vertex_copy.dstOffset = 0;
		vertex_copy.srcOffset = 0;
		vertex_copy.size = vertex_buffer_size;

		vkCmdCopyBuffer(cmd, staging.buffer, mesh_buffers.vertex_buffer.buffer, 1, &vertex_copy);

		VkBufferCopy index_copy{ 0 };
		index_copy.dstOffset = 0;
		index_copy.srcOffset = vertex_buffer_size;
		index_copy.size = index_buffer_size;

		vkCmdCopyBuffer(cmd, staging.buffer, mesh_buffers.index_buffer.buffer, 1, &index_copy);
	});

	destroy_buffer(staging);

	return mesh_buffers;
}

void VkEngine::run() {
    SDL_Event sdl_event;
    bool should_quit = false;

    while (!should_quit) {
        while (SDL_PollEvent(&sdl_event) != 0) {
            switch (sdl_event.type) {
                case SDL_EVENT_QUIT: {
                    should_quit = true;
                    break;
                }
                case SDL_EVENT_KEY_DOWN: {
                    switch (sdl_event.key.key) {
                        case SDLK_ESCAPE: {
                            should_quit = true;
                            break;
                        }
                        default: {
                            break;
                        }
                    }
                    break;
                }
                case SDL_EVENT_WINDOW_MINIMIZED: {
                    _stop_rendering = true;
                    break;
                }
                case SDL_EVENT_WINDOW_RESTORED: {
                    _stop_rendering = false;
                    break;
                }
                default: {
                    break;
                }
            }

            ImGui_ImplSDL3_ProcessEvent(&sdl_event);
        }

        if (_stop_rendering) {
            // throttle the speed to avoid the endless spinning
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        } else {
            // Setup Imgui rendering
            ImGui_ImplVulkan_NewFrame();
            ImGui_ImplSDL3_NewFrame();
            ImGui::NewFrame();

            if (ImGui::Begin("Compute Effect")) {
                ComputeEffect& selected = _compute_effects[_current_compute_effect];
            
                ImGui::Text("Selected effect: %s", selected.name);
            
                ImGui::SliderInt("Effect Index", &_current_compute_effect, 0, _compute_effects.size() - 1);
            
                ImGui::InputFloat4("data1", (float*) &selected.data.data1);
                ImGui::InputFloat4("data2", (float*) &selected.data.data2);
                ImGui::InputFloat4("data3", (float*) &selected.data.data3);
                ImGui::InputFloat4("data4", (float*) &selected.data.data4);

                ImGui::End();
		    }

            ImGui::Render();

            // Main frame rendering
            draw();
        }
    }
}

void VkEngine::draw_imgui(VkCommandBuffer cmd, VkImageView target_image_view) {
    VkRenderingAttachmentInfo color_attachment = vkinit::rendering_attachment_info(target_image_view, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	VkRenderingInfo rendering_info = vkinit::rendering_info(_swapchain_extent, &color_attachment, nullptr);

	vkCmdBeginRendering(cmd, &rendering_info);

	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);

	vkCmdEndRendering(cmd);
}

void VkEngine::draw_background(VkCommandBuffer cmd) {
    ComputeEffect& effect = _compute_effects[_current_compute_effect];

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, effect.pipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, effect.layout, 0, 1, &_draw_image_descriptors, 0, nullptr);

    vkCmdPushConstants(cmd, effect.layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ComputePushConstants), &effect.data);

    // Divide image extent by compute shader block size
    vkCmdDispatch(cmd, std::ceil(_draw_extent.width / 16.0), std::ceil(_draw_extent.height / 16.0), 1);
}

void VkEngine::draw_geometry(VkCommandBuffer cmd) {
	VkRenderingAttachmentInfo color_attachment = vkinit::rendering_attachment_info(_draw_image.image_view, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    VkRenderingAttachmentInfo depth_attachment = vkinit::depth_attachment_info(_depth_image.image_view);
	VkRenderingInfo render_info = vkinit::rendering_info(_draw_extent, &color_attachment, &depth_attachment);
	vkCmdBeginRendering(cmd, &render_info);

	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _mesh_pipeline);

	VkViewport viewport = {};
	viewport.x = 0;
	viewport.y = 0;
	viewport.width = _draw_extent.width;
	viewport.height = _draw_extent.height;
	viewport.minDepth = 0.f;
	viewport.maxDepth = 1.f;
	vkCmdSetViewport(cmd, 0, 1, &viewport);

	VkRect2D scissor = {};
	scissor.offset.x = 0;
	scissor.offset.y = 0;
	scissor.extent.width = _draw_extent.width;
	scissor.extent.height = _draw_extent.height;
	vkCmdSetScissor(cmd, 0, 1, &scissor);

    MeshAsset& mesh_asset = *_test_meshes[2];

    glm::mat4 view = glm::translate(glm::vec3{ 0, 0, -5 });
    glm::mat4 projection = glm::perspective(glm::radians(70.f), (float)_draw_extent.width / (float)_draw_extent.height, 0.1f, 10000.f);
	// Invert the Y axis so that we match opengl and gltf
	projection[1][1] *= -1;

	GPUDrawPushConstants push_constants;
	push_constants.world_matrix = projection * view;
	push_constants.vertex_buffer = mesh_asset.mesh_buffers.vertex_buffer_address;
	vkCmdPushConstants(cmd, _mesh_pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(GPUDrawPushConstants), &push_constants);

	vkCmdBindIndexBuffer(cmd, mesh_asset.mesh_buffers.index_buffer.buffer, 0, VK_INDEX_TYPE_UINT32);

	vkCmdDrawIndexed(cmd, mesh_asset.surfaces[0].count, 1, mesh_asset.surfaces[0].start_index, 0, 0);

	vkCmdEndRendering(cmd);
}

std::optional<EngineRunError> VkEngine::draw() {
    // Wait until the gpu has finished rendering the last frame on the current index
	if (vkWaitForFences(_device, 1, &get_current_frame()._render_fence, true, seconds_to_nanoseconds(1))) {
        std::print(INIT_ERROR_STRING, "Could not wait on render fence");
        return EngineRunError::Vk_WaitOnFenceFailed;
    }
    get_current_frame()._deletion_queue.flush();
	if (vkResetFences(_device, 1, &get_current_frame()._render_fence)) {
        std::print(INIT_ERROR_STRING, "Could not reset render fence");
        return EngineRunError::Vk_ResetFenceFailed;
    }

    // TODO: Will need to be configurable in the future
    _draw_extent.width = _draw_image.image_extent.width;
	_draw_extent.height = _draw_image.image_extent.height;

    // Request image from the swapchain
	uint32_t swapchain_image_index;
	if (vkAcquireNextImageKHR(_device, _swapchain, seconds_to_nanoseconds(1), get_current_frame()._swapchain_ready_semaphore, nullptr, &swapchain_image_index)) {
        std::print(INIT_ERROR_STRING, "Could not acquire swapchain image");
        return EngineRunError::Vk_ResetFenceFailed;
    }

    // Reset and start command buffer
	VkCommandBuffer cmd = get_current_frame()._main_command_buffer;
	if (vkResetCommandBuffer(cmd, 0)) {
        std::print(INIT_ERROR_STRING, "Could not reset CommandBuffer");
        return EngineRunError::Vk_ResetCommandBufferFailed;
    }
	VkCommandBufferBeginInfo cmdBeginInfo = vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
	if (vkBeginCommandBuffer(cmd, &cmdBeginInfo)) {
        std::print(INIT_ERROR_STRING, "Could not begin CommandBuffer");
        return EngineRunError::Vk_BeginCommandBufferFailed;
    }

    // Prepare draw image to be rendered to (compute not using depth atm)
    vkutil::transition_image(cmd, _draw_image.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

    // To draw with compute, we must be in general format
    draw_background(cmd);

    // Make draw and depth image ready to be used as attachments
    vkutil::transition_image(cmd, _draw_image.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    vkutil::transition_image(cmd, _depth_image.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

    // To draw with graphics pipeline, we must be in color attachment format
    draw_geometry(cmd);

    // Prepare draw image to write into swapchain image, execute copy, prepare swapchain image to present
	vkutil::transition_image(cmd, _draw_image.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
	vkutil::transition_image(cmd, _swapchain_images[swapchain_image_index], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    // Copy Draw image to swapchain
    vkutil::copy_image_to_image(cmd, _draw_image.image, _swapchain_images[swapchain_image_index], _draw_extent, _swapchain_extent);

    // Transition swapchain image layout to Attachment Optimal so we can draw to it
	vkutil::transition_image(cmd, _swapchain_images[swapchain_image_index], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

	draw_imgui(cmd,  _swapchain_image_views[swapchain_image_index]);

	// Transition swapchain image to presentable format
    vkutil::transition_image(cmd, _swapchain_images[swapchain_image_index], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    // End command buffer and submit queue
	if (vkEndCommandBuffer(cmd)) {
        std::print(INIT_ERROR_STRING, "Could not end CommandBuffer");
        return EngineRunError::Vk_EndCommandBufferFailed;
    }

	VkCommandBufferSubmitInfo cmd_info = vkinit::command_buffer_submit_info(cmd);	
	VkSemaphoreSubmitInfo wait_info = vkinit::semaphore_submit_info(
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR, get_current_frame()._swapchain_ready_semaphore);
	VkSemaphoreSubmitInfo signal_info = vkinit::semaphore_submit_info(
        VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, get_current_frame()._render_finished_semaphore);	

	VkSubmitInfo2 submit = vkinit::submit_info(&cmd_info, &signal_info, &wait_info);	

	if (vkQueueSubmit2(_graphics_queue, 1, &submit, get_current_frame()._render_fence)) {
        std::print(INIT_ERROR_STRING, "Could not submit queue");
        return EngineRunError::Vk_QueueSubmitFailed;
    }

    // Present queue
	VkPresentInfoKHR present_info = {};
	present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	present_info.pNext = nullptr;
	present_info.pSwapchains = &_swapchain;
	present_info.swapchainCount = 1;

	present_info.pWaitSemaphores = &get_current_frame()._render_finished_semaphore;
	present_info.waitSemaphoreCount = 1;

	present_info.pImageIndices = &swapchain_image_index;

	if (vkQueuePresentKHR(_graphics_queue, &present_info)) {
        std::print(INIT_ERROR_STRING, "Could not present queue");
        return EngineRunError::Vk_QueuePresentFailed;
    }

	_frame_number++;

    return std::nullopt;
}