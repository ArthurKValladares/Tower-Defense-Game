#include "cube.h"

#include "../renderer/vk_engine.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/transform.hpp>

Cube::Cube(VkEngine* engine, std::string name, glm::vec3 translate, glm::quat rotation, glm::vec3 scale) 
{
    mesh_node.mesh = std::make_shared<MeshAsset>();
    mesh_node.mesh->name = std::move(name);

    std::vector<uint32_t> indices;
    std::vector<Vertex> vertices;

    //
    // Load/Create Mesh
    //
    MeshAsset& mesh_asset = *mesh_node.mesh;

    // TODO: Fill out Mesh asset

    mesh_asset.mesh_buffers = engine->upload_mesh(indices, vertices);

    //
    // Load/Create Node
    //

    glm::mat4 tm = glm::translate(glm::mat4(1.f), translate);
    glm::mat4 rm = glm::toMat4(rotation);
    glm::mat4 sm = glm::scale(glm::mat4(1.f), scale);
    mesh_node.local_transform = tm * rm * sm;
}