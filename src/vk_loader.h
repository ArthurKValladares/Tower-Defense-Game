#pragma once

#include "vk_types.h"
#include "vk_material.h"

#include <string>
#include <vector>
#include <optional>
#include <filesystem>

struct GLTFMaterial {
	MaterialInstance data;
};

struct GeoSurface {
    uint32_t start_index;
    uint32_t count;
    std::shared_ptr<GLTFMaterial> material;
};

struct MeshAsset {
    std::string name;

    std::vector<GeoSurface> surfaces;
    GPUMeshBuffers mesh_buffers;
};

struct VkEngine;
std::optional<std::vector<std::shared_ptr<MeshAsset>>> load_gltf_meshes(VkEngine* engine, std::filesystem::path file_path);