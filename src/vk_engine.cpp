#include "defs.h"
#include "vk_engine.h"

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
        return EngineInitError::VK_SwapchainInitFailed;
    }
    vkb::Swapchain vkb_swapchain = vkb_swapchain_result.value();

	_swapchain_extent = vkb_swapchain.extent;
	_swapchain = vkb_swapchain.swapchain;
    if (!vkb_swapchain.get_images().has_value()) {
        std::print(INIT_ERROR_STRING, "Could not get Swapchain Images with vk-bootstrap");
        return EngineInitError::VK_SwapchainImagesInitFailed;
    }
	_swapchain_images = vkb_swapchain.get_images().value();
    if (!vkb_swapchain.get_images().has_value()) {
        std::print(INIT_ERROR_STRING, "Could not get Swapchain Images Views with vk-bootstrap");
        return EngineInitError::VK_SwapchainImageViewsInitFailed;
    }
	_swapchain_image_views = vkb_swapchain.get_image_views().value();

    return std::nullopt;
}

void VkEngine::destroy_swapchain() {
    //vkDestroySwapchainKHR(_device, _swapchain, nullptr);

	for (int i = 0; i < _swapchain_image_views.size(); i++) {
		//vkDestroyImageView(_device, _swapchain_image_views[i], nullptr);
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
        return EngineInitError::VK_DeviceInitFailed;
    }
    vkb::PhysicalDevice vkb_physical_device = vkb_physical_device_result.value();

	vkb::DeviceBuilder device_builder{ vkb_physical_device };
	vkb::Result<vkb::Device> vkb_device_result = device_builder.build();

    if (!vkb_device_result.has_value()) {
        std::print(INIT_ERROR_STRING, "Could not initialize device with vk-bootstrap");
        return EngineInitError::VK_DeviceInitFailed;
    }
    vkb::Device vkb_device = vkb_device_result.value();

	_device = vkb_device.device;
	_chosen_gpu = vkb_physical_device.physical_device;

    return std::nullopt;
}

std::optional<EngineInitError> VkEngine::init_swapchain() {
    return create_swapchain(_window_extent.width, _window_extent.height);;
}

std::optional<EngineInitError> VkEngine::init_commands() {
    return std::nullopt;
}

std::optional<EngineInitError> VkEngine::init_sync_structures() {
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
        destroy_swapchain();

		//vkDestroySurfaceKHR(_instance, _surface, nullptr);
		//vkDestroyDevice(_device, nullptr);
		vkb::destroy_debug_utils_messenger(_instance, _debug_messenger);
		//vkDestroyInstance(_instance, nullptr);

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

void VkEngine::draw() {

}

// TODO: For some weird reason, VkDestroy* functions are unresolved.