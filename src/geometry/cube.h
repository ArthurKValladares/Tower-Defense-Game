#pragma once

#include "../renderer/vk_renderable.h"

#include <glm/gtx/quaternion.hpp>

// TODO: MAybe should just inherit from MeshNode instead.
struct Cube {
    Cube(VkEngine* engine, std::string name,
        glm::vec3 translate = glm::vec3(0.0), glm::quat rotation = glm::quat(), glm::vec3 scale = glm::vec3(1.0),
        glm::vec4 color = glm::vec4 { 1.f });
    ~Cube();

    void draw(const glm::mat4& top_matrix, DrawContext& ctx);

private:
    VkEngine* creator;
    MeshNode mesh_node;
    AllocatedBuffer material_data_buffer;
};