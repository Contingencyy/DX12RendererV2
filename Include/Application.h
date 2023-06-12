#pragma once

namespace Application
{

	void Init();
	void Run();
	void Exit();
	
	void PollEvents();
	void Update(float dt);
	void Render();

	// As long as the application is running, the main function will call update and render
	bool IsRunning();
	// If the application should close, the main function will exit and close instead of just exiting and initializing again
	bool ShouldClose();

}
