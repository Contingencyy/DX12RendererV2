#include "Pch.h"
#include "Application.h"
#include "Window.h"
#include "Renderer/Renderer.h"
#include "Scene.h"
#include "Input.h"
#include "AssetManager.h"

#include "imgui/imgui.h"

namespace Application
{

	struct InternalData
	{
		LARGE_INTEGER current_ticks, last_ticks;
		LARGE_INTEGER frequency;
		int64_t elapsed = 0;
		float delta_time = 0.0;

		bool running = false;
		bool should_close = false;
	} static data;

	void Init()
	{
		// ----------------------------------------------------------------------------------
		// Create the window

		Window::WindowProps window_props = {};
		window_props.name = L"DXV2 Renderer";
		window_props.width = 1280;
		window_props.height = 720;

		Window::Create(window_props);

		// ----------------------------------------------------------------------------------
		// Initialize core systems

		Renderer::RendererInitParams renderer_init_params = {};
		renderer_init_params.hWnd = Window::GetHWnd();
		renderer_init_params.width = window_props.width;
		renderer_init_params.height = window_props.height;

		Renderer::Init(renderer_init_params);
		AssetManager::Init();

		// ----------------------------------------------------------------------------------
		// Load textures

		AssetManager::LoadTexture("Assets/Textures/kermit.png");
		//AssetManager::LoadModel("Assets/Models/SponzaPBR/NewSponza_Main_glTF_002.gltf");
		AssetManager::LoadModel("Assets/Models/ABeautifulGame/ABeautifulGame.gltf");
		AssetManager::LoadModel("Assets/Models/Sponza/Sponza.gltf");
		
		data.running = true;
	}

	void Exit()
	{
		data.running = false;

		AssetManager::Exit();
		Renderer::Exit();

		Window::Destroy();
	}

	void Run()
	{
		QueryPerformanceFrequency(&data.frequency);
		QueryPerformanceCounter(&data.last_ticks);

		while (!data.should_close && data.running)
		{
			QueryPerformanceCounter(&data.current_ticks);

			data.elapsed = data.current_ticks.QuadPart - data.last_ticks.QuadPart;
			data.elapsed *= 1000000;
			data.elapsed /= data.frequency.QuadPart;

			data.delta_time = (double)data.elapsed / 1000000;

			PollEvents();
			Update(data.delta_time);
			Render();

			data.last_ticks = data.current_ticks;

			// We reset and decommit the thread local allocator every frame
			g_thread_alloc.Reset();
			g_thread_alloc.Decommit();
		}
	}

	void PollEvents()
	{
		MSG msg = {};
		while (::PeekMessage(&msg, NULL, 0, 0, PM_REMOVE) != NULL)
		{
			if (msg.message == WM_QUIT)
			{
				data.should_close = true;
				break;
			}

			::TranslateMessage(&msg);
			::DispatchMessage(&msg);
		}

		if (!ImGui::GetIO().WantCaptureMouse)
		{
			if (Input::IsKeyPressed(Input::KeyCode_LeftMouse))
			{
				Window::SetMouseCapture(true);
			}
		}
		if (Input::IsKeyPressed(Input::KeyCode_RightMouse))
		{
			Window::SetMouseCapture(false);
		}
	}

	void Update(float dt)
	{
		Input::UpdateMouseMove();
		Scene::Update(dt);
	}

	void Render()
	{
		Renderer::BeginFrame(Scene::GetCameraView(), Scene::GetCameraProjection());

		Scene::Render();

		// Render the current frame
		Renderer::RenderFrame();

		// Render ImGui
		Renderer::BeginImGuiFrame();
		Application::OnImGuiRender();
		Renderer::OnImGuiRender();
		Renderer::RenderImGui();

		// End the frame, swap buffers
		Renderer::EndFrame();
	}

	void OnImGuiRender()
	{
		ImGui::Begin("General");
		
		ImGui::Text("Delta time: %.3f ms", data.delta_time * 1000.0);
		ImGui::Text("FPS: %u", (uint32_t)(1.0 / data.delta_time));

		ImGui::End();
	}

	bool IsRunning()
	{
		return data.running;
	}

	bool ShouldClose()
	{
		return data.should_close;
	}

}