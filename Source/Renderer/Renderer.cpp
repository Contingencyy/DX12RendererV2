#include "Pch.h"
#include "Renderer/Renderer.h"
#include "Renderer/D3DState.h"
#include "Renderer/DX12.h"
#include "Renderer/ResourceTracker.h"

#include "imgui/imgui.h"
#include "imgui/imgui_impl_win32.h"
#include "imgui/imgui_impl_dx12.h"
#include "implot/implot.h"

// Specify the D3D12 agility SDK version and path
// This will help the D3D12.dll loader pick the right D3D12Core.dll (either the system installed or provided agility)
extern "C" { __declspec(dllexport) extern const UINT D3D12SDKVersion = 711; }
extern "C" { __declspec(dllexport) extern const char* D3D12SDKPath = ".\\D3D12\\"; }

D3DState d3d_state;

namespace Renderer
{

#define MAX_RENDER_MESHES 1000

	struct TextureResource
	{
		ID3D12Resource* resource;
		DescriptorAllocation srv;
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
		Material material;
	};

	struct InternalData
	{
		LinearAllocator alloc;
		MemoryScope memory_scope;

		ResourceSlotmap<MeshResource>* mesh_slotmap;
		ResourceSlotmap<TextureResource>* texture_slotmap;

		TextureResource* default_white_texture;
		ResourceHandle default_white_texture_handle;
		TextureResource* default_normal_texture;
		ResourceHandle default_normal_texture_handle;

		RenderMeshData* render_mesh_data;

		RenderSettings settings = {
			.pbr = {
				.use_linear_perceptual_roughness = 1,
				.diffuse_brdf = PBR_DIFFUSE_BRDF_LAMBERT,
			},
			.post_process = {
				.tonemap_operator = TONEMAP_OP_REINHARD_LUM_WHITE,
				.gamma = 1.0,
				.exposure = 1.5,
				.max_white = 50.0,
			}
		};

		struct FrameStatistics
		{
			size_t draw_call_count;
			size_t mesh_count;
			size_t total_vertex_count;
			size_t total_triangle_count;
		} stats;
	} static data;

	static const char* DiffuseBRDFName(uint32_t diffuse_brdf)
	{
		switch (diffuse_brdf)
		{
		case PBR_DIFFUSE_BRDF_LAMBERT:
			return "Lambert";
		case PBR_DIFFUSE_BRDF_BURLEY:
			return "Burley";
		case PBR_DIFFUSE_BRDF_OREN_NAYAR:
			return "Oren-Nayar";
		default:
			return "Undefined";
		}
	}

	static const char* TonemapOperatorName(uint32_t tonemap_operator)
	{
		switch (tonemap_operator)
		{
		case TONEMAP_OP_REINHARD_RGB:
			return "Reinhard RGB";
		case TONEMAP_OP_REINHARD_RGB_WHITE:
			return "Reinhard RGB white";
		case TONEMAP_OP_REINHARD_LUM_WHITE:
			return "Reinhard luminance white";
		case TONEMAP_OP_UNCHARTED2:
			return "Uncharted2";
		default:
			return "Undefined";
		}
	}

	static uint32_t TextureFormatBPP(TextureFormat format)
	{
		switch (format)
		{
		case TextureFormat_RGBA8_Unorm:
		case TextureFormat_D32_Float:
			return 4;
		case TextureFormat_RGBA16_Float:
			return 8;
		}
	}

