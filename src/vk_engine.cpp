#include "vk_engine.h"

#include <SDL3/SDL.h>
#include <print>

#define INIT_ERROR_STRING "Engine init failed with code {}"
std::optional<EngineInitError> VkEngine::init() {

    if (SDL_Init(SDL_INIT_VIDEO)) {
        std::print(INIT_ERROR_STRING, SDL_GetError());
        return EngineInitError::SDL_InitFailed;
    }

    const SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_VULKAN);

    _window = SDL_CreateWindow("TD Game", _window_extent.width, _window_extent.height, window_flags);
    if (_window == NULL) {
        std::print(INIT_ERROR_STRING, SDL_GetError());
        return EngineInitError::SDL_CreateWindowFailed;
    }

    if(SDL_MaximizeWindow(_window)) {
        std::print(INIT_ERROR_STRING, SDL_GetError());
        return EngineInitError::SDL_MaximizeFailed;
    }

    int new_width, new_height;
    if (SDL_GetWindowSize(_window, &new_width, &new_height)) {
        std::print(INIT_ERROR_STRING, SDL_GetError());
        return EngineInitError::SDL_GetSizeFailed;
    }
    _window_extent.width = static_cast<uint32_t>(new_width);
    _window_extent.height = static_cast<uint32_t>(new_height);

    _is_initialized = true;

    return std::nullopt;
}

void VkEngine::cleanup() {
    if (_is_initialized) {
        SDL_DestroyWindow(_window);
    }
}

void VkEngine::run() {
    SDL_Event sdl_event;
    bool should_quit = false;

    while (!should_quit) {
        while (SDL_PollEvent(&sdl_event) != 0) {
            if (sdl_event.type == SDL_EVENT_QUIT) {
                should_quit = true;
            }
        }

        draw();
    }
}

void VkEngine::draw() {

}