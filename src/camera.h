#pragma once

#include "vk_types.h"
#include <SDL3/SDL_events.h>

struct Camera {
    glm::vec3 velocity;
    glm::vec3 position;

    float pitch { 0.f };
    float yaw { 0.f };

    glm::mat4 get_view_matrix();
    glm::mat4 get_rotation_matrix();

    void process_sdl_event(SDL_Event& e);

    void update();
};