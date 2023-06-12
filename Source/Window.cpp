#include "Pch.h"
#include "Window.h"

namespace Window
{

	HWND g_hWnd;
	RECT g_window_rect;
	RECT g_client_rect;

	LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
	{
		//if (Application::IsRunning())
		if (true)
		{
			switch (msg)
			{
			case WM_SIZE:
			case WM_SIZING:
			{
				// Window is being resized
				::GetClientRect(hWnd, &g_client_rect);
			} break;
			
			case WM_DESTROY:
			{
				::PostQuitMessage(0);
			} break;

			default:
			{
				return ::DefWindowProcW(hWnd, msg, wParam, lParam);
			} break;
			}
		}
	}

	void Create(const WindowProps& props)
	{
		// Register window class
		HINSTANCE hInst = GetModuleHandle(NULL);
		const wchar_t* window_class_name = L"DX12RendererV2Window";

		WNDCLASSEXW window_class = {};
		window_class.cbSize = sizeof(WNDCLASSEX);
		window_class.style = CS_HREDRAW | CS_VREDRAW;
		window_class.hInstance = hInst;
		window_class.lpfnWndProc = &WndProc;
		window_class.cbClsExtra = 0;
		window_class.cbWndExtra = 0;
		window_class.hIcon = ::LoadIcon(hInst, NULL);
		window_class.hCursor = ::LoadCursor(NULL, IDC_ARROW);
		window_class.hbrBackground = (HBRUSH)(COLOR_WINDOWFRAME);
		window_class.lpszMenuName = NULL;
		window_class.lpszClassName = window_class_name;
		window_class.hIconSm = ::LoadIcon(hInst, NULL);

		static ATOM atom = ::RegisterClassExW(&window_class);
		DXV2_ASSERT(atom > 0 && "Failed to register the window class");

		// Create the actual window
		int screen_width = ::GetSystemMetrics(SM_CXSCREEN);
		int screen_height = ::GetSystemMetrics(SM_CYSCREEN);

		g_window_rect = { 0, 0, (LONG)props.width, (LONG)props.height };
		::AdjustWindowRect(&g_window_rect, WS_OVERLAPPEDWINDOW, FALSE);

		int window_width = g_window_rect.right - g_window_rect.left;
		int window_height = g_window_rect.bottom - g_window_rect.top;

		int window_x = DXV2_MAX(0, (screen_width - window_width) / 2);
		int window_y = DXV2_MAX(0, (screen_height - window_height) / 2);

		g_hWnd = ::CreateWindowExW(NULL, window_class_name, props.name, WS_OVERLAPPEDWINDOW,
			window_x, window_y, window_width, window_height, NULL, NULL, hInst, nullptr);
		DXV2_ASSERT(g_hWnd && "Failed to create the window\n");

		::GetClientRect(g_hWnd, &g_client_rect);
		::ShowWindow(g_hWnd, 1);
	}

	void Destroy()
	{
		// This does not do anything yet
	}

}
