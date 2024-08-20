#pragma once

#include "vk_types.h"
#include "vk_material.h"
#include "vk_loader.h"

struct RenderObject {
    uint32_t index_count;
    uint32_t first_index;
    VkBuffer index_buffer;
    
    MaterialInstance* material;

    glm::mat4 transform;
    VkDeviceAddress vertex_buffer_address;
};

struct DrawContext {
	std::vector<RenderObject> OpaqueSurfaces;
};

// TODO: Maybe make this some sort of enum-based approach intead of ingeritance-based later
class IRenderable {
    virtual void Draw(const glm::mat4& topMatrix, DrawContext& ctx) = 0;
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

    virtual void Draw(const glm::mat4& top_matrix, DrawContext& ctx)
    {
        for (auto& c : children) {
            c->Draw(top_matrix, ctx);
        }
    }
};

struct MeshNode : public Node {

	std::shared_ptr<MeshAsset> mesh;

	virtual void Draw(const glm::mat4& topMatrix, DrawContext& ctx) override;
};