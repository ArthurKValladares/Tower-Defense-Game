#include "defs.h"
#include "vk_engine.h"
#include "vk_initializers.h"
#include "vk_image.h"

#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <VkBootstrap.h>

#include <print>
#include <thread>
#include <chrono>

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

    _main_deletion_queue.push_function([&]() {
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

	_main_deletion_queue.push_function([=]() {
		vkDestroyImageView(_device, _draw_image.image_view, nullptr);
		vmaDestroyImage(_allocator, _draw_image.image, _draw_image.allocation);
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

    return std::nullopt;
}

std::optional<EngineInitError> VkEngine::init_sync_structures() {
	// Fence starts signalled so we can wait on it on the first frame
	VkFenceCreateInfo fenceCreateInfo = vkinit::fence_create_info(VK_FENCE_CREATE_SIGNALED_BIT);
	VkSemaphoreCreateInfo semaphoreCreateInfo = vkinit::semaphore_create_info();

	for (int i = 0; i < FRAME_OVERLAP; i++) {
		if(vkCreateFence(_device, &fenceCreateInfo, nullptr, &_frames[i]._render_fence)) {
            std::print(INIT_ERROR_STRING, "Could not create Fence");
            return EngineInitError::Vk_CreateFenceFailed;
        }

		if (vkCreateSemaphore(_device, &semaphoreCreateInfo, nullptr, &_frames[i]._swapchain_ready_semaphore)) {
            std::print(INIT_ERROR_STRING, "Could not swapchain Semaphore");
            return EngineInitError::Vk_CreateSemaphoreFailed;
        }
		if (vkCreateSemaphore(_device, &semaphoreCreateInfo, nullptr, &_frames[i]._render_finished_semaphore)) {
            std::print(INIT_ERROR_STRING, "Could not render Semaphore");
            return EngineInitError::Vk_CreateSemaphoreFailed;
        }
	}

    return std::nullopt;
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
            if (sdl_event.type == SDL_EVENT_QUIT) {
                should_quit = true;
            }
        }

        if (_stop_rendering) {
            // throttle the speed to avoid the endless spinning
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        } else {
            draw();
        }
    }
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

    // Prepare draw image to be rendered to
    vkutil::transition_image(cmd, _draw_image.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

    // Rendering commands
    {
	    VkClearColorValue clearValue;
	    float flash = std::abs(std::sin(_frame_number / 120.f));
	    clearValue = { { 0.0f, 0.0f, flash, 1.0f } };

	    VkImageSubresourceRange clearRange = vkinit::image_subresource_range(VK_IMAGE_ASPECT_COLOR_BIT);

	    vkCmdClearColorImage(cmd, _draw_image.image, VK_IMAGE_LAYOUT_GENERAL, &clearValue, 1, &clearRange);
    }

    // Prepare draw image to write into swapchain image, execute copy, prepare swapchain image to present
	vkutil::transition_image(cmd, _draw_image.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
	vkutil::transition_image(cmd, _swapchain_images[swapchain_image_index], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    vkutil::copy_image_to_image(cmd, _draw_image.image, _swapchain_images[swapchain_image_index], _draw_extent, _swapchain_extent);

    vkutil::transition_image(cmd, _swapchain_images[swapchain_image_index], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

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