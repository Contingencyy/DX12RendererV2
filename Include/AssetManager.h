#pragma once
#include "ResourceSlotmap.h"

struct Model
{
	struct Node
	{
		uint32_t num_meshes;
		ResourceHandle* mesh_handles;
		ResourceHandle* texture_handles;
		//Mat4x4 transform;
		const char* name;

		//size_t* children;
		//uint32_t num_children;
	};

	Node* nodes;
	uint32_t num_nodes;
	//size_t root_nodes;
	//uint32_t num_root_nodes;
	const char* name;
};

namespace AssetManager
{

	ResourceHandle LoadTexture(const char* filepath);
	Model LoadModel(const char* filepath);

}
