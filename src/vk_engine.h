#pragma once

#include <optional>
#include <vector>
#include <deque>
#include <functional>

#include "vk_types.h"

#define FRAME_OVERLAP 2

enum class EngineInitError {
    SDL_InitFailed,
    SDL_CreateWindowFailed,
    SDL_MaximizeFailed,
    SDL_GetSizeFailed,
    SDL_VulkanInitFailed,
    VK_InstanceInitFailed,
    Vk_DeviceInitFailed,
    Vk_SwapchainInitFailed,
    Vk_SwapchainImagesInitFailed,
    Vk_SwapchainImageViewsInitFailed,
    Vk_GraphicsQueueInitFailed,
    Vk_GraphicsQueueFamilyInitFailed,
    Vk_CreateCommandPoolFailed,
    Vk_CreateCommandBufferFailed,
    Vk_CreateFenceFailed,
    Vk_CreateSemaphoreFailed,
    Vk_CreateDrawImageFailed,
    Vk_CreateDrawImageViewFailed,
};

enum class EngineRunError {
    Vk_WaitOnFenceFailed,
    Vk_ResetFenceFailed,
    Vk_ResetCommandBufferFailed,
    Vk_BeginCommandBufferFailed,
    Vk_EndCommandBufferFailed,
    Vk_QueueSubmitFailed,
    Vk_QueuePresentFailed,
};

struct DeletionQueue
{
    // TODO: Better implementation would store arrays of vulkan handles of various types such as VkImage/VkBuffer/etc
	std::deque<std::function<void()>> deletors;

	void push_function(std::function<void()>&& function) {
		deletors.push_back(function);
	}

	void flush() {
		// Delete in FILO order
		for (auto it = deletors.rbegin(); it != deletors.rend(); it++) {
			(*it)();
		}

		deletors.clear();
	}
};

struct FrameData {
	VkCommandPool _command_pool;
	VkCommandBuffer _main_command_buffer;

    VkSemaphore _swapchain_ready_semaphore, _render_finished_semaphore;
	VkFence _render_fence;

    DeletionQueue _deletion_queue;
};

struct VkEngine {

    std::optional<EngineInitError> init();
    
    void run();

    void cleanup();

private:
    std::optional<EngineInitError> create_swapchain(uint32_t width, uint32_t height);
    void destroy_swapchain();

    std::optional<EngineInitError> init_vulkan();
	std::optional<EngineInitError> init_swapchain();
	std::optional<EngineInitError> init_commands();
	std::optional<EngineInitError> init_sync_structures();

    std::optional<EngineRunError> draw();

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

    // Memory Allocator
    VmaAllocator _allocator;

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

    // Draw data
	AllocatedImage _draw_image;
	VkExtent2D _draw_extent;

    // Window Data
    VkExtent2D _window_extent = { 0 , 0 };
    struct SDL_Window* _window = nullptr;

    DeletionQueue _main_deletion_queue;
};

// TODO: Result type instead of Optional, easy switch
// With an abstraction for the `if (err) {print; return;}` case

// TODO: Idead, VkCommandBuffer state-machine abstraction
// TODO: 3 queue families, one for drawing the frame, one for async compute, the other for data transfer.
// TODO: Multi-threaded CommandBuffer recording, submission in background thread