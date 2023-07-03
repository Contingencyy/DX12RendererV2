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
		bool running = false;
		bool should_close = false;

		ResourceHandle texture;
		Model model;
		Model model2;
	} static data;

	void Init()
	{
		// ----------------------------------------------------------------------------------
		// Initialize the window

		Window::WindowProps window_props = {};
		window_props.name = L"DXV2 Renderer";
		window_props.width = 1280;
		window_props.height = 720;

		Window::Create(window_props);

		// ----------------------------------------------------------------------------------
		// Initialize the renderer

		Renderer::RendererInitParams renderer_init_params = {};
		renderer_init_params.hWnd = Window::GetHWnd();
		renderer_init_params.width = window_props.width;
		renderer_init_params.height = window_props.height;

		Renderer::Init(renderer_init_params);

		// ----------------------------------------------------------------------------------
		// Load textures

		data.texture = AssetManager::LoadTexture("Assets/Textures/kermit.png");
		data.model = AssetManager::LoadModel("Assets/Models/ABeautifulGame/ABeautifulGame.gltf");
		data.model2 = AssetManager::LoadModel("Assets/Models/Sponza/Sponza.gltf");
		
		data.running = true;
	}

	void Exit()
	{
		data.running = false;

		Renderer::Exit();

		Window::Destroy();
	}

	void Run()
	{
		LARGE_INTEGER current_ticks, last_ticks;
		LARGE_INTEGER frequency;
		QueryPerformanceFrequency(&frequency);
		QueryPerformanceCounter(&last_ticks);

		int64_t elapsed = 0;

		while (!data.should_close && data.running)
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

		if (Input::IsKeyPressed(Input::KeyCode_LeftMouse))
		{
			Window::SetMouseCapture(true);
		}
		else if (Input::IsKeyPressed(Input::KeyCode_RightMouse))
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
		// Begin a new frame
		Renderer::BeginFrame(Scene::GetCameraView(), Scene::GetCameraProjection());

		for (uint32_t node_idx = 0; node_idx < data.model.num_nodes; ++node_idx)
		{
			Model::Node* node = &data.model.nodes[node_idx];

			for (uint32_t mesh_idx = 0; mesh_idx < node->num_meshes; ++mesh_idx)
			{
				Renderer::RenderMesh(node->mesh_handles[mesh_idx], node->texture_handles[mesh_idx], Mat4x4FromTRS(Vec3(0.0), EulerToQuat(Vec3(0.0)), Vec3(400.0)));
			}
		}

		for (uint32_t node_idx = 0; node_idx < data.model2.num_nodes; ++node_idx)
		{
			Model::Node* node = &data.model2.nodes[node_idx];

			for (uint32_t mesh_idx = 0; mesh_idx < node->num_meshes; ++mesh_idx)
			{
				Renderer::RenderMesh(node->mesh_handles[mesh_idx], node->texture_handles[mesh_idx], Mat4x4Identity());
			}
		}

		// Draw Dear ImGui menus
		Renderer::OnImGuiRender();

		// End the current frame and render it
		Renderer::EndFrame();
		Renderer::RenderFrame();
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