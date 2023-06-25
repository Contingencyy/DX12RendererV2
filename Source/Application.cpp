#include "Pch.h"
#include "Application.h"
#include "Window.h"
#include "Renderer/Renderer.h"
#include "FileIO.h"
#include "Scene.h"

#include "imgui/imgui.h"

namespace Application
{

	struct InternalData
	{
		bool running = false;
		bool should_close = false;
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

		FileIO::LoadImageResult texture = FileIO::LoadImage("Assets/Textures/kermit.png");
		Renderer::UploadTextureParams texture_params = {};
		texture_params.format = Renderer::TextureFormat_RGBA8;
		texture_params.width = texture.width;
		texture_params.height = texture.height;
		texture_params.bytes = texture.bytes;
		texture_params.name = L"Assets/Textures/kermit.png";
		Renderer::UploadTexture(texture_params);
		delete texture_params.bytes;

		FileIO::LoadGLTFResult mesh = FileIO::LoadGLTF("Assets/Models/ABeautifulGame/ABeautifulGame.gltf");
		for (uint32_t i = 0; i < mesh.num_meshes; ++i)
		{
			Renderer::UploadMesh(mesh.mesh_params[i]);
		}
		
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
	}

	void Update(float dt)
	{
		Scene::Update(dt);
	}

	void Render()
	{
		// Begin a new frame
		Renderer::BeginFrame(Scene::GetCameraView(), Scene::GetCameraProjection());

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