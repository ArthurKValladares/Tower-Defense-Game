#pragma once

#include "vk_renderable.h"
#include "vk_engine.h"

#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/transform.hpp>
#include <glm/gtx/quaternion.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

namespace {
    VkFilter extract_filter(fastgltf::Filter filter)
    {
        switch (filter) {
        case fastgltf::Filter::Nearest:
        case fastgltf::Filter::NearestMipMapNearest:
        case fastgltf::Filter::NearestMipMapLinear:
            return VK_FILTER_NEAREST;

        case fastgltf::Filter::Linear:
        case fastgltf::Filter::LinearMipMapNearest:
        case fastgltf::Filter::LinearMipMapLinear:
        default:
            return VK_FILTER_LINEAR;
        }
    }

    VkSamplerMipmapMode extract_mipmap_mode(fastgltf::Filter filter)
    {
        switch (filter) {
        case fastgltf::Filter::NearestMipMapNearest:
        case fastgltf::Filter::LinearMipMapNearest:
            return VK_SAMPLER_MIPMAP_MODE_NEAREST;

        case fastgltf::Filter::NearestMipMapLinear:
        case fastgltf::Filter::LinearMipMapLinear:
        default:
            return VK_SAMPLER_MIPMAP_MODE_LINEAR;
        }
    }

	std::optional<AllocatedImage> load_image(VkEngine* engine, fastgltf::Asset& asset, fastgltf::Image& image)
	{
		AllocatedImage new_image {};

		int width, height, nr_channels;

		std::visit(
			fastgltf::visitor {
				[](auto& arg) {},
				[&](fastgltf::sources::URI& filePath) {
					std::print("URI source.\n");

					// We don't support offsets with stbi.
					assert(filePath.fileByteOffset == 0);
					// We're only capable of loading local files
					assert(filePath.uri.isLocalPath());

					const std::string path(filePath.uri.path().begin(), filePath.uri.path().end());
					unsigned char* data = stbi_load(path.c_str(), &width, &height, &nr_channels, 4);
					if (data) {
						VkExtent3D imagesize;
						imagesize.width = width;
						imagesize.height = height;
						imagesize.depth = 1;

						new_image = engine->create_image(data, imagesize, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT,false);

						stbi_image_free(data);
					}
				},
				[&](fastgltf::sources::Vector& vector) {
					std::print("Vector source.\n");

					unsigned char* data = stbi_load_from_memory(
						(stbi_uc*) vector.bytes.data(), static_cast<int>(vector.bytes.size()),
						&width, &height, &nr_channels, 4
					);
					if (data) {
						VkExtent3D imagesize;
						imagesize.width = width;
						imagesize.height = height;
						imagesize.depth = 1;

						new_image = engine->create_image(data, imagesize, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT,false);

						stbi_image_free(data);
					}
				},
				[&](fastgltf::sources::BufferView& view) {
					std::print("BufferView source.\n");

					fastgltf::BufferView& bufferView = asset.bufferViews[view.bufferViewIndex];
					fastgltf::Buffer& buffer = asset.buffers[bufferView.bufferIndex];

					// We only care about VectorWithMime here, because we specify LoadExternalBuffers, meaning all buffers
					// are already loaded into a vector.
					std::visit(
						fastgltf::visitor {
							[](auto& arg) {
								std::print("Fallback Buffer data source.\n");
							},
							[&](fastgltf::sources::Array& array) {
								std::print("Array Buffer data source.\n");

								unsigned char* data = stbi_load_from_memory((stbi_uc*) array.bytes.data() + bufferView.byteOffset,
									static_cast<int>(bufferView.byteLength),
									&width, &height, &nr_channels, 4);
								if (data) {
									VkExtent3D imagesize;
									imagesize.width = width;
									imagesize.height = height;
									imagesize.depth = 1;

									new_image = engine->create_image(data, imagesize, VK_FORMAT_R8G8B8A8_UNORM,
										VK_IMAGE_USAGE_SAMPLED_BIT,false);

									stbi_image_free(data);
								}
							},
						},
						buffer.data
					);
				},
			},
			image.data
		);

		if (new_image.image == VK_NULL_HANDLE) {
			return {};
		} else {
			return new_image;
		}
	}
};

