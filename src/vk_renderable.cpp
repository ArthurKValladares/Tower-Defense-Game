#pragma once

#include "vk_renderable.h"

void MeshNode::Draw(const glm::mat4& top_matrix, DrawContext& ctx)
{
	glm::mat4 node_matrix = top_matrix * world_transform;

	for (GeoSurface& s : mesh->surfaces) {
		RenderObject def;
		def.index_count = s.count;
		def.first_index = s.start_index;
		def.index_buffer = mesh->mesh_buffers.index_buffer.buffer;
		def.material = &s.material->data;

		def.transform = node_matrix;
		def.vertex_buffer_address = mesh->mesh_buffers.vertex_buffer_address;
		
		ctx.OpaqueSurfaces.push_back(def);
	}

	Node::Draw(top_matrix, ctx);
}