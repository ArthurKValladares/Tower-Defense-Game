#pragma once

#include "vk_types.h"
#include <SDL3/SDL_events.h>

struct PerspectiveCamera {
    glm::vec3 velocity;
    glm::vec3 position;

    float fov { 70.f };
    float pitch { 0.f };
    float yaw { 0.f };

    glm::mat4 get_view_matrix();
    glm::mat4 get_proj_matrix(float apect_ratio);

    void process_sdl_event(SDL_Event& e);

    void update();

private:
    glm::mat4 get_rotation_matrix();
};

struct OrthographicCamera {
    glm::vec3 velocity;
    glm::vec3 position;

    // TODO: Hook up viewing angle
    glm::vec3 viewing_angle;

    float x_half_size = { 100.0 };
    float y_half_size = { 100.0 };
    float z_half_size = { 100.0 };

    glm::mat4 get_view_matrix();
    glm::mat4 get_proj_matrix();

    void process_sdl_event(SDL_Event& e);

    void update();
};