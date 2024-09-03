#pragma once

#include "../renderer/vk_renderable.h"

struct Cube {
    Cube(glm::vec3 translate = glm::vec3(0.0), float scale = 1.0);

private:
    MeshAsset mesh;
    Node node;
};