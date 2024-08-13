#pragma once

#include <optional>
#include <vector>

#include "vk_types.h"

enum class EngineInitError {
    SDL_InitFailed,
    SDL_CreateWindowFailed,
    SDL_MaximizeFailed,
    SDL_GetSizeFailed,
    SDL_VulkanInitFailed,
    VK_InstanceInitFailed,
    VK_DeviceInitFailed,
    VK_SwapchainInitFailed,
    VK_SwapchainImagesInitFailed,
    VK_SwapchainImageViewsInitFailed,
};

struct VkEngine {

    std::optional<EngineInitError> init();
    
    void run();

    void cleanup();

private:
    void draw();

    std::optional<EngineInitError> create_swapchain(uint32_t width, uint32_t height);
    void destroy_swapchain();

    std::optional<EngineInitError> init_vulkan();
	std::optional<EngineInitError> init_swapchain();
	std::optional<EngineInitError> init_commands();
	std::optional<EngineInitError> init_sync_structures();

    // Engine Data
    bool _is_initialized = false;
    uint64_t _frame_numer = 0;
    bool _stop_rendering = false;

    // Vulkan Device Data
    VkInstance _instance;
	VkDebugUtilsMessengerEXT _debug_messenger;
	VkPhysicalDevice _chosen_gpu;
	VkDevice _device;
	VkSurfaceKHR _surface;

    // Vulkan Swapchain Data
    VkSwapchainKHR _swapchain;
	VkFormat _swapchain_image_format;
	std::vector<VkImage> _swapchain_images;
	std::vector<VkImageView> _swapchain_image_views;
	VkExtent2D _swapchain_extent;

    // Window Data
    VkExtent2D _window_extent = { 0 , 0 };
    struct SDL_Window* _window = nullptr;
};