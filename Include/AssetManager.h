#pragma once
#include "Containers/ResourceSlotmap.h"

struct Model
{
	struct Node
	{
		uint32_t num_meshes;
		ResourceHandle* mesh_handles;
		ResourceHandle* texture_handles;
		Mat4x4 transform;

		uint32_t num_children;
		size_t* children;

		const char* name;
	};

	uint32_t num_nodes;
	Node* nodes;
	uint32_t num_root_nodes;
	size_t* root_nodes;

	const char* name;
};

namespace AssetManager
{

	void Init();
	void Exit();

	void LoadTexture(const char* filepath);
	ResourceHandle GetTexture(const char* filepath);
	void LoadModel(const char* filepath);
	Model* GetModel(const char* filepath);

}
