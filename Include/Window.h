#pragma once

namespace Window
{

	struct WindowProps
	{
		const wchar_t* name;
		uint32_t width;
		uint32_t height;
	};

	void Create(const WindowProps& props);
	void Destroy();

	void SetMouseCapture(bool capture);
	void ResetMousePosition();
	void GetWindowCenter(uint32_t* x, uint32_t* y);

	HWND GetHWnd();

}