void MeshNode::draw(const glm::mat4& top_matrix, DrawContext& ctx)
{
	glm::mat4 node_matrix = top_matrix * world_transform;

	for (GeoSurface& s : mesh->surfaces) {
		RenderObject def;
		def.index_count = s.count;
		def.first_index = s.start_index;
		def.index_buffer = mesh->mesh_buffers.index_buffer.buffer;
		def.material = &s.material->data;
        def.bounds = s.bounds;
		def.transform = node_matrix;
		def.vertex_buffer_address = mesh->mesh_buffers.vertex_buffer_address;
		
        if (s.material->data.pass_type == MaterialPass::Transparent) {
            ctx.transparent_surfaces.push_back(def);
        } else {
		    ctx.opaque_surfaces.push_back(def);
        }
	}

	Node::draw(top_matrix, ctx);
}

std::optional<std::shared_ptr<LoadedGLTF>> LoadedGLTF::load_gltf(VkEngine* engine, std::string_view file_path) {
    //
    // Load Gltf file
    //

    std::print("Loading GLTF: {}\n", file_path);

    std::shared_ptr<LoadedGLTF> scene = std::make_shared<LoadedGLTF>();
    scene->creator = engine;
    LoadedGLTF& file = *scene.get();

    fastgltf::Parser parser {};

    constexpr auto gltf_options = 
        fastgltf::Options::DontRequireValidAssetMember | fastgltf::Options::AllowDouble | fastgltf::Options::LoadExternalBuffers;

    fastgltf::Expected<fastgltf::GltfDataBuffer> expected_data = fastgltf::GltfDataBuffer::FromPath(file_path);
    if (expected_data.error() != fastgltf::Error::None) {
        std::print("Failed to load glTF: {} \n", fastgltf::getErrorName(expected_data.error()));
        return {};
    }
    fastgltf::GltfDataBuffer& data = expected_data.get();

    fastgltf::Asset gltf;

    std::filesystem::path path = file_path;

    auto type = fastgltf::determineGltfFileType(data);
    if (type == fastgltf::GltfType::glTF) {
        auto load = parser.loadGltf(data, path.parent_path(), gltf_options);
        if (load) {
            gltf = std::move(load.get());
        } else {
            std::print("Failed to load glTF: {}\n", fastgltf::getErrorName(load.error()));
            return {};
        }
    } else if (type == fastgltf::GltfType::GLB) {
        auto load = parser.loadGltfBinary(data, path.parent_path(), gltf_options);
        if (load) {
            gltf = std::move(load.get());
        } else {
            std::print("Failed to load glTF: {}\n", fastgltf::getErrorName(load.error()));
            return {};
        }
    } else {
        std::print("Failed to determine glTF container.\n");
        return {};
    }

    // Create descriptor pool with a default estimated size
    std::vector<DescriptorAllocator::PoolSizeRatio> sizes = { 
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1 }
    };

    file.descriptor_pool.init(engine->_device, gltf.materials.size(), sizes);

    //
    // Load samplers
    //

    for (fastgltf::Sampler& sampler : gltf.samplers) {

        VkSamplerCreateInfo sampl = { .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO, .pNext = nullptr};
        sampl.maxLod = VK_LOD_CLAMP_NONE;
        sampl.minLod = 0;

        sampl.magFilter = extract_filter(sampler.magFilter.value_or(fastgltf::Filter::Nearest));
        sampl.minFilter = extract_filter(sampler.minFilter.value_or(fastgltf::Filter::Nearest));

        sampl.mipmapMode= extract_mipmap_mode(sampler.minFilter.value_or(fastgltf::Filter::Nearest));

        VkSampler new_sampler;
        vkCreateSampler(engine->_device, &sampl, nullptr, &new_sampler);

        file.samplers.push_back(new_sampler);
    }

    // Temporal arrays for all the objects to use while creating the GLTF data
    std::vector<std::shared_ptr<MeshAsset>> meshes;
    std::vector<std::shared_ptr<Node>> nodes;
    std::vector<AllocatedImage> images;
    std::vector<std::shared_ptr<GLTFMaterial>> materials;

    //
    // Load all textures
    //

    for (fastgltf::Image& image : gltf.images) {
		std::print("--- gltf loading texture: {} ---\n", image.name);

        std::optional<AllocatedImage> img = load_image(engine, gltf, image);

		if (img.has_value()) {
			images.push_back(*img);
			file.images[image.name.c_str()] = *img;
		}
		else {
			// we failed to load, so lets give the slot a default white texture to not
			// completely break loading
			images.push_back(engine->_default_images._error_checkerboard_image);
			std::print("gltf failed to load texture\n");
		}
    }

    //
    // Load materials
    //

    // Create buffer big enough to hold the material data
    // TODO: Will be more complicated once we have more material types
    file.material_data_buffer = engine->create_buffer(
        sizeof(GLTFMetallic_Roughness::MaterialConstants) * gltf.materials.size(),
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU
    );

    int data_index = 0;
    GLTFMetallic_Roughness::MaterialConstants* sceneMaterialConstants =
        (GLTFMetallic_Roughness::MaterialConstants*) file.material_data_buffer.info.pMappedData;

    for (fastgltf::Material& mat : gltf.materials) {
        std::shared_ptr<GLTFMaterial> new_material = std::make_shared<GLTFMaterial>();
        materials.push_back(new_material);
        file.materials[mat.name.c_str()] = new_material;

        GLTFMetallic_Roughness::MaterialConstants constants;
        constants.color_factors.x = mat.pbrData.baseColorFactor[0];
        constants.color_factors.y = mat.pbrData.baseColorFactor[1];
        constants.color_factors.z = mat.pbrData.baseColorFactor[2];
        constants.color_factors.w = mat.pbrData.baseColorFactor[3];

        constants.metal_rough_factors.x = mat.pbrData.metallicFactor;
        constants.metal_rough_factors.y = mat.pbrData.roughnessFactor;

        // Write material parameters to buffer
        sceneMaterialConstants[data_index] = constants;

        MaterialPass pass_type = MaterialPass::MainColor;
        if (mat.alphaMode == fastgltf::AlphaMode::Blend) {
            pass_type = MaterialPass::Transparent;
        }

        GLTFMetallic_Roughness::MaterialResources material_resources;
        // Set defaults
        material_resources.color_image = engine->_default_images._white_image;
        material_resources.color_sampler = engine->_default_images._sampler_linear;
        material_resources.metal_rough_image = engine->_default_images._white_image;
        material_resources.metal_rough_sampler = engine->_default_images._sampler_linear;

        // set the uniform buffer for the material data
        material_resources.data_buffer = file.material_data_buffer.buffer;
        material_resources.data_buffer_offset = data_index * sizeof(GLTFMetallic_Roughness::MaterialConstants);

        // Grab color image and sampler
        if (mat.pbrData.baseColorTexture.has_value()) {
            size_t img = gltf.textures[mat.pbrData.baseColorTexture.value().textureIndex].imageIndex.value();
            size_t sampler = gltf.textures[mat.pbrData.baseColorTexture.value().textureIndex].samplerIndex.value();

            material_resources.color_image = images[img];
            material_resources.color_sampler = file.samplers[sampler];
        }

        // Build material
        new_material->data = engine->metal_rough_material.write_material(engine->_device, pass_type, material_resources, file.descriptor_pool);

        data_index++;
    }

    //
    // Load meshes
    //

    // use the same vectors for all meshes so that the memory doesnt reallocate as
    // often
    std::vector<uint32_t> indices;
    std::vector<Vertex> vertices;

    for (fastgltf::Mesh& mesh : gltf.meshes) {
        std::shared_ptr<MeshAsset> new_mesh = std::make_shared<MeshAsset>();
        meshes.push_back(new_mesh);
        file.meshes[mesh.name.c_str()] = new_mesh;
        new_mesh->name = mesh.name;

        // clear the mesh arrays each mesh, we dont want to merge them by error
        indices.clear();
        vertices.clear();

        for (auto&& p : mesh.primitives) {
            GeoSurface new_surface;
            new_surface.start_index = (uint32_t)indices.size();
            new_surface.count = (uint32_t)gltf.accessors[p.indicesAccessor.value()].count;

            size_t initial_vtx = vertices.size();

            // load indexes
            {
                fastgltf::Accessor& index_accessor = gltf.accessors[p.indicesAccessor.value()];
                indices.reserve(indices.size() + index_accessor.count);

                fastgltf::iterateAccessor<std::uint32_t>(gltf, index_accessor,
                    [&](std::uint32_t idx) {
                        indices.push_back(idx + initial_vtx);
                    });
            }

            // load vertex positions
            {
                fastgltf::Accessor& pos_accessor = gltf.accessors[p.findAttribute("POSITION")->accessorIndex];
                vertices.resize(vertices.size() + pos_accessor.count);

                fastgltf::iterateAccessorWithIndex<glm::vec3>(gltf, pos_accessor,
                    [&](glm::vec3 v, size_t index) {
                        Vertex newvtx;
                        newvtx.position = v;
                        newvtx.normal = { 1, 0, 0 };
                        newvtx.color = glm::vec4 { 1.f };
                        newvtx.uv_x = 0;
                        newvtx.uv_y = 0;
                        vertices[initial_vtx + index] = newvtx;
                    });
            }

            // load vertex normals
            auto normals = p.findAttribute("NORMAL");
            if (normals != p.attributes.end()) {

                fastgltf::iterateAccessorWithIndex<glm::vec3>(gltf, gltf.accessors[(*normals).accessorIndex],
                    [&](glm::vec3 v, size_t index) {
                        vertices[initial_vtx + index].normal = v;
                    });
            }

            // load UVs
            auto uv = p.findAttribute("TEXCOORD_0");
            if (uv != p.attributes.end()) {

                fastgltf::iterateAccessorWithIndex<glm::vec2>(gltf, gltf.accessors[(*uv).accessorIndex],
                    [&](glm::vec2 v, size_t index) {
                        vertices[initial_vtx + index].uv_x = v.x;
                        vertices[initial_vtx + index].uv_y = v.y;
                    });
            }

            // load vertex colors
            auto colors = p.findAttribute("COLOR_0");
            if (colors != p.attributes.end()) {

                fastgltf::iterateAccessorWithIndex<glm::vec4>(gltf, gltf.accessors[(*colors).accessorIndex],
                    [&](glm::vec4 v, size_t index) {
                        vertices[initial_vtx + index].color = v;
                    });
            }

            if (p.materialIndex.has_value()) {
                new_surface.material = materials[p.materialIndex.value()];
            } else {
                new_surface.material = materials[0];
            }

            // Calculate bounds
            glm::vec3 min_pos = vertices[initial_vtx].position;
            glm::vec3 max_pos = vertices[initial_vtx].position;
            for (int i = initial_vtx; i < vertices.size(); i++) {
                min_pos = glm::min(min_pos, vertices[i].position);
                max_pos = glm::max(max_pos, vertices[i].position);
            }
            new_surface.bounds.origin = (max_pos + min_pos) / 2.f;
            new_surface.bounds.extents = (max_pos - min_pos) / 2.f;
            new_surface.bounds.sphere_radius = glm::length(new_surface.bounds.extents);

            new_mesh->surfaces.push_back(new_surface);
        }

        new_mesh->mesh_buffers = engine->upload_mesh(indices, vertices);
    }

    //
    // Load nodes
    //

    for (fastgltf::Node& node : gltf.nodes) {
        std::shared_ptr<Node> new_node;

        // Find if the node has a mesh, and if it does hook it to the mesh pointer,
        // and allocate it with the meshnode class
        if (node.meshIndex.has_value()) {
            new_node = std::make_shared<MeshNode>();
            static_cast<MeshNode*>(new_node.get())->mesh = meshes[*node.meshIndex];
        } else {
            new_node = std::make_shared<Node>();
        }

        nodes.push_back(new_node);
        file.nodes[node.name.c_str()];

        std::visit(
            fastgltf::visitor {
                [&](fastgltf::math::fmat4x4 matrix) {
                    memcpy(&new_node->local_transform, matrix.data(), sizeof(matrix));
                },
                [&](fastgltf::TRS transform) {
                    glm::vec3 tl(transform.translation[0], transform.translation[1],
                        transform.translation[2]);
                    glm::quat rot(transform.rotation[3], transform.rotation[0], transform.rotation[1],
                        transform.rotation[2]);
                    glm::vec3 sc(transform.scale[0], transform.scale[1], transform.scale[2]);

                    glm::mat4 tm = glm::translate(glm::mat4(1.f), tl);
                    glm::mat4 rm = glm::toMat4(rot);
                    glm::mat4 sm = glm::scale(glm::mat4(1.f), sc);

                    new_node->local_transform = tm * rm * sm;
                }
            },
            node.transform
        );
    }

    //
    // Build scene-graph
    //

    // Setup transform hierarchy
    for (int i = 0; i < gltf.nodes.size(); i++) {
        fastgltf::Node& node = gltf.nodes[i];
        std::shared_ptr<Node>& sceneNode = nodes[i];

        for (auto& c : node.children) {
            sceneNode->children.push_back(nodes[c]);
            nodes[c]->parent = sceneNode;
        }
    }

    // Find roots
    for (auto& node : nodes) {
        if (node->parent.lock() == nullptr) {
            file.roots.push_back(node);
            node->refresh_transform(glm::mat4 { 1.f });
        }
    }

    return scene;
}

