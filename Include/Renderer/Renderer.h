#pragma once
#include "Containers/ResourceSlotmap.h"
#include "AssetManager.h"

namespace Renderer
{

	struct RendererInitParams
	{
		HWND hWnd;
		uint32_t width;
		uint32_t height;
	};

	enum TextureFormat
	{
		TextureFormat_RGBA8_Unorm,
		TextureFormat_RGBA16_Float,
		TextureFormat_D32_Float
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
		DXMath::Vec3 normal;
		DXMath::Vec4 tangent;
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

	void RenderMesh(ResourceHandle mesh_handle, const Material& material, const Mat4x4& transform);

	ResourceHandle UploadTexture(const UploadTextureParams& params);
	ResourceHandle UploadMesh(const UploadMeshParams& params);

	void OnWindowResize(uint32_t new_width, uint32_t new_height);
	void OnImGuiRender();

	// ImGui
	void BeginImGuiFrame();
	void RenderImGui();

	bool IsInitialized();

}
