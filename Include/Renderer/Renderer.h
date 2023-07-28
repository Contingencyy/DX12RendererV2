#pragma once
#include "Containers/ResourceSlotmap.h"

namespace Renderer
{

	struct RendererInitParams
	{
		LinearAllocator* alloc;
		HWND hWnd;
		uint32_t width;
		uint32_t height;
	};

	enum TextureFormat
	{
		TextureFormat_RGBA8,
		TextureFormat_D32
	};

	struct UploadTextureParams
	{
		TextureFormat format;
		uint32_t width;
		uint32_t height;
		uint8_t* bytes;

		const char* name;
	};

	struct Vertex
	{
		DXMath::Vec3 pos;
		DXMath::Vec2 uv;
	};

	struct UploadMeshParams
	{
		uint32_t num_vertices;
		Vertex* vertices;
		uint32_t num_indices;
		uint32_t* indices;
	};

	void Init(const RendererInitParams& params);
	void Exit();
	void Flush();

	void BeginFrame(const Mat4x4& view, const Mat4x4& projection);
	void RenderFrame();
	void EndFrame();

	void RenderMesh(ResourceHandle mesh_handle, ResourceHandle texture_handle, const Mat4x4& transform);

	ResourceHandle UploadTexture(const UploadTextureParams& params);
	ResourceHandle UploadMesh(const UploadMeshParams& params);

	void OnWindowResize(uint32_t new_width, uint32_t new_height);
	void OnImGuiRender();

	// ImGui
	void BeginImGuiFrame();
	void RenderImGui();

	bool IsInitialized();

}