void LoadedGLTF::draw(const glm::mat4& top_matrix, DrawContext& ctx)
{
    // create renderables from the scenenodes
    for (auto& n : roots) {
        n->draw(top_matrix, ctx);
    }
}

void LoadedGLTF::clear_all() {
	VkDevice dv = creator->_device;

    descriptor_pool.destroy_pools(dv);
    creator->destroy_buffer(material_data_buffer);

    for (auto& [k, v] : meshes) {

		creator->destroy_buffer(v->mesh_buffers.index_buffer);
		creator->destroy_buffer(v->mesh_buffers.vertex_buffer);
    }

    for (auto& [k, v] : images) {
        
        if (v.image == creator->_default_images._error_checkerboard_image.image) {
            // Don't destroy the default images
            continue;
        }
        creator->destroy_image(v);
    }

	for (auto& sampler : samplers) {
		vkDestroySampler(dv, sampler, nullptr);
    }
}

bool is_visible(const RenderObject& obj, const glm::mat4& view_proj) {
    // Create the 8 corners of the mesh-space bounding box, with the bounds x: [-1, 1], y: [-1, 1], z: [-1, 1]
    std::array<glm::vec3, 8> corners {
        glm::vec3 { 1, 1, 1 },
        glm::vec3 { 1, 1, -1 },
        glm::vec3 { 1, -1, 1 },
        glm::vec3 { 1, -1, -1 },
        glm::vec3 { -1, 1, 1 },
        glm::vec3 { -1, 1, -1 },
        glm::vec3 { -1, -1, 1 },
        glm::vec3 { -1, -1, -1 },
    };

    glm::mat4 matrix = view_proj * obj.transform;

    // Initial min/max bounds outside mesh bounding box
    glm::vec3 min = { 1.5, 1.5, 1.5 };
    glm::vec3 max = { -1.5, -1.5, -1.5 };

    for (int c = 0; c < 8; c++) {
        // Project each corner into clip space
        glm::vec3 corner_extent = corners[c] * obj.bounds.extents;
        glm::vec4 v = matrix * glm::vec4(obj.bounds.origin + corner_extent, 1.f);

        // Perspective correction
        v.x = v.x / v.w;
        v.y = v.y / v.w;
        v.z = v.z / v.w;

        min = glm::min(glm::vec3 { v.x, v.y, v.z }, min);
        max = glm::max(glm::vec3 { v.x, v.y, v.z }, max);
    }

    if (min.z > 1.f || max.z < 0.f || min.x > 1.f || max.x < -1.f || min.y > 1.f || max.y < -1.f) {
        return false;
    } else {
        return true;
    }
}

float distance_to_camera(const RenderObject& obj, const Camera& camera) {
    return glm::length(obj.bounds.origin - camera.position);
}