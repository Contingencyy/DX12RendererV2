#include "Pch.h"
#include "Application.h"

namespace Application
{

	static bool g_running = false;
	static bool g_should_close = false;

	void Init()
	{
		
		
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

		while (g_running)
		{
			QueryPerformanceCounter(&current_ticks);

			elapsed = current_ticks.QuadPart - last_ticks.QuadPart;
			elapsed *= 1000000;
			elapsed /= frequency.QuadPart;

			float delta_time = (double)elapsed / 1000000;
			Update(delta_time);
			Render();

			last_ticks = current_ticks;
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