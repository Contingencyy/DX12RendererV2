#include "Pch.h"
#include "Window.h"
#include "Input.h"
#include "Renderer/Renderer.h"

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace Window
{

	struct InternalData
	{
		HWND hWnd;
		RECT window_rect;
		RECT client_rect;

		bool capture_mouse;
	} static data;

	LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
	{
		if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
		{
			return true;
		}

		switch (msg)
		{
		case WM_SIZE:
		case WM_SIZING:
		{
			// Window is being resized
			::GetClientRect(hWnd, &data.client_rect);
			if (Renderer::IsInitialized())
			{
				Renderer::OnWindowResize(data.client_rect.right - data.client_rect.left, data.client_rect.bottom - data.client_rect.top);
			}
		} break;

		case WM_SYSKEYDOWN:
		case WM_KEYDOWN:
		case WM_LBUTTONDOWN:
		case WM_RBUTTONDOWN:
		{
			Input::OnKeyPressed(wParam);
		} break;

		case WM_SYSKEYUP:
		case WM_KEYUP:
		{
			Input::OnKeyReleased(wParam);
		} break;
		case WM_LBUTTONUP:
		{
			Input::OnKeyReleased(VK_LBUTTON);
		} break;
		case WM_RBUTTONUP:
		{
			Input::OnKeyReleased(VK_RBUTTON);
		} break;

		case WM_MOUSEMOVE:
		{
			Input::OnMouseMoved(lParam);
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
		DX_ASSERT(atom > 0 && "Failed to register the window class");

		// Create the actual window
		int screen_width = ::GetSystemMetrics(SM_CXSCREEN);
		int screen_height = ::GetSystemMetrics(SM_CYSCREEN);

		data.window_rect = { 0, 0, (LONG)props.width, (LONG)props.height };
		::AdjustWindowRect(&data.window_rect, WS_OVERLAPPEDWINDOW, FALSE);

		int window_width = data.window_rect.right - data.window_rect.left;
		int window_height = data.window_rect.bottom - data.window_rect.top;

		int window_x = DX_MAX(0, (screen_width - window_width) / 2);
		int window_y = DX_MAX(0, (screen_height - window_height) / 2);

		data.hWnd = ::CreateWindowExW(NULL, window_class_name, props.name, WS_OVERLAPPEDWINDOW,
			window_x, window_y, window_width, window_height, NULL, NULL, hInst, nullptr);
		DX_ASSERT(data.hWnd && "Failed to create the window\n");

		::GetClientRect(data.hWnd, &data.client_rect);
		::ShowWindow(data.hWnd, 1);
	}

	void Destroy()
	{
	}

	void SetMouseCapture(bool capture)
	{
		if (data.capture_mouse != capture)
		{
			data.capture_mouse = capture;

			::GetWindowRect(data.hWnd, &data.window_rect);
			ShowCursor(!capture);
			ClipCursor(capture ? &data.window_rect : nullptr);
		}
	}

	HWND GetHWnd()
	{
		return data.hWnd;
	}

}
