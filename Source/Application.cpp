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

	static void RenderModelNode(const Model& model, const Model::Node& node, Mat4x4& current_transform)
	{
		for (uint32_t mesh_idx = 0; mesh_idx < node.num_meshes; ++mesh_idx)
		{
			Renderer::RenderMesh(node.mesh_handles[mesh_idx], node.texture_handles[mesh_idx], current_transform);
		}

		for (uint32_t child_idx = 0; child_idx < node.num_children; ++child_idx)
		{
			const Model::Node& child_node = model.nodes[node.children[child_idx]];
			RenderModelNode(model, child_node, Mat4x4Mul(child_node.transform, current_transform));
		}
	}

	static void RenderModel(const Model& model, Mat4x4& current_transform)
	{
		for (uint32_t root_node_idx = 0; root_node_idx < model.num_root_nodes; ++root_node_idx)
		{
			const Model::Node& root_node = model.nodes[model.root_nodes[root_node_idx]];
			RenderModelNode(model, root_node, Mat4x4Mul(root_node.transform, current_transform));
		}
	}

	void Render()
	{
		Renderer::BeginFrame(Scene::GetCameraView(), Scene::GetCameraProjection());

		Mat4x4 model_transform = Mat4x4FromTRS(Vec3(0.0), EulerToQuat(Vec3(0.0)), Vec3(100.0));
		RenderModel(data.model, model_transform);
		RenderModel(data.model2, model_transform);

		// Render the current frame
		Renderer::RenderFrame();

		// Render ImGui
		Renderer::BeginImGuiFrame();
		Renderer::OnImGuiRender();
		Renderer::RenderImGui();

		// End the frame, swap buffers
		Renderer::EndFrame();
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