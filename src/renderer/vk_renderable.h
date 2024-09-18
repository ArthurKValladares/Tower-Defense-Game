#pragma once

#include "vk_types.h"
#include "vk_material.h"
#include "vk_descriptors.h"
#include "camera.h"

#include <optional>
#include <unordered_map>

struct Bounds {
    glm::vec3 origin;
    float sphere_radius;
    glm::vec3 extents;
};

struct RenderObject {
    uint32_t index_count;
    uint32_t first_index;
    VkBuffer index_buffer;
    
    Bounds bounds;

    MaterialInstance* material;

    glm::mat4 transform;
    VkDeviceAddress vertex_buffer_address;
};

bool is_visible(const RenderObject& obj, const glm::mat4& view_proj);
float distance_to_camera(const RenderObject& obj, const PerspectiveCamera& camera);

struct DrawContext {
	std::vector<RenderObject> opaque_surfaces;
    std::vector<RenderObject> transparent_surfaces;
};

class IRenderable {
    virtual void draw(const glm::mat4& topMatrix, DrawContext& ctx) = 0;
};

struct Node : public IRenderable {
    // weak_ptr avoids circular dependencies
    std::weak_ptr<Node> parent;
    std::vector<std::shared_ptr<Node>> children;

    glm::mat4 local_transform;
    glm::mat4 world_transform;

    void refresh_transform(const glm::mat4& parent_matrix)
    {
        world_transform = parent_matrix * local_transform;
        for (auto c : children) {
            c->refresh_transform(world_transform);
        }
    }

    virtual void draw(const glm::mat4& top_matrix, DrawContext& ctx)
    {
        for (auto& c : children) {
            c->draw(top_matrix, ctx);
        }
    }
};

struct GLTFMaterial {
	MaterialInstance data;
};

struct GeoSurface {
    uint32_t start_index;
    uint32_t count;
    Bounds bounds;
    std::shared_ptr<GLTFMaterial> material;
};

struct MeshAsset {
    std::string name;

    std::vector<GeoSurface> surfaces;
    GPUMeshBuffers mesh_buffers;
};

struct MeshNode : public Node {

	std::shared_ptr<MeshAsset> mesh;

	virtual void draw(const glm::mat4& topMatrix, DrawContext& ctx) override;
};

class VkEngine;
struct LoadedGLTF : public IRenderable {

    static std::optional<std::shared_ptr<LoadedGLTF>> load_gltf(VkEngine* engine, std::string_view file_path);

    // GLTF data
    std::unordered_map<std::string, std::shared_ptr<MeshAsset>> meshes;
    std::unordered_map<std::string, std::shared_ptr<Node>> nodes;
    std::unordered_map<std::string, AllocatedImage> images;
    std::unordered_map<std::string, std::shared_ptr<GLTFMaterial>> materials;

    std::vector<std::shared_ptr<Node>> roots;

    std::vector<VkSampler> samplers;

    DescriptorAllocator descriptor_pool;

    AllocatedBuffer material_data_buffer;

    VkEngine* creator;

    ~LoadedGLTF() { clear_all(); };

    virtual void draw(const glm::mat4& topMatrix, DrawContext& ctx);

private:

    void clear_all();
};
