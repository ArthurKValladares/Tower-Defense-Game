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

void VkEngine::resize_swapchain() {
    vkDeviceWaitIdle(_device);

	destroy_swapchain();

	int w, h;
	SDL_GetWindowSize(_window, &w, &h);
	_window_extent.width = w;
	_window_extent.height = h;

	create_swapchain(_window_extent.width, _window_extent.height);

	_resize_requested = false;
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
	VkPhysicalDeviceVulkan13Features features13{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES };
	features13.dynamicRendering = true;
	features13.synchronization2 = true;

	VkPhysicalDeviceVulkan12Features features12{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES };
	features12.bufferDeviceAddress = true;
	features12.descriptorIndexing = true;

	vkb::PhysicalDeviceSelector selector{ vkb_instance };
	vkb::Result<vkb::PhysicalDevice> vkb_physical_device_result = selector
		.set_minimum_version(1, 3)
		.set_required_features_13(features13)
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
    // create a descriptor pool
    std::vector<DescriptorAllocator::PoolSizeRatio> sizes = {
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 3 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3 },
    };
    _global_descriptor_allocator.init(_device, 10, sizes);

	_main_deletion_queue.push_function( [&]() {
		_global_descriptor_allocator.destroy_pools(_device);
	});

    {
        DescriptorLayoutBuilder builder;
        builder.add_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
        _draw_image_descriptor_layout = builder.build(_device, VK_SHADER_STAGE_COMPUTE_BIT);
    }

    {
        DescriptorLayoutBuilder builder;
        builder.add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        _gpu_scene_data_descriptor_layout  = builder.build(_device, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
    }

    _main_deletion_queue.push_function([&]() {
        vkDestroyDescriptorSetLayout(_device, _draw_image_descriptor_layout, nullptr);
        vkDestroyDescriptorSetLayout(_device, _gpu_scene_data_descriptor_layout, nullptr);
    });

    _draw_image_descriptors = _global_descriptor_allocator.allocate(_device, _draw_image_descriptor_layout);
    {
        DescriptorWriter writer;	
		writer.write_image(0, _draw_image.image_view, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
        writer.update_set(_device, _draw_image_descriptors);
    }

	for (int i = 0; i < FRAME_OVERLAP; i++) {
		// create a descriptor pool
		std::vector<DescriptorAllocator::PoolSizeRatio> frame_sizes = {
			{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 3 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3 },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3 },
			{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4 },
		};

		_frames[i]._frame_descriptors = DescriptorAllocator{};
		_frames[i]._frame_descriptors.init(_device, 1000, frame_sizes);

		_main_deletion_queue.push_function([&, i]() {
			_frames[i]._frame_descriptors.destroy_pools(_device);
		});
	}
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

void VkEngine::init_pipelines() {
    init_background_pipelines();

    metal_rough_material.build_pipelines(this);
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

void VkEngine::init_camera() {
	main_camera.velocity = glm::vec3(0.f);
	main_camera.position = glm::vec3(30.f, -00.f, -085.f);

	main_camera.pitch = 0;
	main_camera.yaw = 0;
}

std::optional<EngineInitError> VkEngine::init() {

    if (SDL_Init(SDL_INIT_VIDEO)) {
        std::print(INIT_ERROR_STRING, SDL_GetError());
        return EngineInitError::SDL_InitFailed;
    }

    const SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);

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
    init_default_data();
	init_camera();

    _is_initialized = true;

    return std::nullopt;
}

void VkEngine::cleanup() {
    if (_is_initialized) {
        vkDeviceWaitIdle(_device);

		loaded_scenes.clear();

        for (int i = 0; i < FRAME_OVERLAP; i++) {
			vkDestroyCommandPool(_device, _frames[i]._command_pool, nullptr);

            vkDestroyFence(_device, _frames[i]._render_fence, nullptr);
		    vkDestroySemaphore(_device, _frames[i]._render_finished_semaphore, nullptr);
		    vkDestroySemaphore(_device ,_frames[i]._swapchain_ready_semaphore, nullptr);

            _frames[i]._deletion_queue.flush();
		}

        metal_rough_material.clear_resources(_device);

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
    VK_CHECK(vkResetFences(_device, 1, &_imm_fence));
	VK_CHECK(vkResetCommandBuffer(_imm_command_buffer, 0));

	VkCommandBuffer cmd = _imm_command_buffer;
	VkCommandBufferBeginInfo cmd_begin_info = vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
	VK_CHECK(vkBeginCommandBuffer(cmd, &cmd_begin_info));
    {
	    function(cmd);
    }
	VK_CHECK(vkEndCommandBuffer(cmd));

	VkCommandBufferSubmitInfo cmd_info = vkinit::command_buffer_submit_info(cmd);
	VkSubmitInfo2 submit = vkinit::submit_info(&cmd_info, nullptr, nullptr);
	VK_CHECK(vkQueueSubmit2(_graphics_queue, 1, &submit, _imm_fence));

    VK_CHECK(vkWaitForFences(_device, 1, &_imm_fence, true, 9999999999));
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

void VkEngine::init_default_meshes() {
    std::string structure_path = { "../assets/structure.glb" };
    auto structure_file = LoadedGLTF::load_gltf(this,structure_path);

    assert(structure_file.has_value());

    loaded_scenes["structure"] = *structure_file;
}

void VkEngine::init_default_textures() {
// Create flat-colored images
	uint32_t white = glm::packUnorm4x8(glm::vec4(1, 1, 1, 1));
	_default_images._white_image = create_image((void*)&white, VkExtent3D{ 1, 1, 1 }, VK_FORMAT_R8G8B8A8_UNORM,
		VK_IMAGE_USAGE_SAMPLED_BIT);

	uint32_t grey = glm::packUnorm4x8(glm::vec4(0.66f, 0.66f, 0.66f, 1));
	_default_images._grey_image = create_image((void*)&grey, VkExtent3D{ 1, 1, 1 }, VK_FORMAT_R8G8B8A8_UNORM,
		VK_IMAGE_USAGE_SAMPLED_BIT);

	uint32_t black = glm::packUnorm4x8(glm::vec4(0, 0, 0, 0));
	_default_images._black_image = create_image((void*)&black, VkExtent3D{ 1, 1, 1 }, VK_FORMAT_R8G8B8A8_UNORM,
		VK_IMAGE_USAGE_SAMPLED_BIT);

	// Create checkerboard image
	uint32_t magenta = glm::packUnorm4x8(glm::vec4(1, 0, 1, 1));
	std::array<uint32_t, 16 *16 > pixels;
	for (int x = 0; x < 16; x++) {
		for (int y = 0; y < 16; y++) {
			pixels[y*16 + x] = ((x % 2) ^ (y % 2)) ? magenta : black;
		}
	}
	_default_images._error_checkerboard_image = create_image(pixels.data(), VkExtent3D{16, 16, 1}, VK_FORMAT_R8G8B8A8_UNORM,
		VK_IMAGE_USAGE_SAMPLED_BIT);

    // Create samplers
	VkSamplerCreateInfo sampl = {.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};

	sampl.magFilter = VK_FILTER_NEAREST;
	sampl.minFilter = VK_FILTER_NEAREST;

	vkCreateSampler(_device, &sampl, nullptr, &_default_images._sampler_nearest);

	sampl.magFilter = VK_FILTER_LINEAR;
	sampl.minFilter = VK_FILTER_LINEAR;
	vkCreateSampler(_device, &sampl, nullptr, &_default_images._sampler_linear);

    // Cleanup
	_main_deletion_queue.push_function([&](){
		vkDestroySampler(_device, _default_images._sampler_nearest, nullptr);
		vkDestroySampler(_device, _default_images._sampler_linear,nullptr);

		destroy_image(_default_images._white_image);
		destroy_image(_default_images._grey_image);
		destroy_image(_default_images._black_image);
		destroy_image(_default_images._error_checkerboard_image);
	});
}

void VkEngine::init_default_material() {
    GLTFMetallic_Roughness::MaterialResources material_resources;

	material_resources.color_image = _default_images._white_image;
	material_resources.color_sampler = _default_images._sampler_linear;
	material_resources.metal_rough_image = _default_images._white_image;
	material_resources.metal_rough_sampler = _default_images._sampler_linear;

    // Create and write to material buffer
	AllocatedBuffer material_constants =
        create_buffer(sizeof(GLTFMetallic_Roughness::MaterialConstants), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

	GLTFMetallic_Roughness::MaterialConstants* scene_uniform_data =
        (GLTFMetallic_Roughness::MaterialConstants*) material_constants.allocation->GetMappedData();
	scene_uniform_data->color_factors = glm::vec4{1,1,1,1};
	scene_uniform_data->metal_rough_factors = glm::vec4{1,0.5,0,0};

	_main_deletion_queue.push_function([=, this]() {
		destroy_buffer(material_constants);
	});

	material_resources.data_buffer = material_constants.buffer;
	material_resources.data_buffer_offset = 0;

	default_data = metal_rough_material.write_material(
        _device, MaterialPass::MainColor, material_resources, _global_descriptor_allocator);
}

void VkEngine::init_default_data() {
    init_default_textures();
    init_default_material();
	init_default_meshes();
}

void VkEngine::update_scene()
{
	main_camera.update();

	main_draw_context.opaque_surfaces.clear();
	main_draw_context.transparent_surfaces.clear();
	
	scene_data.view = main_camera.get_view_matrix();

	scene_data.proj = glm::perspective(
        glm::radians(70.f), (float)_window_extent.width / (float)_window_extent.height, 0.1f, 10000.f);
	// Invert y axis to match gltf
	scene_data.proj[1][1] *= -1;

	scene_data.view_proj = scene_data.proj * scene_data.view;

	scene_data.ambient_color = glm::vec4(.1f);
	scene_data.sunlight_color = glm::vec4(1.f);
	scene_data.sunlight_direction = glm::vec4(0,1,0.5,1.f);

	loaded_scenes["structure"]->draw(glm::mat4{ 1.f }, main_draw_context);
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

AllocatedImage VkEngine::create_image(VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mip_mapped)
{
	AllocatedImage new_image;
	new_image.image_format = format;
	new_image.image_extent = size;

	VkImageCreateInfo img_info = vkinit::image_create_info(format, usage, size);
	if (mip_mapped) {
		img_info.mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(size.width, size.height)))) + 1;
	}

	VmaAllocationCreateInfo allocinfo = {};
	allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
	allocinfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	VK_CHECK(vmaCreateImage(_allocator, &img_info, &allocinfo, &new_image.image, &new_image.allocation, nullptr));

	VkImageAspectFlags aspectFlag = VK_IMAGE_ASPECT_COLOR_BIT;
	if (format == VK_FORMAT_D32_SFLOAT) {
		aspectFlag = VK_IMAGE_ASPECT_DEPTH_BIT;
	}

	VkImageViewCreateInfo view_info = vkinit::image_view_create_info(format, new_image.image, aspectFlag);
	view_info.subresourceRange.levelCount = img_info.mipLevels;

	VK_CHECK(vkCreateImageView(_device, &view_info, nullptr, &new_image.image_view));

	return new_image;
}

AllocatedImage VkEngine::create_image(void* data, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mip_mapped)
{
	size_t data_size = size.depth * size.width * size.height * 4;
	AllocatedBuffer upload_buffer = create_buffer(data_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

	memcpy(upload_buffer.info.pMappedData, data, data_size);

	AllocatedImage new_image = create_image(size, format, usage | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, mip_mapped);

	immediate_submit([&](VkCommandBuffer cmd) {
		vkutil::transition_image(cmd, new_image.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

		VkBufferImageCopy copy_region = {};
		copy_region.bufferOffset = 0;
		copy_region.bufferRowLength = 0;
		copy_region.bufferImageHeight = 0;

		copy_region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		copy_region.imageSubresource.mipLevel = 0;
		copy_region.imageSubresource.baseArrayLayer = 0;
		copy_region.imageSubresource.layerCount = 1;
		copy_region.imageExtent = size;

		vkCmdCopyBufferToImage(cmd, upload_buffer.buffer, new_image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
			&copy_region);

		if (mip_mapped) {
            vkutil::generate_mipmaps(
				cmd, new_image.image, VkExtent2D{new_image.image_extent.width, new_image.image_extent.height});
        } else {
            vkutil::transition_image(cmd, new_image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        }
    });

	destroy_buffer(upload_buffer);

	return new_image;
}

void VkEngine::destroy_image(const AllocatedImage& img)
{
    vkDestroyImageView(_device, img.image_view, nullptr);
    vmaDestroyImage(_allocator, img.image, img.allocation);
}

void VkEngine::run() {
    SDL_Event sdl_event;
    bool should_quit = false;

    while (!should_quit) {
		auto start = std::chrono::system_clock::now();

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

			main_camera.process_sdl_event(sdl_event);
            ImGui_ImplSDL3_ProcessEvent(&sdl_event);
        }

        if (_stop_rendering) {
            // throttle the speed to avoid the endless spinning
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

		if (_resize_requested) {
			resize_swapchain();
		}

		//
		// Setup Imgui rendering
		//

		ImGui_ImplVulkan_NewFrame();
		ImGui_ImplSDL3_NewFrame();
		ImGui::NewFrame();
		ImGui::Begin("Debug Data");

		ImGui::SliderFloat("Render Scale", &_render_scale, 0.3f, 1.f);

		if (ImGui::TreeNode("Compute Effect")) {
			ComputeEffect& selected = _compute_effects[_current_compute_effect];

			ImGui::Text("Selected effect: %s", selected.name);

			ImGui::SliderInt("Effect Index", &_current_compute_effect, 0, _compute_effects.size() - 1);

			ImGui::InputFloat4("data1", (float*)&selected.data.data1);
			ImGui::InputFloat4("data2", (float*)&selected.data.data2);
			ImGui::InputFloat4("data3", (float*)&selected.data.data3);
			ImGui::InputFloat4("data4", (float*)&selected.data.data4);

			ImGui::TreePop();
		}

		if (ImGui::TreeNode("Stats")) {
			ImGui::Text("frametime %f ms", stats.frametime);
			ImGui::Text("draw time %f ms", stats.mesh_draw_time);
			ImGui::Text("update time %f ms", stats.scene_update_time);
			ImGui::Text("triangles %i", stats.triangle_count);
			ImGui::Text("draws %i", stats.drawcall_count);
			
			ImGui::TreePop();
		}

		ImGui::End();
		ImGui::Render();

		update_scene();

		// Main frame rendering
		draw();

		//get clock again, compare with start clock
		auto end = std::chrono::system_clock::now();
		auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
		stats.frametime = elapsed.count() / 1000.f;
    }
}

void VkEngine::draw_imgui(VkCommandBuffer cmd, VkImageView target_image_view) {
    VkRenderingAttachmentInfo color_attachment = vkinit::attachment_info(target_image_view, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	VkRenderingInfo rendering_info = vkinit::rendering_info(_swapchain_extent, &color_attachment, nullptr);

	vkCmdBeginRendering(cmd, &rendering_info);

	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);

	vkCmdEndRendering(cmd);
}

void VkEngine::draw_geometry(VkCommandBuffer cmd) {
    std::vector<uint32_t> opaque_draws;
    opaque_draws.reserve(main_draw_context.opaque_surfaces.size());

    for (int i = 0; i < main_draw_context.opaque_surfaces.size(); i++) {
       if (is_visible(main_draw_context.opaque_surfaces[i], scene_data.view_proj)) {
            opaque_draws.push_back(i);
       }
    }

    // sort the opaque surfaces by material and mesh
    std::sort(opaque_draws.begin(), opaque_draws.end(), [&](const auto& iA, const auto& iB) {
		const RenderObject& A = main_draw_context.opaque_surfaces[iA];
		const RenderObject& B = main_draw_context.opaque_surfaces[iB];
        if (A.material == B.material) {
            return A.index_buffer < B.index_buffer;
        } else {
            return A.material < B.material;
        }
    });

    //allocate a new uniform buffer for the scene data
    AllocatedBuffer gpu_scene_data_buffer =  create_buffer(sizeof(GPUSceneData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

    //add it to the deletion queue of this frame so it gets deleted once its been used
    get_current_frame()._deletion_queue.push_function([=,this](){
        destroy_buffer(gpu_scene_data_buffer);
    });

    //write the buffer
    GPUSceneData* sceneUniformData = (GPUSceneData*)gpu_scene_data_buffer.allocation->GetMappedData();
    *sceneUniformData = scene_data;

    //create a descriptor set that binds that buffer and update it
    VkDescriptorSet globalDescriptor = get_current_frame()._frame_descriptors.allocate(_device, _gpu_scene_data_descriptor_layout);

	DescriptorWriter writer;
	writer.write_buffer(0, gpu_scene_data_buffer.buffer, sizeof(GPUSceneData), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	writer.update_set(_device, globalDescriptor);

    MaterialPipeline* lastPipeline = nullptr;
    MaterialInstance* lastMaterial = nullptr;
    VkBuffer lastIndexBuffer = VK_NULL_HANDLE;

    auto draw = [&](const RenderObject& r) {
        if (r.material != lastMaterial) {
            lastMaterial = r.material;
            if (r.material->pipeline != lastPipeline) {

                lastPipeline = r.material->pipeline;
                vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, r.material->pipeline->pipeline);
                vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,r.material->pipeline->layout, 0, 1,
                    &globalDescriptor, 0, nullptr);

				VkViewport viewport = {};
				viewport.x = 0;
				viewport.y = 0;
				viewport.width = (float)_window_extent.width;
				viewport.height = (float)_window_extent.height;
				viewport.minDepth = 0.f;
				viewport.maxDepth = 1.f;

				vkCmdSetViewport(cmd, 0, 1, &viewport);

				VkRect2D scissor = {};
				scissor.offset.x = 0;
				scissor.offset.y = 0;
				scissor.extent.width = _window_extent.width;
				scissor.extent.height = _window_extent.height;

				vkCmdSetScissor(cmd, 0, 1, &scissor);
            }

            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, r.material->pipeline->layout, 1, 1,
                &r.material->material_set, 0, nullptr);
        }
        if (r.index_buffer != lastIndexBuffer) {
            lastIndexBuffer = r.index_buffer;
            vkCmdBindIndexBuffer(cmd, r.index_buffer, 0, VK_INDEX_TYPE_UINT32);
        }
        // calculate final mesh matrix
        GPUDrawPushConstants push_constants;
        push_constants.world_matrix = r.transform;
        push_constants.vertex_buffer = r.vertex_buffer_address;

        vkCmdPushConstants(cmd, r.material->pipeline->layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(GPUDrawPushConstants), &push_constants);

        stats.drawcall_count++;
        stats.triangle_count += r.index_count / 3;
        vkCmdDrawIndexed(cmd, r.index_count, 1, r.first_index, 0, 0);
    };

    stats.drawcall_count = 0;
    stats.triangle_count = 0;

    for (auto& r : opaque_draws) {
        draw(main_draw_context.opaque_surfaces[r]);
    }

    for (auto& r : main_draw_context.transparent_surfaces) {
        draw(r);
    }

    // We delete the draw commands now that we processed them
    main_draw_context.opaque_surfaces.clear();
    main_draw_context.transparent_surfaces.clear();
}

void VkEngine::draw_background(VkCommandBuffer cmd) {
    ComputeEffect& effect = _compute_effects[_current_compute_effect];

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, effect.pipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, effect.layout, 0, 1, &_draw_image_descriptors, 0, nullptr);

    vkCmdPushConstants(cmd, effect.layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ComputePushConstants), &effect.data);

    // Divide image extent by compute shader block size
    vkCmdDispatch(cmd, std::ceil(_draw_extent.width / 16.0), std::ceil(_draw_extent.height / 16.0), 1);
}

void VkEngine::draw_main(VkCommandBuffer cmd) {
	draw_background(cmd);

    vkutil::transition_image(cmd, _draw_image.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

	VkRenderingAttachmentInfo color_attachment = vkinit::attachment_info(
		_draw_image.image_view, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	VkRenderingAttachmentInfo depth_attachment = vkinit::depth_attachment_info(
		_depth_image.image_view, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
	VkRenderingInfo renderInfo = vkinit::rendering_info(_window_extent, &color_attachment, &depth_attachment);

	vkCmdBeginRendering(cmd, &renderInfo);

	auto start = std::chrono::system_clock::now();
	draw_geometry(cmd);
	auto end = std::chrono::system_clock::now();
	
	auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

	stats.mesh_draw_time = elapsed.count() / 1000.f;

	vkCmdEndRendering(cmd);
}

void VkEngine::draw() {
    // Wait until the gpu has finished rendering the last frame on the current index
	VK_CHECK(
		vkWaitForFences(_device, 1, &get_current_frame()._render_fence, true, seconds_to_nanoseconds(1)));

    get_current_frame()._deletion_queue.flush();
    get_current_frame()._frame_descriptors.clear_pools(_device);

	uint32_t swapchain_image_index;
    const VkResult e = vkAcquireNextImageKHR(_device, _swapchain, seconds_to_nanoseconds(1), get_current_frame()._swapchain_ready_semaphore, nullptr, &swapchain_image_index);
	if (e == VK_ERROR_OUT_OF_DATE_KHR) {
        _resize_requested = true;
		return;
    }

	// TODO: Will need to be configurable in the future
    _draw_extent.height =
        std::min(_swapchain_extent.height, _draw_image.image_extent.height) * _render_scale;
    _draw_extent.width =
        std::min(_swapchain_extent.width, _draw_image.image_extent.width) * _render_scale;

	VK_CHECK(vkResetFences(_device, 1, &get_current_frame()._render_fence));

    // Reset and start command buffer
	VkCommandBuffer cmd = get_current_frame()._main_command_buffer;
	VK_CHECK(vkResetCommandBuffer(cmd, 0));

	VkCommandBufferBeginInfo cmdBeginInfo = vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
	VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

    // Make draw and depth image ready to be used as attachments
    vkutil::transition_image(cmd, _draw_image.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
    vkutil::transition_image(cmd, _depth_image.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

    draw_main(cmd);

	// Transtion the draw image and the swapchain image into their correct transfer layouts
	vkutil::transition_image(cmd, _draw_image.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
	vkutil::transition_image(cmd, _swapchain_images[swapchain_image_index], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

	VkExtent2D extent;
	extent.height = _window_extent.height;
	extent.width = _window_extent.width;

	// Execute a copy from the draw image into the swapchain
    vkutil::copy_image_to_image(
		cmd, _draw_image.image, _swapchain_images[swapchain_image_index], _draw_extent, _swapchain_extent);

	// Set swapchain image layout to Attachment Optimal so we can draw it
	vkutil::transition_image(
		cmd, _swapchain_images[swapchain_image_index], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

	// Draw imgui into the swapchain image
	draw_imgui(cmd, _swapchain_image_views[swapchain_image_index]);

	// Set swapchain image layout to Present so we can draw it
	vkutil::transition_image(
		cmd, _swapchain_images[swapchain_image_index], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

	//finalize the command buffer (we can no longer add commands, but it can now be executed)
	VK_CHECK(vkEndCommandBuffer(cmd));

	//prepare the submission to the queue. 
	//we want to wait on the _presentSemaphore, as that semaphore is signaled when the swapchain is ready
	//we will signal the _renderSemaphore, to signal that rendering has finished
	VkCommandBufferSubmitInfo cmdinfo = vkinit::command_buffer_submit_info(cmd);
	VkSemaphoreSubmitInfo waitInfo = vkinit::semaphore_submit_info(
		VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR, get_current_frame()._swapchain_ready_semaphore);
	VkSemaphoreSubmitInfo signalInfo = vkinit::semaphore_submit_info(
		VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, get_current_frame()._render_finished_semaphore);
	VkSubmitInfo2 submit = vkinit::submit_info(&cmdinfo, &signalInfo, &waitInfo);

	// Submit command buffer to the queue and execute it.
	// _render_fence will now block until the graphic commands finish execution
	VK_CHECK(vkQueueSubmit2(_graphics_queue, 1, &submit, get_current_frame()._render_fence));

	// Prepare present
	// this will put the image we just rendered to into the visible window.
	// we want to wait on the _renderSemaphore for that, 
	// as its necessary that drawing commands have finished before the image is displayed to the user
	VkPresentInfoKHR presentInfo = vkinit::present_info();
	presentInfo.pSwapchains = &_swapchain;
	presentInfo.swapchainCount = 1;
	presentInfo.pWaitSemaphores = &get_current_frame()._render_finished_semaphore;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pImageIndices = &swapchain_image_index;

	VkResult presentResult = vkQueuePresentKHR(_graphics_queue, &presentInfo);
	if (e == VK_ERROR_OUT_OF_DATE_KHR) {
        _resize_requested = true;
        return;
	}

	_frame_number++;
}