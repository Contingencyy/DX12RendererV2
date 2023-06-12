#include "Pch.h"
#include "Application.h"
#include "Window.h"
#include "Renderer.h"

namespace Application
{

	bool g_running = false;
	bool g_should_close = false;

	void Init()
	{
		Window::WindowProps window_props = {};
		window_props.name = L"DXV2 Renderer";
		window_props.width = 1280;
		window_props.height = 720;

		Window::Create(window_props);
		
		g_running = true;
	}

	void Exit()
	{


		g_running = false;
	}

	void Run()
	{
		LARGE_INTEGER current_ticks, last_ticks;
		LARGE_INTEGER frequency;
		QueryPerformanceFrequency(&frequency);
		QueryPerformanceCounter(&last_ticks);

		int64_t elapsed = 0;

		while (!g_should_close && g_running)
		{
			QueryPerformanceCounter(&current_ticks);

			elapsed = current_ticks.QuadPart - last_ticks.QuadPart;
			elapsed *= 1000000;
			elapsed /= frequency.QuadPart;

			float delta_time = (double)elapsed / 1000000;
			PollEvents();
			Update(delta_time);
			Render();

			last_ticks = current_ticks;
		}
	}

	void PollEvents()
	{
		MSG msg = {};
		while (::PeekMessage(&msg, NULL, 0, 0, PM_REMOVE) != NULL)
		{
			if (msg.message == WM_QUIT)
			{
				g_should_close = true;
				break;
			}

			::TranslateMessage(&msg);
			::DispatchMessage(&msg);
		}
	}

	void Update(float dt)
	{
		printf("%f\n", dt);
	}

	void Render()
	{

	}

	bool IsRunning()
	{
		return g_running;
	}

	bool ShouldClose()
	{
		return g_should_close;
	}

}