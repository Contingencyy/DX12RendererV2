#include "Pch.h"
#include "Renderer/Renderer.h"
#include "Renderer/D3DState.h"
#include "Renderer/DX12.h"

#include "imgui/imgui.h"
#include "imgui/imgui_impl_win32.h"
#include "imgui/imgui_impl_dx12.h"

// Specify the D3D12 agility SDK version and path
// This will help the D3D12.dll loader pick the right D3D12Core.dll (either the system installed or provided agility)
extern "C" { __declspec(dllexport) extern const UINT D3D12SDKVersion = 711; }
extern "C" { __declspec(dllexport) extern const char* D3D12SDKPath = ".\\D3D12\\"; }

D3DState d3d_state;

namespace Renderer
{

#define MAX_RENDER_MESHES 1000

	enum ReservedDescriptor : uint32_t
	{
		ReservedDescriptor_DearImGui,
		ReservedDescriptor_Count
	};

	struct TextureResource
	{
		ID3D12Resource* resource;
		D3D12_CPU_DESCRIPTOR_HANDLE srv_cpu;
		D3D12_GPU_DESCRIPTOR_HANDLE srv_gpu;
	};

	struct MeshResource
	{
		D3D12_VERTEX_BUFFER_VIEW vbv;
		ID3D12Resource* vertex_buffer;
		D3D12_INDEX_BUFFER_VIEW ibv;
		ID3D12Resource* index_buffer;
	};

	struct RenderMeshData
	{
		ResourceHandle mesh_handle;
		ResourceHandle texture_handle;
	};

	struct InternalData
	{
		Allocator allocator;

		ResourceSlotmap<MeshResource>* mesh_slotmap;
		ResourceSlotmap<TextureResource>* texture_slotmap;

		ResourceHandle default_white_texture;

		RenderMeshData* render_mesh_data;

		struct FrameStatistics
		{
			size_t draw_call_count;
			size_t mesh_count;
			size_t total_vertex_count;
			size_t total_triangle_count;
		} stats;
	} static data;

	static uint32_t TextureFormatBPP(TextureFormat format)
	{
		switch (format)
		{
		case TextureFormat_RGBA8:
			return 4;
		}
	}

	static DXGI_FORMAT TextureFormatToDXGIFormat(TextureFormat format)
	{
		switch (format)
		{
		case TextureFormat_RGBA8:
			return DXGI_FORMAT_R8G8B8A8_UNORM;
		case TextureFormat_D32:
			return DXGI_FORMAT_D32_FLOAT;
		}
	}

	static D3DState::FrameContext* GetFrameContextCurrent()
	{
		return &d3d_state.frame_ctx[d3d_state.current_back_buffer_idx];
	}

	static D3DState::FrameContext* GetFrameContext(uint32_t back_buffer_idx)
	{
		return &d3d_state.frame_ctx[back_buffer_idx];
	}

	static void InitD3DState(const RendererInitParams& params)
	{
#ifdef _DEBUG
		// Enable debug layer
		// TODO: Add GPU validation toggle
		ID3D12Debug* debug_controller;
		DX_FAILED_HR(D3D12GetDebugInterface(IID_PPV_ARGS(&debug_controller)))
		{
			debug_controller->EnableDebugLayer();
		}

#if DX_GPU_VALIDATION
		ID3D12Debug1* debug_controller1;
		DX_CHECK_HR_BRANCH(debug_controller->QueryInterface(IID_PPV_ARGS(&debug_controller1)))
		{
			debug_controller1->SetEnableGPUBasedValidation(true);
		}
#endif
		DX_RELEASE_OBJECT(debug_controller);
#endif

#ifdef _DEBUG
		uint32_t factory_flags = DXGI_CREATE_FACTORY_DEBUG;
#else
		uint32_t factory_flags = 0;
#endif

		// Create the dxgi factory
		IDXGIFactory7* dxgi_factory;
		DX_CHECK_HR_ERR(CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG, IID_PPV_ARGS(&dxgi_factory)), "Failed to create DXGI factory");

