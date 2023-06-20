#pragma once

namespace Renderer
{
	struct Vertex;
	struct UploadMeshParams;
}

namespace FileIO
{

	struct LoadImageResult
	{
		uint32_t width;
		uint32_t height;
		uint8_t* bytes;
	};

	LoadImageResult LoadImage(const char* filepath);

	struct LoadGLTFResult
	{
		uint32_t num_meshes;
		Renderer::UploadMeshParams* mesh_params;
	};

	LoadGLTFResult LoadGLTF(const char* filepath);

}
