#include "camera.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/transform.hpp>
#include <glm/gtx/quaternion.hpp>

void PerspectiveCamera::update(float dt)
{
    const glm::mat4 camera_rotation = get_rotation_matrix();
    position += glm::vec3(camera_rotation * glm::vec4(velocity * dt, 0.f));
}

void PerspectiveCamera::process_sdl_event(SDL_Event& e)
{
    if (e.type == SDL_EVENT_KEY_DOWN) {
        if (e.key.key == SDLK_W) { velocity.z = -1; }
        if (e.key.key == SDLK_S) { velocity.z = 1; }
        if (e.key.key == SDLK_A) { velocity.x = -1; }
        if (e.key.key == SDLK_D) { velocity.x = 1; }
        if (e.key.key == SDLK_SPACE) { velocity.y = 1; }
        if (e.key.key == SDLK_LSHIFT) { velocity.y = -1; }
    }

    if (e.type == SDL_EVENT_KEY_UP) {
        if (e.key.key == SDLK_W) { velocity.z = 0; }
        if (e.key.key == SDLK_S) { velocity.z = 0; }
        if (e.key.key == SDLK_A) { velocity.x = 0; }
        if (e.key.key == SDLK_D) { velocity.x = 0; }
        if (e.key.key == SDLK_SPACE) { velocity.y = 0; }
        if (e.key.key == SDLK_LSHIFT) { velocity.y = 0; }
    }

    if (e.type == SDL_EVENT_MOUSE_MOTION) {
        yaw += (float)e.motion.xrel / 200.f;
        pitch -= (float)e.motion.yrel / 200.f;
    }
}

glm::mat4 PerspectiveCamera::get_view_matrix()
{
    const glm::mat4 camera_translation = glm::translate(glm::mat4(1.f), position);
    const glm::mat4 camera_rotation = get_rotation_matrix();
    // To create a correct model view, we need to move the world in opposite
    // direction to the camera
    return glm::inverse(camera_translation * camera_rotation);
}

glm::mat4 PerspectiveCamera::get_proj_matrix(float apect_ratio)
{
    glm::mat4 proj = glm::perspective(
        glm::radians(fov), apect_ratio, 10000.f, 0.1f);
    proj[1][1] *= -1;
    return proj;
}

glm::mat4 PerspectiveCamera::get_rotation_matrix()
{
    const glm::quat pitch_rotation = glm::angleAxis(pitch, glm::vec3 { 1.f, 0.f, 0.f });
    const glm::quat yaw_rotation = glm::angleAxis(yaw, glm::vec3 { 0.f, -1.f, 0.f });

    return glm::toMat4(yaw_rotation) * glm::toMat4(pitch_rotation);
}

void OrthographicCamera::update(float dt)
{
    const glm::mat4 camera_rotation = get_rotation_matrix();
    position += glm::vec3(camera_rotation * glm::vec4(velocity * dt, 0.f));
}

void OrthographicCamera::process_sdl_event(SDL_Event& e)
{
    if (e.type == SDL_EVENT_KEY_DOWN) {
        if (e.key.key == SDLK_W) { velocity.y = 1; }
        if (e.key.key == SDLK_S) { velocity.y = -1; }
        if (e.key.key == SDLK_A) { velocity.x = -1; }
        if (e.key.key == SDLK_D) { velocity.x = 1; }
    }

    if (e.type == SDL_EVENT_KEY_UP) {
        if (e.key.key == SDLK_W) { velocity.y = 0; }
        if (e.key.key == SDLK_S) { velocity.y = 0; }
        if (e.key.key == SDLK_A) { velocity.x = 0; }
        if (e.key.key == SDLK_D) { velocity.x = 0; }
    }
}

glm::mat4 OrthographicCamera::get_view_matrix()
{
    const glm::mat4 camera_translation = glm::translate(glm::mat4(1.f), position);
    const glm::mat4 camera_rotation = get_rotation_matrix();

    // To create a correct model view, we need to move the world in opposite
    // direction to the camera
    return glm::inverse(camera_translation * camera_rotation);
}

glm::mat4 OrthographicCamera::get_proj_matrix()
{
    glm::mat4 proj = glm::ortho(-half_sizes.x, half_sizes.x, -half_sizes.y, half_sizes.y, -half_sizes.z, half_sizes.z);
    proj[1][1] *= -1;
    return proj;
}


glm::mat4 OrthographicCamera::get_rotation_matrix()
{
    const glm::quat pitch_rotation = glm::angleAxis(glm::radians(pitch), glm::vec3 { 1.f, 0.f, 0.f });
    const glm::quat yaw_rotation = glm::angleAxis(glm::radians(yaw), glm::vec3 { 0.f, -1.f, 0.f });

    return glm::toMat4(yaw_rotation) * glm::toMat4(pitch_rotation);
}