#pragma once

#include <optional>
#include <vector>
#include <deque>
#include <functional>
#include <span>

#include "vk_types.h"
#include "vk_descriptors.h"
#include "vk_material.h"
#include "vk_gltf_material.h"
#include "vk_renderable.h"
#include "camera.h"

#include "../geometry/cube.h"
#include "../map_editor/map.h"

#include <glm/vec4.hpp>

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

struct ComputePushConstants {
	glm::vec4 data1;
	glm::vec4 data2;
	glm::vec4 data3;
	glm::vec4 data4;
};

struct GPUSceneData {
    glm::mat4 view;
    glm::mat4 proj;
    glm::mat4 view_proj;
    glm::vec4 ambient_color;
    glm::vec4 sunlight_direction;
    glm::vec4 sunlight_color;
};

struct ComputeEffect {
    const char* name;

	VkPipeline pipeline;
	VkPipelineLayout layout;

	ComputePushConstants data;
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

    DescriptorAllocator _frame_descriptors;
};

struct DefaultImages {
    AllocatedImage _white_image;
	AllocatedImage _black_image;
	AllocatedImage _grey_image;
	AllocatedImage _error_checkerboard_image;

    VkSampler _sampler_linear;
	VkSampler _sampler_nearest;
};

struct EngineStats {
    float frametime;
    int triangle_count;
    int drawcall_count;
    float scene_update_time;
    float mesh_draw_time;
};

struct VkEngine {

    std::optional<EngineInitError> init();
    void run();
    void cleanup();

    VkDevice vk_device() { return _device; }
    
    GPUMeshBuffers upload_mesh(std::span<uint32_t> indices, std::span<Vertex> vertices);

    AllocatedImage create_image(VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mip_mapped = false);
    AllocatedImage create_image(void* data, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mip_mapped = false);
    void destroy_image(const AllocatedImage& img);

    void destroy_buffer(const AllocatedBuffer& buffer);

private:
    std::optional<EngineInitError> create_swapchain(uint32_t width, uint32_t height);
    void resize_swapchain();
    void destroy_swapchain();

    std::optional<EngineInitError> init_vulkan();
	std::optional<EngineInitError> init_swapchain();
	std::optional<EngineInitError> init_commands();
	std::optional<EngineInitError> init_sync_structures();
    void init_descriptors();
    void init_pipelines();
	void init_background_pipelines();
    void init_imgui();

    void draw_imgui(VkCommandBuffer cmd, VkImageView target_image_view);
    void draw_background(VkCommandBuffer cmd);
    void draw_geometry(VkCommandBuffer cmd);

    void draw_main(VkCommandBuffer cmd);
    void draw();

    FrameData& get_current_frame() {
        return _frames[_frame_number % FRAME_OVERLAP];
    };

    void immediate_submit(std::function<void(VkCommandBuffer cmd)>&& function);
    AllocatedBuffer create_buffer(size_t alloc_size, VkBufferUsageFlags usage, VmaMemoryUsage memory_usage);

    void init_default_data();
    void init_default_meshes();
    void init_default_textures();
    void init_default_material();
    void init_camera();

    void update_scene();

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
    bool _resize_requested;
    VkSwapchainKHR _swapchain;
	VkFormat _swapchain_image_format;
	std::vector<VkImage> _swapchain_images;
	std::vector<VkImageView> _swapchain_image_views;
	VkExtent2D _swapchain_extent;

    // GPU Command data
    FrameData _frames[FRAME_OVERLAP];
	VkQueue _graphics_queue;
	uint32_t _graphics_queue_family;

    // Descriptor data
    DescriptorAllocator _global_descriptor_allocator;

	VkDescriptorSet _draw_image_descriptors;
	VkDescriptorSetLayout _draw_image_descriptor_layout;

    GPUSceneData scene_data;
    VkDescriptorSetLayout _gpu_scene_data_descriptor_layout;

    // Pipeline data
    std::vector<ComputeEffect> _compute_effects;
    int _current_compute_effect{0};

    // Mesh data
    std::unordered_map<std::string, std::shared_ptr<LoadedGLTF>> loaded_scenes;
    Map map;

    DrawContext main_draw_context;

    // Immediate submit data
    VkFence _imm_fence;
    VkCommandBuffer _imm_command_buffer;
    VkCommandPool _imm_command_pool;

    // Images
    DefaultImages _default_images;

    // Material data
    MaterialInstance default_data;
    GLTFMetallic_Roughness metal_rough_material;
    FlatColorMaterial flat_color_material;

    // Draw data
	AllocatedImage _draw_image;
    AllocatedImage _depth_image;
	VkExtent2D _draw_extent;
    float _render_scale = 1.f;
    bool draw_wireframe = false;
    
    // Window Data
    VkExtent2D _window_extent = { 0 , 0 };
    struct SDL_Window* _window = nullptr;

    DeletionQueue _main_deletion_queue;

    EngineStats stats;

    bool use_ortho_camera = true;
    PerspectiveCamera main_camera;
    OrthographicCamera ortho_camera;

    // TODO: Too many friend classes
    friend class GLTFMetallic_Roughness;
    friend class FlatColorMaterial;
    friend class LoadedGLTF;
    friend class Cube;
};

// TODO: Instead of all the optional stuff that is vbloating the code, just print an error and abort,
// Like VK_CHECK - which I should use more

// TODO: Idead, VkCommandBuffer state-machine abstraction
// TODO: 3 queue families, one for drawing the frame, one for async compute, the other for data transfer.
// TODO: Multi-threaded CommandBuffer recording, submission in background thread

// TODO: I don't quite like the per-frame descriptor stuff, remove soon `_gpu_scene_data_descriptor_layout`