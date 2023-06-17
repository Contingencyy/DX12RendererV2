#pragma once

namespace Renderer
{

	struct RendererInitParams
	{
		HWND hWnd;
		uint32_t width;
		uint32_t height;
	};

	void Init(const RendererInitParams& params);
	void Exit();

	void BeginFrame();
	void EndFrame();
	void RenderFrame();

	void Flush();

	void OnWindowResize(uint32_t new_width, uint32_t new_height);
	void OnImGuiRender();

	bool IsInitialized();

}