	static DXGI_FORMAT TextureFormatToDXGIFormat(TextureFormat format)
	{
		switch (format)
		{
		case TextureFormat_RGBA8_Unorm:
			return DXGI_FORMAT_R8G8B8A8_UNORM;
		case TextureFormat_RGBA16_Float:
			return DXGI_FORMAT_R16G16B16A16_FLOAT;
		case TextureFormat_D32_Float:
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

	static void CreateRenderTargets()
	{
		// Create HDR render target
		{
			D3D12_CLEAR_VALUE clear_value = {};
			clear_value.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
			clear_value.Color[0] = clear_value.Color[2] = clear_value.Color[3] = 1.0;
			clear_value.Color[1] = 0.0;

			d3d_state.hdr_render_target = DX12::CreateTexture(L"HDR render target", clear_value.Format, d3d_state.render_width, d3d_state.render_height,
				D3D12_RESOURCE_STATE_COMMON, &clear_value, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
			DX12::CreateTextureRTV(d3d_state.hdr_render_target, d3d_state.reserved_rtvs.GetCPUHandle(ReservedDescriptorRTV_HDRRenderTarget), clear_value.Format);
			DX12::CreateTextureSRV(d3d_state.hdr_render_target, d3d_state.reserved_cbv_srv_uavs.GetCPUHandle(ReservedDescriptorSRV_HDRRenderTarget),
				clear_value.Format);
		}

		// Create SDR render target
		{
			D3D12_CLEAR_VALUE clear_value = {};
			clear_value.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			clear_value.Color[0] = clear_value.Color[2] = clear_value.Color[3] = 1.0;
			clear_value.Color[1] = 0.0;

			d3d_state.sdr_render_target = DX12::CreateTexture(L"SDR render target", clear_value.Format, d3d_state.render_width, d3d_state.render_height,
				D3D12_RESOURCE_STATE_COMMON, &clear_value, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
			DX12::CreateTextureRTV(d3d_state.sdr_render_target, d3d_state.reserved_rtvs.GetCPUHandle(ReservedDescriptorRTV_SDRRenderTarget), clear_value.Format);
			DX12::CreateTextureUAV(d3d_state.sdr_render_target, d3d_state.reserved_cbv_srv_uavs.GetCPUHandle(ReservedDescriptorUAV_SDRRenderTarget), clear_value.Format);
		}

		// Create depth buffer
		{
			D3D12_CLEAR_VALUE clear_value = {};
			clear_value.Format = DXGI_FORMAT_D32_FLOAT;
			clear_value.DepthStencil.Depth = 1.0;
			clear_value.DepthStencil.Stencil = 0;

			d3d_state.depth_buffer = DX12::CreateTexture(L"Depth buffer", clear_value.Format, d3d_state.render_width, d3d_state.render_height,
				D3D12_RESOURCE_STATE_DEPTH_WRITE, &clear_value, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);
			DX12::CreateTextureDSV(d3d_state.depth_buffer, d3d_state.reserved_dsvs.GetCPUHandle(ReservedDescriptorDSV_DepthBuffer), clear_value.Format);
		}
	}

	static void InitD3DState(const RendererInitParams& params)
	{
		d3d_state.render_width = params.width;
		d3d_state.render_height = params.height;

#ifdef _DEBUG
		// Enable debug layer
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
		DX_RELEASE_INTERFACE(debug_controller1);
#endif
		DX_RELEASE_INTERFACE(debug_controller);
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
				DX_CHECK_HR(dxgi_adapter->GetDesc1(&d3d_state.adapter_desc));
				DX_RELEASE_INTERFACE(d3d_state.adapter);
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
		DX_RELEASE_INTERFACE(info_queue);
#endif

		// Create the swap chain
		BOOL allow_tearing = FALSE;
		DX_FAILED_HR(dxgi_factory->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allow_tearing, sizeof(BOOL)))
		{
			d3d_state.tearing_supported = (allow_tearing == TRUE);
		}

		// Check feature support for required features
		D3D12_FEATURE_DATA_D3D12_OPTIONS12 feature_options12 = {};
		d3d_state.device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS12, &feature_options12, sizeof(feature_options12));
		DX_ASSERT(feature_options12.EnhancedBarriersSupported && "DirectX 12 Enhanced barriers are not supported");
		
		/*D3D12_FEATURE_DATA_D3D12_OPTIONS16 feature_options16 = {};
		d3d_state.device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS16, &feature_options16, sizeof(feature_options16));
		DX_ASSERT(feature_options16.GPUUploadHeapSupported && "DirectX 12 GPU upload heaps are not supported");*/

		// Create the command queue for the swap chain
		d3d_state.swapchain_command_queue = DX12::CreateCommandQueue(D3D12_COMMAND_LIST_TYPE_DIRECT, D3D12_COMMAND_QUEUE_PRIORITY_NORMAL);

		DXGI_SWAP_CHAIN_DESC1 swap_chain_desc = {};
		swap_chain_desc.Width = params.width;
		swap_chain_desc.Height = params.height;
		swap_chain_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		swap_chain_desc.Stereo = FALSE;
		swap_chain_desc.SampleDesc = { 1, 0 };
		swap_chain_desc.BufferUsage = DXGI_USAGE_BACK_BUFFER;
		swap_chain_desc.BufferCount = DX_BACK_BUFFER_COUNT;
		swap_chain_desc.Scaling = DXGI_SCALING_STRETCH;
		swap_chain_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		swap_chain_desc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
		swap_chain_desc.Flags = d3d_state.tearing_supported ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;

		IDXGISwapChain1* swap_chain;
		DX_CHECK_HR_ERR(dxgi_factory->CreateSwapChainForHwnd(d3d_state.swapchain_command_queue, params.hWnd, &swap_chain_desc, nullptr, nullptr, &swap_chain), "Failed to create DXGI swap chain");
		DX_CHECK_HR(swap_chain->QueryInterface(&d3d_state.swapchain));
		DX_CHECK_HR(dxgi_factory->MakeWindowAssociation(params.hWnd, DXGI_MWA_NO_ALT_ENTER));
		DX_RELEASE_INTERFACE(d3d_state.swapchain);
		d3d_state.current_back_buffer_idx = d3d_state.swapchain->GetCurrentBackBufferIndex();

		// Create descriptor heaps
		d3d_state.descriptor_heap_rtv = data.memory_scope.New<DescriptorHeap>(&data.memory_scope, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, DX_DESCRIPTOR_HEAP_SIZE_RTV);
		d3d_state.descriptor_heap_dsv = data.memory_scope.New<DescriptorHeap>(&data.memory_scope, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, DX_DESCRIPTOR_HEAP_SIZE_DSV);
		d3d_state.descriptor_heap_cbv_srv_uav = data.memory_scope.New<DescriptorHeap>(&data.memory_scope, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, DX_DESCRIPTOR_HEAP_SIZE_CBV_SRV_UAV);

		// Allocate reserved descriptors
		d3d_state.reserved_rtvs = d3d_state.descriptor_heap_rtv->Allocate(ReservedDescriptorRTV_Count);
		d3d_state.reserved_dsvs = d3d_state.descriptor_heap_dsv->Allocate(ReservedDescriptorDSV_Count);
		d3d_state.reserved_cbv_srv_uavs = d3d_state.descriptor_heap_cbv_srv_uav->Allocate(ReservedDescriptorCBVSRVUAV_Count);

		CreateRenderTargets();

		//d3d_state.command_queue_direct = CreateCommandQueue(D3D12_COMMAND_LIST_TYPE_DIRECT, D3D12_COMMAND_QUEUE_PRIORITY_NORMAL);
		for (uint32_t back_buffer_idx = 0; back_buffer_idx < DX_BACK_BUFFER_COUNT; ++back_buffer_idx)
		{
			D3DState::FrameContext* frame_ctx = GetFrameContext(back_buffer_idx);
			frame_ctx->back_buffer_fence_value = 0;
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

			// Create the render settings constant buffer
			frame_ctx->render_settings_cb = DX12::CreateUploadBuffer(L"Render settings constant buffer", sizeof(RenderSettings));
			frame_ctx->render_settings_cb->Map(0, nullptr, (void**)&frame_ctx->render_settings_ptr);

			// Create the scene constant buffer
			frame_ctx->scene_cb = DX12::CreateUploadBuffer(L"Scene constant buffer", sizeof(SceneData));
			frame_ctx->scene_cb->Map(0, nullptr, (void**)&frame_ctx->scene_cb_ptr);
		}
		DX_CHECK_HR_ERR(d3d_state.device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&d3d_state.frame_fence)), "Failed to create fence");

		// Create the DXC compiler state
		DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&d3d_state.dxc_compiler));
		DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&d3d_state.dxc_utils));
		d3d_state.dxc_utils->CreateDefaultIncludeHandler(&d3d_state.dxc_include_handler);

		// Create the upload buffer
		d3d_state.upload_buffer = DX12::CreateUploadBuffer(L"Generic upload buffer", DX_MB(256));
		d3d_state.upload_buffer->Map(0, nullptr, (void**)&d3d_state.upload_buffer_ptr);
	}

	static void CreatePipelines()
	{
		// Default graphics pipeline
		{
			D3D12_ROOT_PARAMETER1 root_params[2] = {};
			// Render settings constant buffer
			root_params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
			root_params[0].Descriptor.ShaderRegister = 0;
			root_params[0].Descriptor.RegisterSpace = 0;
			root_params[0].Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_NONE;
			root_params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

			// Scene constant buffer
			root_params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
			root_params[1].Descriptor.ShaderRegister = 0;
			root_params[1].Descriptor.RegisterSpace = 1;
			root_params[1].Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_NONE;
			root_params[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

			D3D12_STATIC_SAMPLER_DESC static_samplers[1] = {};
			static_samplers[0].Filter = D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR;
			static_samplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
			static_samplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
			static_samplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
			static_samplers[0].MipLODBias = 0;
			static_samplers[0].MaxAnisotropy = 0;
			static_samplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
			static_samplers[0].BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
			static_samplers[0].MinLOD = 0.0f;
			static_samplers[0].MaxLOD = D3D12_FLOAT32_MAX;
			static_samplers[0].ShaderRegister = 0;
			static_samplers[0].RegisterSpace = 0;
			static_samplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

			D3D12_VERSIONED_ROOT_SIGNATURE_DESC root_sig_desc = {};
			root_sig_desc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
			root_sig_desc.Desc_1_1.NumParameters = DX_ARRAY_SIZE(root_params);
			root_sig_desc.Desc_1_1.pParameters = root_params;
			root_sig_desc.Desc_1_1.NumStaticSamplers = DX_ARRAY_SIZE(static_samplers);
			root_sig_desc.Desc_1_1.pStaticSamplers = static_samplers;
			root_sig_desc.Desc_1_1.Flags =
				D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED |
				D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

			d3d_state.default_raster_pipeline.d3d_root_sig = DX12::CreateRootSignature(root_sig_desc);
			d3d_state.default_raster_pipeline.d3d_pso = DX12::CreateGraphicsPipelineState(
				d3d_state.default_raster_pipeline.d3d_root_sig,
				d3d_state.hdr_render_target->GetDesc().Format,
				d3d_state.depth_buffer->GetDesc().Format,
				L"Include/Shaders/Default_VS_PS.hlsl",
				L"Include/Shaders/Default_VS_PS.hlsl"
			);
		}

		// Post process compute pipeline
		{
			D3D12_ROOT_PARAMETER1 root_params[2] = {};
			root_params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
			root_params[0].Descriptor.ShaderRegister = 0;
			root_params[0].Descriptor.RegisterSpace = 0;
			root_params[0].Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_NONE;
			root_params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

			root_params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
			root_params[1].Constants.Num32BitValues = 2;
			root_params[1].Constants.ShaderRegister = 0;
			root_params[1].Constants.RegisterSpace = 1;
			root_params[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

			D3D12_VERSIONED_ROOT_SIGNATURE_DESC root_sig_desc = {};
			root_sig_desc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
			root_sig_desc.Desc_1_1.NumParameters = DX_ARRAY_SIZE(root_params);
			root_sig_desc.Desc_1_1.pParameters = root_params;
			root_sig_desc.Desc_1_1.NumStaticSamplers = 0;
			root_sig_desc.Desc_1_1.pStaticSamplers = nullptr;
			root_sig_desc.Desc_1_1.Flags = D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED;

			d3d_state.post_process_pipeline.d3d_root_sig = DX12::CreateRootSignature(root_sig_desc);
			d3d_state.post_process_pipeline.d3d_pso = DX12::CreateComputePipelineState(
				d3d_state.post_process_pipeline.d3d_root_sig, L"Include/Shaders/PostProcess_CS.hlsl"
			);
		}
	}

	static void InitDearImGui()
	{
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImPlot::CreateContext();
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
		ImGui_ImplDX12_Init(d3d_state.device, DX_BACK_BUFFER_COUNT, swap_chain_desc.BufferDesc.Format, d3d_state.descriptor_heap_cbv_srv_uav->GetD3D12DescriptorHeap(),
			d3d_state.reserved_cbv_srv_uavs.GetCPUHandle(ReservedDescriptorSRV_DearImGui), d3d_state.reserved_cbv_srv_uavs.GetGPUHandle(ReservedDescriptorSRV_DearImGui));
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

	void Init(const RendererInitParams& params)
	{
		// Initialize slotmaps
		data.memory_scope = MemoryScope(&data.alloc, data.alloc.at_ptr);
		data.texture_slotmap = data.memory_scope.New<ResourceSlotmap<TextureResource>>(&data.memory_scope);
		data.mesh_slotmap = data.memory_scope.New<ResourceSlotmap<MeshResource>>(&data.memory_scope);
		data.render_mesh_data = data.memory_scope.Allocate<RenderMeshData>(1000);

		ResourceTracker::Init(&data.memory_scope);
		InitD3DState(params);
		CreatePipelines();
		InitDearImGui();

		// ------------------------------------------------------------------------------------
		// Default textures

		{
			// Default white texture
			uint32_t white_texture_data = 0xFFFFFFFF;
			UploadTextureParams upload_texture_params = {};
			upload_texture_params.width = 1;
			upload_texture_params.height = 1;
			upload_texture_params.format = TextureFormat_RGBA8_Unorm;
			upload_texture_params.bytes = (uint8_t*)&white_texture_data;
			upload_texture_params.name = "Default white texture";
			data.default_white_texture_handle = Renderer::UploadTexture(upload_texture_params);
			data.default_white_texture = data.texture_slotmap->Find(data.default_white_texture_handle);
		}

		{
			// Default normal texture
			uint8_t r = 127;
			uint8_t g = 127;
			uint8_t b = 255;
			uint8_t a = 255;
			uint32_t normal_texture_data = (a << 24) | (b << 16) | (g << 8) | (r << 0);

			UploadTextureParams upload_texture_params = {};
			upload_texture_params.width = 1;
			upload_texture_params.height = 1;
			upload_texture_params.format = TextureFormat_RGBA8_Unorm;
			upload_texture_params.bytes = (uint8_t*)&normal_texture_data;
			upload_texture_params.name = "Default normal texture";
			data.default_normal_texture_handle = Renderer::UploadTexture(upload_texture_params);
			data.default_normal_texture = data.texture_slotmap->Find(data.default_normal_texture_handle);
		}

		d3d_state.initialized = true;
	}

	void Exit()
	{
		Flush();

		// Release reserved descriptors
		d3d_state.descriptor_heap_rtv->Release(d3d_state.reserved_rtvs);
		d3d_state.descriptor_heap_dsv->Release(d3d_state.reserved_dsvs);
		d3d_state.descriptor_heap_cbv_srv_uav->Release(d3d_state.reserved_cbv_srv_uavs);

		d3d_state.upload_buffer->Unmap(0, nullptr);
		// NOTE: Back buffers have the same ref count, so we only need to release one of them fully to release the other two
		DX_RELEASE_OBJECT(GetFrameContextCurrent()->back_buffer);

		for (uint32_t back_buffer_idx = 0; back_buffer_idx < DX_BACK_BUFFER_COUNT; ++back_buffer_idx)
		{
			D3DState::FrameContext* frame_ctx = GetFrameContext(back_buffer_idx);
			DX_RELEASE_OBJECT(frame_ctx->command_allocator);
			DX_RELEASE_OBJECT(frame_ctx->command_list);
			frame_ctx->instance_buffer->Unmap(0, nullptr);
			frame_ctx->render_settings_cb->Unmap(0, nullptr);
			frame_ctx->scene_cb->Unmap(0, nullptr);
		}

		ImGui_ImplDX12_Shutdown();
		ImGui_ImplWin32_Shutdown();
		ImPlot::DestroyContext();
		ImGui::DestroyContext();

		// Releases all tracked ID3D12 resources (does not call ID3D12Resource::Unmap)
		ResourceTracker::Exit();

		DX_RELEASE_OBJECT(d3d_state.default_raster_pipeline.d3d_root_sig);
		DX_RELEASE_OBJECT(d3d_state.default_raster_pipeline.d3d_pso);
		DX_RELEASE_OBJECT(d3d_state.post_process_pipeline.d3d_root_sig);
		DX_RELEASE_OBJECT(d3d_state.post_process_pipeline.d3d_pso);

		DX_RELEASE_OBJECT(d3d_state.dxc_include_handler);
		DX_RELEASE_OBJECT(d3d_state.dxc_utils);
		DX_RELEASE_OBJECT(d3d_state.dxc_compiler);
		DX_RELEASE_OBJECT(d3d_state.frame_fence);
		DX_RELEASE_OBJECT(d3d_state.swapchain);
		DX_RELEASE_OBJECT(d3d_state.swapchain_command_queue);

		// NOTE: I am not very fond of this.. Memory scopes are great for RAII, but my systems do not have a constructor/destructor
		// which means we have to call the memory scope destructor manually here. I will figure this out later, I need to make up my mind about
		// RAII first.
		data.memory_scope.~MemoryScope();

		/*ID3D12DebugDevice2* debug_device;
		d3d_state.device->QueryInterface(IID_PPV_ARGS(&debug_device));
		debug_device->ReportLiveDeviceObjects(D3D12_RLDO_DETAIL);
		DX_RELEASE_INTERFACE(debug_device);*/

		DX_RELEASE_OBJECT(d3d_state.device);
		DX_RELEASE_OBJECT(d3d_state.adapter);

		/*IDXGIDebug1* dxgi_debug;
		DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgi_debug));
		DX_CHECK_HR(dxgi_debug->ReportLiveObjects(DXGI_DEBUG_DX, DXGI_DEBUG_RLO_ALL));
		DX_RELEASE_INTERFACE(dxgi_debug);*/

		d3d_state.initialized = false;
	}

	void Flush()
	{
		DX12::WaitOnFence(d3d_state.swapchain_command_queue, d3d_state.frame_fence, d3d_state.frame_fence_value);
	}

	void BeginFrame(const Vec3& view_pos, const Mat4x4& view, const Mat4x4& projection)
	{
		DX_PERF_SCOPE("Renderer::BeginFrame");

		// ----------------------------------------------------------------------------------
		// Wait on the current back buffer until all commands on it have finished execution

		// We wait here right before actually using the back buffer again to minimize the amount of time
		// to maximize the amount of work the CPU can do before actually having to wait on the GPU.
		// Some examples on the internet wait right after presenting, which is not ideal

		D3DState::FrameContext* frame_ctx = GetFrameContextCurrent();
		DX12::WaitOnFence(d3d_state.swapchain_command_queue, d3d_state.frame_fence, frame_ctx->back_buffer_fence_value);

		frame_ctx->render_settings_ptr->pbr.use_linear_perceptual_roughness = data.settings.pbr.use_linear_perceptual_roughness;
		frame_ctx->render_settings_ptr->pbr.diffuse_brdf = data.settings.pbr.diffuse_brdf;
		frame_ctx->render_settings_ptr->post_process.tonemap_operator = data.settings.post_process.tonemap_operator;
		frame_ctx->render_settings_ptr->post_process.gamma = data.settings.post_process.gamma;
		frame_ctx->render_settings_ptr->post_process.exposure = data.settings.post_process.exposure;
		frame_ctx->render_settings_ptr->post_process.max_white = data.settings.post_process.max_white;

		frame_ctx->scene_cb_ptr->view = view;
		frame_ctx->scene_cb_ptr->projection = projection;
		frame_ctx->scene_cb_ptr->view_projection = Mat4x4Mul(frame_ctx->scene_cb_ptr->view, frame_ctx->scene_cb_ptr->projection);
		frame_ctx->scene_cb_ptr->view_pos = view_pos;

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
		DX_PERF_SCOPE("Renderer::RenderFrame");

		// ----------------------------------------------------------------------------------
		// Create the render target view for the current back buffer

		D3DState::FrameContext* frame_ctx = GetFrameContextCurrent();
		ID3D12GraphicsCommandList6* cmd_list = frame_ctx->command_list;

		D3D12_RESOURCE_BARRIER default_barriers[] = {
			ResourceTracker::TransitionBarrier(d3d_state.hdr_render_target, D3D12_RESOURCE_STATE_RENDER_TARGET),
			//ResourceTracker::TransitionBarrier(d3d_state.depth_buffer, D3D12_RESOURCE_STATE_DEPTH_WRITE)
		};
		cmd_list->ResourceBarrier(DX_ARRAY_SIZE(default_barriers), default_barriers);

		D3D12_CPU_DESCRIPTOR_HANDLE hdr_rtv_handle = d3d_state.reserved_rtvs.GetCPUHandle(ReservedDescriptorRTV_HDRRenderTarget);
		D3D12_CPU_DESCRIPTOR_HANDLE dsv_handle = d3d_state.reserved_dsvs.GetCPUHandle(ReservedDescriptorDSV_DepthBuffer);
		float clear_color[4] = { 1.0, 0.0, 1.0, 1.0 };
		cmd_list->ClearRenderTargetView(hdr_rtv_handle, clear_color, 0, nullptr);
		cmd_list->ClearDepthStencilView(dsv_handle, D3D12_CLEAR_FLAG_DEPTH, 1.0, 0, 0, nullptr);

		// ----------------------------------------------------------------------------------
		// Default geometry and shading render pass

		D3D12_VIEWPORT viewport = { 0.0, 0.0, d3d_state.render_width, d3d_state.render_height, 0.0, 1.0 };
		D3D12_RECT scissor_rect = { 0, 0, LONG_MAX, LONG_MAX };

		cmd_list->RSSetViewports(1, &viewport);
		cmd_list->RSSetScissorRects(1, &scissor_rect);
		cmd_list->OMSetRenderTargets(1, &hdr_rtv_handle, FALSE, &dsv_handle);

		// NOTE: Before binding the root signature, we need to bind the descriptor heap
		ID3D12DescriptorHeap* const descriptor_heaps = { d3d_state.descriptor_heap_cbv_srv_uav->GetD3D12DescriptorHeap() };
		cmd_list->SetDescriptorHeaps(1, &descriptor_heaps);

		cmd_list->SetGraphicsRootSignature(d3d_state.default_raster_pipeline.d3d_root_sig);
		cmd_list->SetPipelineState(d3d_state.default_raster_pipeline.d3d_pso);

		cmd_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		cmd_list->SetGraphicsRootConstantBufferView(0, frame_ctx->render_settings_cb->GetGPUVirtualAddress());
		cmd_list->SetGraphicsRootConstantBufferView(1, frame_ctx->scene_cb->GetGPUVirtualAddress());
		cmd_list->IASetVertexBuffers(1, 1, &frame_ctx->instance_vbv);

		for (size_t mesh = 0; mesh < data.stats.mesh_count; ++mesh)
		{
			RenderMeshData* mesh_data = &data.render_mesh_data[mesh];
			MeshResource* mesh_resource = data.mesh_slotmap->Find(mesh_data->mesh_handle);
			
			cmd_list->IASetVertexBuffers(0, 1, &mesh_resource->vbv);
			cmd_list->IASetIndexBuffer(&mesh_resource->ibv);
			cmd_list->DrawIndexedInstanced(mesh_resource->ibv.SizeInBytes / 4, 1, 0, 0, mesh);

			data.stats.draw_call_count++;
			data.stats.total_vertex_count += mesh_resource->ibv.SizeInBytes / 4;
			data.stats.total_triangle_count += mesh_resource->ibv.SizeInBytes / 4 / 3;
		}

		// ----------------------------------------------------------------------------------
		// Post-processing pass

		D3D12_RESOURCE_BARRIER post_process_barriers[] = {
			ResourceTracker::TransitionBarrier(d3d_state.hdr_render_target, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE),
			ResourceTracker::TransitionBarrier(d3d_state.sdr_render_target, D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
		};
		cmd_list->ResourceBarrier(DX_ARRAY_SIZE(post_process_barriers), post_process_barriers);

		cmd_list->SetComputeRootSignature(d3d_state.post_process_pipeline.d3d_root_sig);
		cmd_list->SetPipelineState(d3d_state.post_process_pipeline.d3d_pso);

		cmd_list->SetComputeRootConstantBufferView(0, frame_ctx->render_settings_cb->GetGPUVirtualAddress());
		uint32_t hdr_srv_index = d3d_state.reserved_cbv_srv_uavs.GetDescriptorHeapIndex(ReservedDescriptorSRV_HDRRenderTarget);
		uint32_t sdr_uav_index = d3d_state.reserved_cbv_srv_uavs.GetDescriptorHeapIndex(ReservedDescriptorUAV_SDRRenderTarget);
		cmd_list->SetComputeRoot32BitConstant(1, hdr_srv_index, 0);
		cmd_list->SetComputeRoot32BitConstant(1, sdr_uav_index, 1);

		uint32_t num_dispatch_threads_x = DX_ALIGN_POW2(d3d_state.render_width, 8) / 8;
		uint32_t num_dispatch_threads_y = DX_ALIGN_POW2(d3d_state.render_height, 8) / 8;
		cmd_list->Dispatch(num_dispatch_threads_x, num_dispatch_threads_y, 1);
	}

	void EndFrame()
	{
		DX_PERF_SCOPE("Renderer::EndFrame");

		// ----------------------------------------------------------------------------------
		// Transition the back buffer to the present state

		D3DState::FrameContext* frame_ctx = GetFrameContextCurrent();
		ID3D12GraphicsCommandList6* cmd_list = frame_ctx->command_list;

		D3D12_RESOURCE_BARRIER resolve_backbuffer_barriers[] = {
			ResourceTracker::TransitionBarrier(d3d_state.sdr_render_target, D3D12_RESOURCE_STATE_COPY_SOURCE),
			DX12::TransitionBarrier(frame_ctx->back_buffer, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_COPY_DEST)
		};
		cmd_list->ResourceBarrier(DX_ARRAY_SIZE(resolve_backbuffer_barriers), resolve_backbuffer_barriers);
		
		cmd_list->CopyResource(frame_ctx->back_buffer, d3d_state.sdr_render_target);

		D3D12_RESOURCE_BARRIER present_barrier = DX12::TransitionBarrier(frame_ctx->back_buffer,
			D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PRESENT);
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

		frame_ctx->back_buffer_fence_value = ++d3d_state.frame_fence_value;
		DX12::SignalCommandQueue(d3d_state.swapchain_command_queue, d3d_state.frame_fence, frame_ctx->back_buffer_fence_value);
		d3d_state.current_back_buffer_idx = d3d_state.swapchain->GetCurrentBackBufferIndex();

		// ----------------------------------------------------------------------------------
		// Increase the total frame index and reset the render statistics

		d3d_state.frame_index++;
		data.stats = { 0 };
	}

	void RenderMesh(ResourceHandle mesh_handle, const Material& material, const Mat4x4& transform)
	{
		DX_ASSERT(data.stats.mesh_count < MAX_RENDER_MESHES);
		data.render_mesh_data[data.stats.mesh_count] =
		{
			// TODO: Default mesh handle? (e.g. Cube)
			.mesh_handle = mesh_handle,
			.material = material
		};

		D3DState::FrameContext* frame_ctx = GetFrameContextCurrent();

		TextureResource* base_color_texture = data.texture_slotmap->Find(data.render_mesh_data[data.stats.mesh_count].material.base_color_texture_handle);
		if (!base_color_texture)
		{
			base_color_texture = data.default_white_texture;
		}
		TextureResource* normal_texture = data.texture_slotmap->Find(data.render_mesh_data[data.stats.mesh_count].material.normal_texture_handle);
		if (!normal_texture)
		{
			normal_texture = data.default_normal_texture;
		}
		TextureResource* metallic_roughness_texture = data.texture_slotmap->Find(data.render_mesh_data[data.stats.mesh_count].material.metallic_roughness_texture_handle);
		if (!metallic_roughness_texture)
		{
			metallic_roughness_texture = data.default_white_texture;
		}

		frame_ctx->instance_buffer_ptr[data.stats.mesh_count].transform = transform;
		frame_ctx->instance_buffer_ptr[data.stats.mesh_count].base_color_texture_index = base_color_texture->srv.descriptor_heap_index;
		frame_ctx->instance_buffer_ptr[data.stats.mesh_count].normal_texture_index = normal_texture->srv.descriptor_heap_index;
		frame_ctx->instance_buffer_ptr[data.stats.mesh_count].metallic_roughness_texture_index = metallic_roughness_texture->srv.descriptor_heap_index;
		frame_ctx->instance_buffer_ptr[data.stats.mesh_count].metallic_factor = material.metallic_factor;
		frame_ctx->instance_buffer_ptr[data.stats.mesh_count].roughness_factor = material.roughness_factor;

		data.stats.mesh_count++;
	}

	ResourceHandle UploadTexture(const UploadTextureParams& params)
	{
		ID3D12Resource* resource = DX12::CreateTexture(DX12::UTF16FromUTF8(&g_thread_alloc, params.name),
			TextureFormatToDXGIFormat(params.format), params.width, params.height);

		D3DState::FrameContext* frame_ctx = GetFrameContextCurrent();
		ID3D12GraphicsCommandList6* cmd_list = frame_ctx->command_list;
		D3D12_RESOURCE_BARRIER copy_dst_barrier = ResourceTracker::TransitionBarrier(resource, D3D12_RESOURCE_STATE_COPY_DEST);
		cmd_list->ResourceBarrier(1, &copy_dst_barrier);

		uint32_t bpp = TextureFormatBPP(params.format);

		D3D12_RESOURCE_DESC dst_desc = resource->GetDesc();
		D3D12_SUBRESOURCE_FOOTPRINT dst_footprint = {};
		dst_footprint.Format = resource->GetDesc().Format;
		dst_footprint.Width = params.width;
		dst_footprint.Height = params.height;
		dst_footprint.Depth = 1;
		dst_footprint.RowPitch = DX_ALIGN_POW2(params.width * bpp, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);

		D3D12_PLACED_SUBRESOURCE_FOOTPRINT src_footprint = {};
		src_footprint.Footprint = dst_footprint;
		src_footprint.Offset = 0;

		uint8_t* src_ptr = params.bytes;
		uint8_t* dst_ptr = d3d_state.upload_buffer_ptr;
		uint32_t dst_pitch = dst_footprint.RowPitch;

		for (uint32_t y = 0; y < params.height; ++y)
		{
			memcpy(dst_ptr, src_ptr, dst_pitch);
			src_ptr += params.width * bpp;
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

		D3D12_RESOURCE_BARRIER pixel_src_barrier = ResourceTracker::TransitionBarrier(resource,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		cmd_list->ResourceBarrier(1, &pixel_src_barrier);
		
		// Execute the command list, wait on it, and reset it
		DX12::ExecuteCommandList(d3d_state.swapchain_command_queue, cmd_list);
		cmd_list->Reset(frame_ctx->command_allocator, nullptr);

		uint64_t fence_value = ++d3d_state.frame_fence_value;
		DX12::SignalCommandQueue(d3d_state.swapchain_command_queue, d3d_state.frame_fence, fence_value);
		DX12::WaitOnFence(d3d_state.swapchain_command_queue, d3d_state.frame_fence, fence_value);

		uint32_t cbv_srv_uav_increment_size = d3d_state.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		
		TextureResource texture_resource = {};
		texture_resource.resource = resource;
		texture_resource.srv = d3d_state.descriptor_heap_cbv_srv_uav->Allocate();
		
		D3D12_RESOURCE_DESC resource_desc = resource->GetDesc();
		DX12::CreateTextureSRV(resource, texture_resource.srv.cpu, resource_desc.Format);

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

		D3D12_RESOURCE_BARRIER copy_dst_barriers[] = {
			ResourceTracker::TransitionBarrier(vertex_buffer, D3D12_RESOURCE_STATE_COPY_DEST),
			ResourceTracker::TransitionBarrier(index_buffer, D3D12_RESOURCE_STATE_COPY_DEST)
		};
		cmd_list->ResourceBarrier(DX_ARRAY_SIZE(copy_dst_barriers), copy_dst_barriers);
		cmd_list->CopyBufferRegion(vertex_buffer, 0, d3d_state.upload_buffer, 0, vb_total_bytes);
		cmd_list->CopyBufferRegion(index_buffer, 0, d3d_state.upload_buffer, vb_total_bytes, ib_total_bytes);

		D3D12_RESOURCE_BARRIER barriers[] =
		{
			ResourceTracker::TransitionBarrier(vertex_buffer, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER),
			ResourceTracker::TransitionBarrier(index_buffer, D3D12_RESOURCE_STATE_INDEX_BUFFER)
		};
		cmd_list->ResourceBarrier(2, barriers);

		DX12::ExecuteCommandList(d3d_state.swapchain_command_queue, cmd_list);
		cmd_list->Reset(frame_ctx->command_allocator, nullptr);

		uint64_t fence_value = ++d3d_state.frame_fence_value;
		DX12::SignalCommandQueue(d3d_state.swapchain_command_queue, d3d_state.frame_fence, fence_value);
		DX12::WaitOnFence(d3d_state.swapchain_command_queue, d3d_state.frame_fence, fence_value);

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
			d3d_state.render_width = new_width;
			d3d_state.render_height = new_height;

			// Release all resolution dependent resources
			// TODO: Releasing these causes a crash, because they all end up in the same slot in the hashmap somehow
			ResourceTracker::ReleaseResource(d3d_state.hdr_render_target);
			ResourceTracker::ReleaseResource(d3d_state.sdr_render_target);
			ResourceTracker::ReleaseResource(d3d_state.depth_buffer);

			// Recreate the render targets with the new resolution, and resize the swapchain back buffers
			CreateRenderTargets();
			ResizeBackBuffers();
		}
	}

	void OnImGuiRender()
	{
		DXGI_QUERY_VIDEO_MEMORY_INFO local_mem_info = {};
		d3d_state.adapter->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &local_mem_info);

		DXGI_QUERY_VIDEO_MEMORY_INFO non_local_mem_info = {};
		d3d_state.adapter->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_NON_LOCAL, &non_local_mem_info);

		// ---------------------------------------------------------------------------
		// General renderer menu with information about the hardware, memory usage, render statistics, etc.

		ImGui::Begin("Renderer");

		ImGui::SetNextItemOpen(true, ImGuiCond_Once);
		if (ImGui::CollapsingHeader("General"))
		{
			ImGui::Text("%ls", d3d_state.adapter_desc.Description);
			ImGui::Text("Dedicated video memory: %u MB", DX_TO_MB(d3d_state.adapter_desc.DedicatedVideoMemory));
			ImGui::Text("Dedicated system memory: %u MB", DX_TO_MB(d3d_state.adapter_desc.DedicatedSystemMemory));
			ImGui::Text("Shared system memory: %u MB", DX_TO_MB(d3d_state.adapter_desc.SharedSystemMemory));
			ImGui::Text("Back buffer count: %u", DX_BACK_BUFFER_COUNT);
			ImGui::Text("Current back buffer: %u", d3d_state.current_back_buffer_idx);
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

		// ---------------------------------------------------------------------------
		// Render settings menu for tweaking all sorts of rendering settings

		ImGui::Begin("Render settings");

		ImGui::Text("Resolution: %ux%u", d3d_state.render_width, d3d_state.render_height);
		ImGui::Checkbox("VSync", &d3d_state.vsync_enabled);
		ImGui::Text("Tearing: %s", d3d_state.tearing_supported ? "true" : "false");

		if (ImGui::CollapsingHeader("Physically-based rendering"))
		{
			ImGui::Indent(20.0);

			ImGui::Checkbox("Use linear perceptual roughness", (bool*)&data.settings.pbr.use_linear_perceptual_roughness);
			if (ImGui::BeginCombo("Diffuse BRDF", DiffuseBRDFName(data.settings.pbr.diffuse_brdf), ImGuiComboFlags_None))
			{
				for (uint32_t diffuse_brdf = 0; diffuse_brdf < PBR_DIFFUSE_BRDF_NUM_TYPES; ++diffuse_brdf)
				{
					bool is_selected = diffuse_brdf == data.settings.pbr.diffuse_brdf;
					if (ImGui::Selectable(DiffuseBRDFName(diffuse_brdf), is_selected))
					{
						data.settings.pbr.diffuse_brdf = diffuse_brdf;
					}

					if (is_selected)
					{
						ImGui::SetItemDefaultFocus();
					}
				}
				ImGui::EndCombo();
			}

			ImGui::Unindent(20.0);
		}

		if (ImGui::CollapsingHeader("Post-processing"))
		{
			ImGui::Indent(20.0);

			if (ImGui::BeginCombo("Tonemap operator", TonemapOperatorName(data.settings.post_process.tonemap_operator), ImGuiComboFlags_None))
			{
				for (uint32_t tonemap_operator = 0; tonemap_operator < TONEMAP_OP_NUM_TYPES; ++tonemap_operator)
				{
					bool is_selected = tonemap_operator == data.settings.post_process.tonemap_operator;
					if (ImGui::Selectable(TonemapOperatorName(tonemap_operator), is_selected))
					{
						data.settings.post_process.tonemap_operator = tonemap_operator;
					}

					if (is_selected)
					{
						ImGui::SetItemDefaultFocus();
					}
				}

				ImGui::EndCombo();
			}

			ImGui::SliderFloat("Gamma", &data.settings.post_process.gamma, 0.01f, 10.0f, "%.3f", ImGuiSliderFlags_None);
			ImGui::SliderFloat("Exposure", &data.settings.post_process.exposure, 0.01f, 10.0f, "%.3f", ImGuiSliderFlags_None);
			ImGui::SliderFloat("Max white", &data.settings.post_process.max_white, 0.01f, 100.0f, "%.3f", ImGuiSliderFlags_None);

			ImGui::Unindent(20.0);
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
		
		D3D12_RESOURCE_BARRIER imgui_rt_barrier = ResourceTracker::TransitionBarrier(d3d_state.sdr_render_target, D3D12_RESOURCE_STATE_RENDER_TARGET);
		cmd_list->ResourceBarrier(1, &imgui_rt_barrier);

		D3D12_CPU_DESCRIPTOR_HANDLE imgui_rtv_handle = d3d_state.reserved_rtvs.GetCPUHandle(ReservedDescriptorRTV_SDRRenderTarget);
		ID3D12DescriptorHeap* descriptor_heap = d3d_state.descriptor_heap_cbv_srv_uav->GetD3D12DescriptorHeap();
		cmd_list->OMSetRenderTargets(1, &imgui_rtv_handle, FALSE, nullptr);
		cmd_list->SetDescriptorHeaps(1, &descriptor_heap);

		ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), cmd_list);
	}

	bool IsInitialized()
	{
		return d3d_state.initialized;
	}

}
