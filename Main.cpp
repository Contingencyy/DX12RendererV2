#define NOMINMAX
#define WINDOWS_LEAN_AND_MEAN
#include <Windows.h>

#include "Application.h"

int main(int argc, char* argv[])
{
	// First order of business, set the DPI context awareness to be per monitor aware
	// We do not want windows to do anything weird to our window with DPI scaling
	SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

	// We use a double loop here to allow the application to be restarted if necessary instead of closing entirely
	while (!Application::ShouldClose())
	{
		Application::Init();

		while (Application::IsRunning())
		{
			Application::Run();
		}
		
		Application::Exit();
	}
}
