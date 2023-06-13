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

}