		// Select the adapter with the most video memory, and make sure it supports our wanted d3d feature level
		D3D_FEATURE_LEVEL d3d_min_feature_level = D3D_FEATURE_LEVEL_12_2;
		IDXGIAdapter1* dxgi_adapter;
		size_t max_dedicated_video_memory = 0;
		for (uint32_t adapter_idx = 0; dxgi_factory->EnumAdapters1(adapter_idx, &dxgi_adapter) != DXGI_ERROR_NOT_FOUND; ++adapter_idx)
		{
			DXGI_ADAPTER_DESC1 adapter_desc = {};
			DX_CHECK_HR(dxgi_adapter->GetDesc1(&adapter_desc));

			if ((adapter_desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) == 0 &&
				SUCCEEDED(D3D12CreateDevice(dxgi_adapter, d3d_min_feature_level, __uuidof(ID3D12Device), nullptr)) &&
				adapter_desc.DedicatedVideoMemory > max_dedicated_video_memory)
			{
				max_dedicated_video_memory = adapter_desc.DedicatedVideoMemory;
				DX_CHECK_HR(dxgi_adapter->QueryInterface(IID_PPV_ARGS(&d3d_state.adapter)));
			}
		}

		// Create the device
		DX_CHECK_HR_ERR(D3D12CreateDevice(d3d_state.adapter, d3d_min_feature_level, IID_PPV_ARGS(&d3d_state.device)), "Failed to create D3D12 device");

#ifdef _DEBUG
		// Set info queue severity behavior
		ID3D12InfoQueue* info_queue;
		DX_FAILED_HR(d3d_state.device->QueryInterface(IID_PPV_ARGS(&info_queue)))
		{
			DX_CHECK_HR(info_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE));
			DX_CHECK_HR(info_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE));
			DX_CHECK_HR(info_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, TRUE));
		}
