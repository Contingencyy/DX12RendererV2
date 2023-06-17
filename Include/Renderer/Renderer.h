#pragma once

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
		TextureFormat_RGBA8
	};

	struct UploadTextureParams
	{
		TextureFormat format;
		uint32_t width;
		uint32_t height;
		uint8_t* bytes;

		const wchar_t* name;
	};

	void Init(const RendererInitParams& params);
	void Exit();
	void Flush();

	void BeginFrame();
	void EndFrame();
	void RenderFrame();

	void UploadTexture(const UploadTextureParams& params);

	void OnWindowResize(uint32_t new_width, uint32_t new_height);
	void OnImGuiRender();

	bool IsInitialized();

}
