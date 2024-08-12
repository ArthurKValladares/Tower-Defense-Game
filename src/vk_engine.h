#pragma once

#include <optional>

#include "vk_types.h"

enum class EngineInitError {
    SDL_InitFailed,
    SDL_CreateWindowFailed,
    SDL_MaximizeFailed,
    SDL_GetSizeFailed,
};

struct VkEngine {

    std::optional<EngineInitError> init();
    
    void run();

    void cleanup();

private:
    void draw();

    bool _is_initialized = false;

    VkExtent2D _window_extent{ 0 , 0 };
    struct SDL_Window* _window = nullptr;
};