#endif

		// Create the command queue for the swap chain
		d3d_state.swapchain_command_queue = DX12::CreateCommandQueue(D3D12_COMMAND_LIST_TYPE_DIRECT, D3D12_COMMAND_QUEUE_PRIORITY_NORMAL);

		// Create the swap chain
		BOOL allow_tearing = FALSE;
		DX_FAILED_HR(dxgi_factory->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allow_tearing, sizeof(BOOL)))
		{
			d3d_state.tearing_supported = (allow_tearing == TRUE);
		}

		DXGI_SWAP_CHAIN_DESC1 swap_chain_desc = {};
		swap_chain_desc.Width = params.width;
		swap_chain_desc.Height = params.height;
		swap_chain_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		swap_chain_desc.Stereo = FALSE;
		swap_chain_desc.SampleDesc = { 1, 0 };
		swap_chain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		swap_chain_desc.BufferCount = DX_BACK_BUFFER_COUNT;
		swap_chain_desc.Scaling = DXGI_SCALING_STRETCH;
		swap_chain_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		swap_chain_desc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
		swap_chain_desc.Flags = d3d_state.tearing_supported ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;

		IDXGISwapChain1* swap_chain;
		DX_CHECK_HR_ERR(dxgi_factory->CreateSwapChainForHwnd(d3d_state.swapchain_command_queue, params.hWnd, &swap_chain_desc, nullptr, nullptr, &swap_chain), "Failed to create DXGI swap chain");
		DX_CHECK_HR(swap_chain->QueryInterface(&d3d_state.swapchain));
		DX_CHECK_HR(dxgi_factory->MakeWindowAssociation(params.hWnd, DXGI_MWA_NO_ALT_ENTER));
		d3d_state.current_back_buffer_idx = d3d_state.swapchain->GetCurrentBackBufferIndex();

		d3d_state.render_width = params.width;
		d3d_state.render_height = params.height;

		// Create command queue, list, and allocator
		//d3d_state.command_queue_direct = CreateCommandQueue(D3D12_COMMAND_LIST_TYPE_DIRECT, D3D12_COMMAND_QUEUE_PRIORITY_NORMAL);
		for (uint32_t back_buffer_idx = 0; back_buffer_idx < DX_BACK_BUFFER_COUNT; ++back_buffer_idx)
		{
			D3DState::FrameContext* frame_ctx = GetFrameContext(back_buffer_idx);
			d3d_state.swapchain->GetBuffer(back_buffer_idx, IID_PPV_ARGS(&frame_ctx->back_buffer));

			DX_CHECK_HR_ERR(d3d_state.device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
				IID_PPV_ARGS(&frame_ctx->command_allocator)), "Failed to create command allocator");
			DX_CHECK_HR_ERR(d3d_state.device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, frame_ctx->command_allocator, nullptr,
				IID_PPV_ARGS(&frame_ctx->command_list)), "Failed to create command list");

			frame_ctx->instance_buffer = DX12::CreateUploadBuffer(L"Instance buffer", sizeof(InstanceData) * MAX_RENDER_MESHES);
			frame_ctx->instance_buffer->Map(0, nullptr, (void**)&frame_ctx->instance_buffer_ptr);
			frame_ctx->instance_vbv.BufferLocation = frame_ctx->instance_buffer->GetGPUVirtualAddress();
			frame_ctx->instance_vbv.StrideInBytes = sizeof(InstanceData);
			frame_ctx->instance_vbv.SizeInBytes = frame_ctx->instance_vbv.StrideInBytes * MAX_RENDER_MESHES;
		}
		DX_CHECK_HR_ERR(d3d_state.device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&d3d_state.fence)), "Failed to create fence");

		// Create depth buffer
		D3D12_CLEAR_VALUE clear_value = {};
		clear_value.Format = DXGI_FORMAT_D32_FLOAT;
		clear_value.DepthStencil.Depth = 1.0;
		clear_value.DepthStencil.Stencil = 0;
		d3d_state.depth_buffer = DX12::CreateTexture(L"Depth buffer", clear_value.Format, d3d_state.render_width, d3d_state.render_height,
			&clear_value, D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);

		// Create descriptor heaps
		d3d_state.descriptor_heap_rtv = DX12::CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, DX_DESCRIPTOR_HEAP_SIZE_RTV);
		d3d_state.descriptor_heap_dsv = DX12::CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_DSV, DX_DESCRIPTOR_HEAP_SIZE_DSV);
		d3d_state.descriptor_heap_cbv_srv_uav = DX12::CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, DX_DESCRIPTOR_HEAP_SIZE_CBV_SRV_UAV, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE);
		d3d_state.cbv_srv_uav_current_index = ReservedDescriptor_Count;

		// Create the DXC compiler state
		DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&d3d_state.dxc_compiler));
		DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&d3d_state.dxc_utils));
		d3d_state.dxc_utils->CreateDefaultIncludeHandler(&d3d_state.dxc_include_handler);

		// Create the scene constant buffer
		d3d_state.scene_cb = DX12::CreateUploadBuffer(L"Scene constant buffer", sizeof(SceneData));
		d3d_state.scene_cb->Map(0, nullptr, (void**)&d3d_state.scene_cb_ptr);

		// Create the upload buffer
		d3d_state.upload_buffer = DX12::CreateUploadBuffer(L"Generic upload buffer", DX_MB(256));
		d3d_state.upload_buffer->Map(0, nullptr, (void**)&d3d_state.upload_buffer_ptr);
	}

	static void InitPipelines()
	{
		d3d_state.default_raster_pipeline.root_sig = DX12::CreateRootSignature();
		d3d_state.default_raster_pipeline.pipeline_state = DX12::CreatePipelineState(d3d_state.default_raster_pipeline.root_sig,
			L"Include/Shaders/Default_VS_PS.hlsl", L"Include/Shaders/Default_VS_PS.hlsl");
	}

	static void InitDearImGui()
	{
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO(); (void)io;

		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
		io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

		ImGui::StyleColorsDark();

		// Init DX12 imgui backend
		HWND hWnd;
		DXGI_SWAP_CHAIN_DESC swap_chain_desc;
		DX_CHECK_HR(d3d_state.swapchain->GetHwnd(&hWnd));
		DX_CHECK_HR(d3d_state.swapchain->GetDesc(&swap_chain_desc));

		uint32_t cbv_srv_uav_increment_size = d3d_state.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		ImGui_ImplWin32_Init(hWnd);
		ImGui_ImplDX12_Init(d3d_state.device, DX_BACK_BUFFER_COUNT, swap_chain_desc.BufferDesc.Format, d3d_state.descriptor_heap_cbv_srv_uav,
			{ d3d_state.descriptor_heap_cbv_srv_uav->GetCPUDescriptorHandleForHeapStart().ptr + ReservedDescriptor_DearImGui * cbv_srv_uav_increment_size },
			{ d3d_state.descriptor_heap_cbv_srv_uav->GetGPUDescriptorHandleForHeapStart().ptr + ReservedDescriptor_DearImGui * cbv_srv_uav_increment_size });
	}

	static void ResizeBackBuffers()
	{
		// Release all back buffers
		for (uint32_t back_buffer_idx = 0; back_buffer_idx < DX_BACK_BUFFER_COUNT; ++back_buffer_idx)
		{
			D3DState::FrameContext* frame_ctx = GetFrameContext(back_buffer_idx);

			ULONG ref_count = frame_ctx->back_buffer->Release();
			// NOTE: Back buffer resources share a reference count, so we want to see if the outstanding references are actually all released
			DX_ASSERT(ref_count + back_buffer_idx == (DX_BACK_BUFFER_COUNT - 1));
			frame_ctx->back_buffer_fence_value = GetFrameContextCurrent()->back_buffer_fence_value;
		}

		// Resize swap chain back buffers and update the current back buffer index
		DXGI_SWAP_CHAIN_DESC swap_chain_desc;
		DX_CHECK_HR(d3d_state.swapchain->GetDesc(&swap_chain_desc));
		DX_CHECK_HR_ERR(d3d_state.swapchain->ResizeBuffers(3, d3d_state.render_width, d3d_state.render_height,
			swap_chain_desc.BufferDesc.Format, swap_chain_desc.Flags), "Failed to resize swap chain back buffers");

		for (uint32_t back_buffer_idx = 0; back_buffer_idx < DX_BACK_BUFFER_COUNT; ++back_buffer_idx)
		{
			D3DState::FrameContext* frame_ctx = GetFrameContext(back_buffer_idx);
			DX_CHECK_HR(d3d_state.swapchain->GetBuffer(back_buffer_idx, IID_PPV_ARGS(&frame_ctx->back_buffer)));
		}

		d3d_state.current_back_buffer_idx = d3d_state.swapchain->GetCurrentBackBufferIndex();
	}

	static void ResizeRenderResolution(uint32_t new_width, uint32_t new_height)
	{
		d3d_state.render_width = new_width;
		d3d_state.render_height = new_height;

		DX_RELEASE_OBJECT(d3d_state.depth_buffer);
		D3D12_CLEAR_VALUE clear_value = {};
		clear_value.Format = DXGI_FORMAT_D32_FLOAT;
		clear_value.DepthStencil.Depth = 1.0;
		clear_value.DepthStencil.Stencil = 0;
		d3d_state.depth_buffer = DX12::CreateTexture(L"Depth buffer", clear_value.Format, d3d_state.render_width, d3d_state.render_height,
			&clear_value, D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);
	}

	void Init(const RendererInitParams& params)
	{
		// Creates the adapter, device, command queue and swapchain, etc.
		InitD3DState(params);
		InitPipelines();
		InitDearImGui();

		// Initialize slotmaps
		data.texture_slotmap = data.allocator.AllocateConstruct<ResourceSlotmap<TextureResource>>(&data.allocator);
		data.mesh_slotmap = data.allocator.AllocateConstruct<ResourceSlotmap<MeshResource>>(&data.allocator);

		data.render_mesh_data = data.allocator.Allocate<RenderMeshData>(1000);

		// Default textures
		{
			uint32_t white_texture_data = 0xFFFFFFFF;
			UploadTextureParams upload_texture_params = {};
			upload_texture_params.width = 1;
			upload_texture_params.height = 1;
			upload_texture_params.format = TextureFormat_RGBA8;
			upload_texture_params.bytes = (uint8_t*)&white_texture_data;
			upload_texture_params.name = "Default white texture";
			data.default_white_texture = Renderer::UploadTexture(upload_texture_params);
		}

		d3d_state.initialized = true;
	}

	void Exit()
	{
		Flush();

		ImGui_ImplDX12_Shutdown();
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();
	}

	void Flush()
	{
		DX12::WaitOnFence(d3d_state.swapchain_command_queue, d3d_state.fence, d3d_state.fence_value);
	}

	void BeginFrame(const Mat4x4& view, const Mat4x4& projection)
	{
		d3d_state.scene_cb_ptr->view = view;
		d3d_state.scene_cb_ptr->projection = projection;
		d3d_state.scene_cb_ptr->view_projection = Mat4x4Mul(d3d_state.scene_cb_ptr->view, d3d_state.scene_cb_ptr->projection);

		// ----------------------------------------------------------------------------------
		// Wait on the current back buffer until all commands on it have finished execution

		// We wait here right before actually using the back buffer again to minimize the amount of time
		// to maximize the amount of work the CPU can do before actually having to wait on the GPU.
		// Some examples on the internet wait right after presenting, which is not ideal

		D3DState::FrameContext* frame_ctx = GetFrameContextCurrent();
		DX12::WaitOnFence(d3d_state.swapchain_command_queue, d3d_state.fence, frame_ctx->back_buffer_fence_value);

		// ----------------------------------------------------------------------------------
		// Reset the command allocator and command list for the current frame

		// If the command list has not been closed before, this will crash, so we need to check here if the fence value for the next back buffer is bigger than 0
		// which means that the command list was closed before once
		// ID3D12CommandList::Close will set the command list into non recording state
		// ID3D12CommandList::Reset will set the command list into recording state, this can be called even if the commands from this command list are still in-flight
		// ID3D12CommandAllocator::Reset can only be called when the commands from that allocator are no longer in-flight (one allocator per back buffer seems reasonable)
		// Only one command list associated with a command allocator can be in the recording state at any given time, which means that for each thread that populates
		// a command list, you need at least one command allocator and at least one command list
		if (frame_ctx->back_buffer_fence_value > 0)
		{
			frame_ctx->command_allocator->Reset();
			frame_ctx->command_list->Reset(frame_ctx->command_allocator, nullptr);
		}
	}

	void RenderFrame()
	{
		// ----------------------------------------------------------------------------------
		// Create the render target view for the current back buffer

		D3DState::FrameContext* frame_ctx = GetFrameContextCurrent();

		D3D12_RENDER_TARGET_VIEW_DESC rtv_desc = {};
		rtv_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		rtv_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
		rtv_desc.Texture2D.MipSlice = 0;
		rtv_desc.Texture2D.PlaneSlice = 0;

		ID3D12Resource* back_buffer = frame_ctx->back_buffer;
		d3d_state.device->CreateRenderTargetView(back_buffer, &rtv_desc, d3d_state.descriptor_heap_rtv->GetCPUDescriptorHandleForHeapStart());

		D3D12_DEPTH_STENCIL_VIEW_DESC dsv_desc = {};
		dsv_desc.Format = DXGI_FORMAT_D32_FLOAT;
		dsv_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
		dsv_desc.Texture2D.MipSlice = 0;
		dsv_desc.Flags = D3D12_DSV_FLAG_NONE;

		d3d_state.device->CreateDepthStencilView(d3d_state.depth_buffer, &dsv_desc, d3d_state.descriptor_heap_dsv->GetCPUDescriptorHandleForHeapStart());
		
		// ----------------------------------------------------------------------------------
		// Transition the back buffer to the render target state, and clear it

		ID3D12GraphicsCommandList6* cmd_list = frame_ctx->command_list;

		D3D12_RESOURCE_BARRIER render_target_barrier = DX12::TransitionBarrier(back_buffer, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
		cmd_list->ResourceBarrier(1, &render_target_barrier);

		float clear_color[4] = { 1, 0, 1, 1 };
		cmd_list->ClearRenderTargetView(d3d_state.descriptor_heap_rtv->GetCPUDescriptorHandleForHeapStart(), clear_color, 0, nullptr);
		cmd_list->ClearDepthStencilView(d3d_state.descriptor_heap_dsv->GetCPUDescriptorHandleForHeapStart(), D3D12_CLEAR_FLAG_DEPTH, 1.0, 0, 0, nullptr);

		// ----------------------------------------------------------------------------------
		// Draw a triangle

		D3D12_VIEWPORT viewport = { 0.0, 0.0, d3d_state.render_width, d3d_state.render_height, 0.0, 1.0 };
		D3D12_RECT scissor_rect = { 0, 0, LONG_MAX, LONG_MAX };

		cmd_list->RSSetViewports(1, &viewport);
		cmd_list->RSSetScissorRects(1, &scissor_rect);

		D3D12_CPU_DESCRIPTOR_HANDLE rtv_handle = d3d_state.descriptor_heap_rtv->GetCPUDescriptorHandleForHeapStart();
		D3D12_CPU_DESCRIPTOR_HANDLE dsv_handle = d3d_state.descriptor_heap_dsv->GetCPUDescriptorHandleForHeapStart();
		cmd_list->OMSetRenderTargets(1, &rtv_handle, FALSE, &dsv_handle);

		cmd_list->SetGraphicsRootSignature(d3d_state.default_raster_pipeline.root_sig);
		cmd_list->SetPipelineState(d3d_state.default_raster_pipeline.pipeline_state);
		cmd_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		ID3D12DescriptorHeap* const descriptor_heaps = { d3d_state.descriptor_heap_cbv_srv_uav };
		cmd_list->SetDescriptorHeaps(1, &descriptor_heaps);
		uint32_t cbv_srv_uav_increment_size = d3d_state.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		cmd_list->SetGraphicsRootConstantBufferView(0, d3d_state.scene_cb->GetGPUVirtualAddress());
		cmd_list->IASetVertexBuffers(1, 1, &frame_ctx->instance_vbv);

		for (size_t mesh = 0; mesh < data.stats.mesh_count; ++mesh)
		{
			RenderMeshData* mesh_data = &data.render_mesh_data[mesh];
			MeshResource* mesh_resource = data.mesh_slotmap->Find(mesh_data->mesh_handle);
			TextureResource* texture_resource = data.texture_slotmap->Find(mesh_data->texture_handle);
			
			cmd_list->IASetVertexBuffers(0, 1, &mesh_resource->vbv);
			cmd_list->IASetIndexBuffer(&mesh_resource->ibv);
			cmd_list->DrawIndexedInstanced(mesh_resource->ibv.SizeInBytes / 4, 1, 0, 0, mesh);

			data.stats.draw_call_count++;
			data.stats.total_vertex_count += mesh_resource->ibv.SizeInBytes / 4;
			data.stats.total_triangle_count += mesh_resource->ibv.SizeInBytes / 4 / 3;
		}
	}

	void EndFrame()
	{
		// ----------------------------------------------------------------------------------
		// Transition the back buffer to the present state

		D3DState::FrameContext* frame_ctx = GetFrameContextCurrent();
		ID3D12GraphicsCommandList6* cmd_list = frame_ctx->command_list;
		D3D12_RESOURCE_BARRIER present_barrier = DX12::TransitionBarrier(frame_ctx->back_buffer, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
		cmd_list->ResourceBarrier(1, &present_barrier);

		// ----------------------------------------------------------------------------------
		// Execute the command list for the current frame

		DX12::ExecuteCommandList(d3d_state.swapchain_command_queue, cmd_list);

		// ----------------------------------------------------------------------------------
		// Present the back buffer on the swap chain

		uint32_t sync_interval = d3d_state.vsync_enabled ? 1 : 0;
		uint32_t present_flags = d3d_state.tearing_supported && !d3d_state.vsync_enabled ? DXGI_PRESENT_ALLOW_TEARING : 0;
		DX_CHECK_HR(d3d_state.swapchain->Present(sync_interval, present_flags));

		// ----------------------------------------------------------------------------------
		// Signal the GPU with the fence value for the current frame, and set the current back buffer index to the next one

		frame_ctx->back_buffer_fence_value = ++d3d_state.fence_value;
		d3d_state.swapchain_command_queue->Signal(d3d_state.fence, frame_ctx->back_buffer_fence_value);
		d3d_state.current_back_buffer_idx = d3d_state.swapchain->GetCurrentBackBufferIndex();

		// ----------------------------------------------------------------------------------
		// Increase the total frame index and reset the render statistics

		d3d_state.frame_index++;
		data.stats = { 0 };
	}

	void RenderMesh(ResourceHandle mesh_handle, ResourceHandle texture_handle, const Mat4x4& transform)
	{
		DX_ASSERT(data.stats.mesh_count < MAX_RENDER_MESHES);
		data.render_mesh_data[data.stats.mesh_count] =
		{
			// TODO: Default mesh handle? (e.g. Cube)
			.mesh_handle = mesh_handle,
			.texture_handle = DX_RESOURCE_HANDLE_VALID(texture_handle) ? texture_handle : data.default_white_texture
		};

		TextureResource* texture_resource = data.texture_slotmap->Find(data.render_mesh_data[data.stats.mesh_count].texture_handle);
		GetFrameContextCurrent()->instance_buffer_ptr[data.stats.mesh_count].transform = transform;
		GetFrameContextCurrent()->instance_buffer_ptr[data.stats.mesh_count].base_color_index =
			(texture_resource->srv_cpu.ptr - d3d_state.descriptor_heap_cbv_srv_uav->GetCPUDescriptorHandleForHeapStart().ptr) / d3d_state.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		data.stats.mesh_count++;
	}

	ResourceHandle UploadTexture(const UploadTextureParams& params)
	{
		ID3D12Resource* resource = DX12::CreateTexture(DX12::UTF16FromUTF8(&g_thread_alloc, params.name), TextureFormatToDXGIFormat(params.format), params.width, params.height);

		D3DState::FrameContext* frame_ctx = GetFrameContextCurrent();
		ID3D12GraphicsCommandList6* cmd_list = frame_ctx->command_list;
		D3D12_RESOURCE_BARRIER copy_dst_barrier = DX12::TransitionBarrier(resource,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
			D3D12_RESOURCE_STATE_COPY_DEST);
		cmd_list->ResourceBarrier(1, &copy_dst_barrier);

		D3D12_RESOURCE_DESC dst_desc = resource->GetDesc();
		D3D12_SUBRESOURCE_FOOTPRINT dst_footprint = {};
		dst_footprint.Format = resource->GetDesc().Format;
		dst_footprint.Width = params.width;
		dst_footprint.Height = params.height;
		dst_footprint.Depth = 1;
		dst_footprint.RowPitch = DX_ALIGN_POW2(params.width * TextureFormatBPP(params.format), D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);

		D3D12_PLACED_SUBRESOURCE_FOOTPRINT src_footprint = {};
		src_footprint.Footprint = dst_footprint;
		src_footprint.Offset = 0;

		uint8_t* src_ptr = params.bytes;
		uint8_t* dst_ptr = d3d_state.upload_buffer_ptr;
		uint32_t dst_pitch = dst_footprint.RowPitch;

		for (uint32_t y = 0; y < params.height; ++y)
		{
			memcpy(dst_ptr, src_ptr, dst_pitch);
			src_ptr += params.width * TextureFormatBPP(params.format);
			dst_ptr += dst_pitch;
		}

		D3D12_TEXTURE_COPY_LOCATION src_loc = {};
		src_loc.pResource = d3d_state.upload_buffer;
		src_loc.PlacedFootprint = src_footprint;
		src_loc.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;

		D3D12_TEXTURE_COPY_LOCATION dst_loc = {};
		dst_loc.pResource = resource;
		dst_loc.SubresourceIndex = 0;
		dst_loc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;

		cmd_list->CopyTextureRegion(&dst_loc, 0, 0, 0, &src_loc, nullptr);

		D3D12_RESOURCE_BARRIER pixel_src_barrier = DX12::TransitionBarrier(resource,
			D3D12_RESOURCE_STATE_COPY_DEST,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		cmd_list->ResourceBarrier(1, &pixel_src_barrier);
		
		// Execute the command list, wait on it, and reset it
		DX12::ExecuteCommandList(d3d_state.swapchain_command_queue, cmd_list);
		cmd_list->Reset(frame_ctx->command_allocator, nullptr);

		uint64_t fence_value = ++d3d_state.fence_value;
		DX12::WaitOnFence(d3d_state.swapchain_command_queue, d3d_state.fence, fence_value);

		D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
		srv_desc.Format = resource->GetDesc().Format;
		srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srv_desc.Texture2D.MipLevels = 1;
		srv_desc.Texture2D.MostDetailedMip = 0;
		srv_desc.Texture2D.PlaneSlice = 0;
		srv_desc.Texture2D.ResourceMinLODClamp = 0;

		uint32_t cbv_srv_uav_increment_size = d3d_state.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		
		TextureResource texture_resource = {};
		texture_resource.resource = resource;
		texture_resource.srv_cpu = { d3d_state.descriptor_heap_cbv_srv_uav->GetCPUDescriptorHandleForHeapStart().ptr + d3d_state.cbv_srv_uav_current_index * cbv_srv_uav_increment_size };
		texture_resource.srv_gpu = { d3d_state.descriptor_heap_cbv_srv_uav->GetGPUDescriptorHandleForHeapStart().ptr + d3d_state.cbv_srv_uav_current_index * cbv_srv_uav_increment_size };
		d3d_state.device->CreateShaderResourceView(resource, &srv_desc, texture_resource.srv_cpu);

		d3d_state.cbv_srv_uav_current_index++;
		return data.texture_slotmap->Insert(texture_resource);
	}

	ResourceHandle UploadMesh(const UploadMeshParams& params)
	{
		size_t vb_total_bytes = params.num_vertices * sizeof(Vertex);
		size_t ib_total_bytes = params.num_indices * sizeof(uint32_t);

		ID3D12Resource* vertex_buffer = DX12::CreateBuffer(L"Vertex buffer", vb_total_bytes);
		ID3D12Resource* index_buffer = DX12::CreateBuffer(L"Index buffer", ib_total_bytes);

		memcpy(d3d_state.upload_buffer_ptr, params.vertices, vb_total_bytes);
		memcpy(d3d_state.upload_buffer_ptr + vb_total_bytes, params.indices, ib_total_bytes);

		D3DState::FrameContext* frame_ctx = GetFrameContextCurrent();
		ID3D12GraphicsCommandList6* cmd_list = frame_ctx->command_list;
		cmd_list->CopyBufferRegion(vertex_buffer, 0, d3d_state.upload_buffer, 0, vb_total_bytes);
		cmd_list->CopyBufferRegion(index_buffer, 0, d3d_state.upload_buffer, vb_total_bytes, ib_total_bytes);

		D3D12_RESOURCE_BARRIER barriers[] =
		{
			DX12::TransitionBarrier(vertex_buffer, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER),
			DX12::TransitionBarrier(index_buffer, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_INDEX_BUFFER)
		};
		cmd_list->ResourceBarrier(2, barriers);

		DX12::ExecuteCommandList(d3d_state.swapchain_command_queue, cmd_list);
		cmd_list->Reset(frame_ctx->command_allocator, nullptr);

		uint64_t fence_value = ++d3d_state.fence_value;
		DX12::WaitOnFence(d3d_state.swapchain_command_queue, d3d_state.fence, fence_value);

		MeshResource mesh_resource = {};
		mesh_resource.vertex_buffer = vertex_buffer;
		mesh_resource.vbv.BufferLocation = vertex_buffer->GetGPUVirtualAddress();
		mesh_resource.vbv.StrideInBytes = sizeof(Vertex);
		mesh_resource.vbv.SizeInBytes = vb_total_bytes;
		mesh_resource.index_buffer = index_buffer;
		mesh_resource.ibv.BufferLocation = index_buffer->GetGPUVirtualAddress();
		mesh_resource.ibv.Format = DXGI_FORMAT_R32_UINT;
		mesh_resource.ibv.SizeInBytes = ib_total_bytes;
		return data.mesh_slotmap->Insert(mesh_resource);
	}

	void OnWindowResize(uint32_t new_width, uint32_t new_height)
	{
		new_width = DX_MAX(1u, new_width);
		new_height = DX_MAX(1u, new_height);

		if (d3d_state.render_width != new_width ||
			d3d_state.render_height != new_height)
		{
			Flush();
			// NOTE: We always have to resize the back buffers to the window rect
			// But for now we also resize the actual render resolution
			ResizeRenderResolution(new_width, new_height);
			ResizeBackBuffers();
		}
	}

	void OnImGuiRender()
	{
		DXGI_QUERY_VIDEO_MEMORY_INFO local_mem_info = {};
		d3d_state.adapter->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &local_mem_info);

		DXGI_QUERY_VIDEO_MEMORY_INFO non_local_mem_info = {};
		d3d_state.adapter->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_NON_LOCAL, &non_local_mem_info);

		ImGui::Begin("Renderer");

		ImGui::SetNextItemOpen(true, ImGuiCond_Once);
		if (ImGui::CollapsingHeader("General"))
		{
			ImGui::Text("Back buffer count: %u", DX_BACK_BUFFER_COUNT);
			ImGui::Text("Current back buffer: %u", d3d_state.current_back_buffer_idx);
		}

		ImGui::SetNextItemOpen(true, ImGuiCond_Once);
		if (ImGui::CollapsingHeader("Settings"))
		{
			ImGui::Text("Resolution: %ux%u", d3d_state.render_width, d3d_state.render_height);
			ImGui::Checkbox("VSync", &d3d_state.vsync_enabled);
			ImGui::Text("Tearing: %s", d3d_state.tearing_supported ? "true" : "false");
		}

		ImGui::SetNextItemOpen(true, ImGuiCond_Once);
		if (ImGui::CollapsingHeader("Statistics"))
		{
			ImGui::Text("Draw calls: %u", data.stats.draw_call_count);
			ImGui::Text("Total vertex count: %u", data.stats.total_vertex_count);
			ImGui::Text("Total triangle count: %u", data.stats.total_triangle_count);
		}

		ImGui::SetNextItemOpen(true, ImGuiCond_Once);
		if (ImGui::CollapsingHeader("GPU Memory"))
		{
			ImGui::Text("Local budget: %u MB", DX_TO_MB(local_mem_info.Budget));
			ImGui::Text("Local usage: %u MB", DX_TO_MB(local_mem_info.CurrentUsage));
			ImGui::Text("Local reserved: %u MB", DX_TO_MB(local_mem_info.CurrentReservation));
			ImGui::Text("Local available: %u MB", DX_TO_MB(local_mem_info.AvailableForReservation));

			ImGui::Text("Non-local budget: %u MB", DX_TO_MB(non_local_mem_info.Budget));
			ImGui::Text("Non-local usage: %u MB", DX_TO_MB(non_local_mem_info.CurrentUsage));
			ImGui::Text("Non-local reserved: %u MB", DX_TO_MB(non_local_mem_info.CurrentReservation));
			ImGui::Text("Non-local available: %u MB", DX_TO_MB(non_local_mem_info.AvailableForReservation));
		}

		ImGui::End();
	}

	void BeginImGuiFrame()
	{
		ImGui_ImplWin32_NewFrame();
		ImGui_ImplDX12_NewFrame();
		ImGui::NewFrame();
	}

	void RenderImGui()
	{
		ID3D12GraphicsCommandList6* cmd_list = GetFrameContextCurrent()->command_list;

		ImGui::Render();

		D3D12_CPU_DESCRIPTOR_HANDLE imgui_srv_handle = d3d_state.descriptor_heap_rtv->GetCPUDescriptorHandleForHeapStart();
		cmd_list->OMSetRenderTargets(1, &imgui_srv_handle, FALSE, nullptr);
		cmd_list->SetDescriptorHeaps(1, &d3d_state.descriptor_heap_cbv_srv_uav);

		ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), cmd_list);
	}

	bool IsInitialized()
	{
		return d3d_state.initialized;
	}

}
