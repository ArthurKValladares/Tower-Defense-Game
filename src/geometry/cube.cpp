#include "cube.h"

#include "../renderer/vk_engine.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/transform.hpp>

Cube::Cube(VkEngine* engine, std::string name, glm::vec3 translate, glm::quat rotation, glm::vec3 scale) 
{
    creator = engine;

    mesh_node.mesh = std::make_shared<MeshAsset>();
    mesh_node.mesh->name = std::move(name);

    std::vector<uint32_t> indices;
    std::vector<Vertex> vertices;
    vertices.resize(8);

    //
    // Load/Create Mesh
    //
    
    // Create top plane vertices
    {
        const float y_pos = 0.5;

        Vertex bottom_left;
        bottom_left.position = glm::vec3(-0.5, y_pos, -0.5);
        bottom_left.normal = { 1, 0, 0 };
        bottom_left.color = glm::vec4 { 1.f };
        bottom_left.uv_x = 0;
        bottom_left.uv_y = 0;
        vertices[0] = bottom_left;

        Vertex bottom_right;
        bottom_right.position = glm::vec3(0.5, y_pos, -0.5);
        bottom_right.normal = { 1, 0, 0 };
        bottom_right.color = glm::vec4 { 1.f };
        bottom_right.uv_x = 0;
        bottom_right.uv_y = 0;
        vertices[1] = bottom_right;

        Vertex top_left;
        top_left.position = glm::vec3(-0.5, y_pos, 0.5);
        top_left.normal = { 1, 0, 0 };
        top_left.color = glm::vec4 { 1.f };
        top_left.uv_x = 0;
        top_left.uv_y = 0;
        vertices[2] = top_left;

        Vertex top_right;
        top_right.position = glm::vec3(0.5, y_pos, 0.5);
        top_right.normal = { 1, 0, 0 };
        top_right.color = glm::vec4 { 1.f };
        top_right.uv_x = 0;
        top_right.uv_y = 0;
        vertices[3] = top_right;

        std::vector plane_indices = {0, 2, 1, 1, 2, 3};
        indices.insert(indices.end(), plane_indices.begin(), plane_indices.end());
    }

    // Create bottom plane vertices
    {
        const float y_pos = -0.5;

        Vertex bottom_left;
        bottom_left.position = glm::vec3(-0.5, y_pos, -0.5);
        bottom_left.normal = { 1, 0, 0 };
        bottom_left.color = glm::vec4 { 1.f };
        bottom_left.uv_x = 0;
        bottom_left.uv_y = 0;
        vertices[0] = bottom_left;

        Vertex bottom_right;
        bottom_right.position = glm::vec3(0.5, y_pos, -0.5);
        bottom_right.normal = { 1, 0, 0 };
        bottom_right.color = glm::vec4 { 1.f };
        bottom_right.uv_x = 0;
        bottom_right.uv_y = 0;
        vertices[1] = bottom_right;

        Vertex top_left;
        top_left.position = glm::vec3(-0.5, y_pos, 0.5);
        top_left.normal = { 1, 0, 0 };
        top_left.color = glm::vec4 { 1.f };
        top_left.uv_x = 0;
        top_left.uv_y = 0;
        vertices[2] = top_left;

        Vertex top_right;
        top_right.position = glm::vec3(0.5, y_pos, 0.5);
        top_right.normal = { 1, 0, 0 };
        top_right.color = glm::vec4 { 1.f };
        top_right.uv_x = 0;
        top_right.uv_y = 0;
        vertices[3] = top_right;

        std::vector plane_indices = {4, 6, 5, 5, 6, 7};
        indices.insert(indices.end(), plane_indices.begin(), plane_indices.end());
    }

    MeshAsset& mesh_asset = *mesh_node.mesh;
    mesh_asset.mesh_buffers = engine->upload_mesh(indices, vertices);

    //
    // Load/Create Node
    //

    glm::mat4 tm = glm::translate(glm::mat4(1.f), translate);
    glm::mat4 rm = glm::toMat4(rotation);
    glm::mat4 sm = glm::scale(glm::mat4(1.f), scale);
    mesh_node.local_transform = tm * rm * sm;

    mesh_node.refresh_transform(glm::mat4 { 1.f });
}

Cube::~Cube() {
    VkDevice dv = creator->vk_device();

    creator->destroy_buffer(mesh_node.mesh->mesh_buffers.index_buffer);
    creator->destroy_buffer(mesh_node.mesh->mesh_buffers.vertex_buffer);
}

void Cube::draw(const glm::mat4& top_matrix, DrawContext& ctx) {
    mesh_node.draw(top_matrix, ctx);
}