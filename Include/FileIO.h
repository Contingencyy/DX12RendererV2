#pragma once

namespace Renderer
{
	struct Vertex;
	struct UploadMeshParams;
}

struct cgltf_data;

namespace FileIO
{

	struct LoadImageResult
	{
		uint32_t width;
		uint32_t height;
		uint8_t* bytes;
	};

	LoadImageResult LoadImage(const char* filepath);
	cgltf_data* LoadGLTF(const char* filepath);

}
