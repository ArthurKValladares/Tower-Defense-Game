#include "cube.h"

#include "../renderer/vk_engine.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/transform.hpp>

Cube::Cube(VkEngine* engine, std::string name, glm::vec3 translate, glm::quat rotation, glm::vec3 scale) 
{
    creator = engine;

    material_data_buffer = engine->create_buffer(
        sizeof(FlatColorMaterial::MaterialConstants),
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU
    );

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

    // Create surface
    GeoSurface new_surface;
    new_surface.start_index = 0;
    new_surface.count = indices.size();
    new_surface.material = std::make_shared<GLTFMaterial>();

    FlatColorMaterial::MaterialResources material_resources;
    material_resources.data_buffer = material_data_buffer.buffer;
    material_resources.data_buffer_offset = 0;

    new_surface.material->data = engine->flat_color_material.write_material(engine->vk_device(), material_resources, engine->_global_descriptor_allocator);

    MeshAsset& mesh_asset = *mesh_node.mesh;

    // Calculate bounds
    // TODO: Duplicated code
    glm::vec3 min_pos = vertices[0].position;
    glm::vec3 max_pos = vertices[0].position;
    for (int i = 0; i < vertices.size(); i++) {
        min_pos = glm::min(min_pos, vertices[i].position);
        max_pos = glm::max(max_pos, vertices[i].position);
    }
    new_surface.bounds.origin = (max_pos + min_pos) / 2.f;
    new_surface.bounds.extents = (max_pos - min_pos) / 2.f;
    new_surface.bounds.sphere_radius = glm::length(new_surface.bounds.extents);

    mesh_asset.surfaces.push_back(new_surface);

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
    creator->destroy_buffer(material_data_buffer);
}

void Cube::draw(const glm::mat4& top_matrix, DrawContext& ctx) {
    mesh_node.draw(top_matrix, ctx);
}