#include "vk_engine.h"

#include <SDL3/SDL.h>
#include <print>

namespace {
    VkExtent2D get_default_window_dims() {
        // TODO: platform-specfic code to get reasonable defaults.
        return {1700 , 900};
    }
};

#define INIT_ERROR_STRING "Engine init failed with code: {}\n"
std::optional<EngineInitError> VkEngine::init() {

    if (SDL_Init(SDL_INIT_VIDEO)) {
        std::print(INIT_ERROR_STRING, SDL_GetError());
        return EngineInitError::SDL_InitFailed;
    }

    const SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_VULKAN);

    _window_extent = get_default_window_dims();
    _window = SDL_CreateWindow("TD Game", _window_extent.width, _window_extent.height, window_flags);
    if (_window == NULL) {
        std::print(INIT_ERROR_STRING, SDL_GetError());
        return EngineInitError::SDL_CreateWindowFailed;
    }

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
                default: {
                    break;
                }
            }
            if (sdl_event.type == SDL_EVENT_QUIT) {
                should_quit = true;
            }
        }

        draw();
    }
}

void VkEngine::draw() {

}