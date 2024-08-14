#pragma once

#include <optional>
#include <vector>

#include "vk_types.h"

#define FRAME_OVERLAP 2

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
    VK_GraphicsQueueInitFailed,
    VK_GraphicsQueueFamilyInitFailed,
    VK_CreateCommandPoolFailed,
    VK_CreateCommandBufferFailed,
};

struct FrameData {
	VkCommandPool _command_pool;
	VkCommandBuffer _main_command_buffer;
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

    FrameData& get_current_frame() {
        return _frames[_frame_number % FRAME_OVERLAP];
    };

private:
    // Engine Data
    bool _is_initialized = false;
    uint64_t _frame_number = 0;
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

    // GPU Command data
    FrameData _frames[FRAME_OVERLAP];
	VkQueue _graphics_queue;
	uint32_t _graphics_queue_family;

    // Window Data
    VkExtent2D _window_extent = { 0 , 0 };
    struct SDL_Window* _window = nullptr;
};

// TODO: Idead, VkCommandBuffer state-machine abstraction
// TODO: 3 queue families, one for drawing the frame, one for async compute, the other for data transfer.
// TODO: Multi-threaded CommandBuffer recording, submission in background thread