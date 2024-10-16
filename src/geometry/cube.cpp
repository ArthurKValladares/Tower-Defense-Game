#include "cube.h"

#include "../renderer/vk_engine.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/transform.hpp>

Cube::Cube(VkEngine* engine, std::string name, 
    glm::vec3 translate, glm::quat rotation, glm::vec3 scale,
    glm::vec4 color) 
{
    creator = engine;

    material_data_buffer = engine->create_buffer(
        sizeof(FlatColorMaterial::MaterialConstants),
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU
    );

    mesh = std::make_shared<MeshAsset>();
    mesh->name = std::move(name);

    std::vector<uint32_t> indices;
    std::vector<Vertex> vertices;
    vertices.resize(8);

    //
    // Load/Create Mesh
    //
    
    // 0   1
    // 
    // 2   3

    // 4   5
    //
    // 6   7

    // Create top plane vertices
    {
        const float y_pos = 0.5;

        Vertex bottom_left;
        bottom_left.position = glm::vec3(-0.5, y_pos, -0.5);
        bottom_left.normal = { 1, 0, 0 };
        bottom_left.color = color;
        bottom_left.uv_x = 0;
        bottom_left.uv_y = 0;
        vertices[0] = bottom_left;

        Vertex bottom_right;
        bottom_right.position = glm::vec3(0.5, y_pos, -0.5);
        bottom_right.normal = { 1, 0, 0 };
        bottom_right.color = color;
        bottom_right.uv_x = 0;
        bottom_right.uv_y = 0;
        vertices[1] = bottom_right;

        Vertex top_left;
        top_left.position = glm::vec3(-0.5, y_pos, 0.5);
        top_left.normal = { 1, 0, 0 };
        top_left.color = color;
        top_left.uv_x = 0;
        top_left.uv_y = 0;
        vertices[2] = top_left;

        Vertex top_right;
        top_right.position = glm::vec3(0.5, y_pos, 0.5);
        top_right.normal = { 1, 0, 0 };
        top_right.color = color;
        top_right.uv_x = 0;
        top_right.uv_y = 0;
        vertices[3] = top_right;

        // 0   1
        // 
        // 2   3
        std::vector plane_indices = {0, 2, 1, 1, 2, 3};
        indices.insert(indices.end(), plane_indices.begin(), plane_indices.end());
    }

    // Create bottom plane vertices
    {
        const float y_pos = -0.5;

        Vertex bottom_left;
        bottom_left.position = glm::vec3(-0.5, y_pos, -0.5);
        bottom_left.normal = { 1, 0, 0 };
        bottom_left.color = color;
        bottom_left.uv_x = 0;
        bottom_left.uv_y = 0;
        vertices[4] = bottom_left;

        Vertex bottom_right;
        bottom_right.position = glm::vec3(0.5, y_pos, -0.5);
        bottom_right.normal = { 1, 0, 0 };
        bottom_right.color = color;
        bottom_right.uv_x = 0;
        bottom_right.uv_y = 0;
        vertices[5] = bottom_right;

        Vertex top_left;
        top_left.position = glm::vec3(-0.5, y_pos, 0.5);
        top_left.normal = { 1, 0, 0 };
        top_left.color = color;
        top_left.uv_x = 0;
        top_left.uv_y = 0;
        vertices[6] = top_left;

        Vertex top_right;
        top_right.position = glm::vec3(0.5, y_pos, 0.5);
        top_right.normal = { 1, 0, 0 };
        top_right.color = color;
        top_right.uv_x = 0;
        top_right.uv_y = 0;
        vertices[7] = top_right;

        // 4   5
        //
        // 6   7
        std::vector plane_indices = {4, 6, 5, 5, 6, 7};
        indices.insert(indices.end(), plane_indices.begin(), plane_indices.end());
    }

    // 0   1
    //
    // 4   5
    {
        std::vector plane_indices = {0, 4, 1, 1, 4, 5};
        indices.insert(indices.end(), plane_indices.begin(), plane_indices.end());
    }

    // 2   3
    //
    // 6   7
    {
        std::vector plane_indices = {2, 6, 3, 3, 6, 7};
        indices.insert(indices.end(), plane_indices.begin(), plane_indices.end());
    }

    // 0   2
    //
    // 4   6
    {
        std::vector plane_indices = {0, 4, 2, 2, 4, 6};
        indices.insert(indices.end(), plane_indices.begin(), plane_indices.end());
    }

    // 3   1
    //
    // 7   5
    {
        std::vector plane_indices = {3, 7, 1, 1, 7, 5};
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

    mesh->surfaces.push_back(new_surface);

    mesh->mesh_buffers = engine->upload_mesh(indices, vertices);

    //
    // Load/Create Node
    //

    glm::mat4 tm = glm::translate(glm::mat4(1.f), translate);
    glm::mat4 rm = glm::toMat4(rotation);
    glm::mat4 sm = glm::scale(glm::mat4(1.f), scale);
    local_transform = tm * rm * sm;

    refresh_transform(glm::mat4 { 1.f });
}

Cube::~Cube() {
    VkDevice dv = creator->vk_device();

    creator->destroy_buffer(mesh->mesh_buffers.index_buffer);
    creator->destroy_buffer(mesh->mesh_buffers.vertex_buffer);
    creator->destroy_buffer(material_data_buffer);
}