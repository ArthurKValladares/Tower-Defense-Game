#pragma once

#include "../renderer/vk_renderable.h"

#include <glm/gtx/quaternion.hpp>

struct Cube {
    Cube(VkEngine* engine, std::string name,
        glm::vec3 translate = glm::vec3(0.0), glm::quat rotation = glm::quat(), glm::vec3 scale = glm::vec3(1.0));

private:
    MeshNode mesh_node;
};