#include "Pch.h"
#include "Renderer.h"

#include "imgui/imgui.h"
#include "imgui/imgui_impl_win32.h"
#include "imgui/imgui_impl_dx12.h"

// TODO: Should add HR error explanation to these macros as well
#define DX_CHECK_HR_ERR(hr, error) \
do { \
	if (FAILED(hr)) \
	{ \
		DX_ASSERT(false && (error)); \
	} \
} \
while (false)

#define DX_CHECK_HR(hr) DX_CHECK_HR_ERR(hr, "")

#define DX_CHECK_HR_BRANCH(hr) \
if (FAILED(hr)) \
{ \
	DX_ASSERT(false); \
} \
else

#define DX_RELEASE_OBJECT(object) if ((object)) { (object)->Release(); } (object) = nullptr

namespace Renderer
{

	struct DX_Vec3
	{
		float x, y, z;
	};

	struct Vertex
	{
		DX_Vec3 pos;
	};

	struct RasterPipeline
	{
		ID3D12RootSignature* root_sig;
		ID3D12PipelineState* pipeline_state;
	};

	enum ReservedDescriptor : uint32_t
	{
		ReservedDescriptor_DearImGui,
		ReservedDescriptor_Count
	};

	struct D3DState
	{
		// Adapter and device
		IDXGIAdapter4* adapter;
		ID3D12Device8* device;

		// Swap chain
		IDXGISwapChain4* swapchain;
		ID3D12CommandQueue* swapchain_command_queue;
		bool tearing_supported;
		bool vsync_enabled;
		uint32_t current_back_buffer_idx;
		uint64_t back_buffer_fence_values[3];

		// Render resolution
		// TODO: Should separate some of these into an API agnostic renderer state, since these do not depend on D3D, same goes for vsync
		uint32_t render_width;
		uint32_t render_height;

		// Command queue, list and allocator
		ID3D12CommandQueue* command_queue_direct;
		ID3D12GraphicsCommandList6* command_list[3];
		ID3D12CommandAllocator* command_allocator[3];
		uint64_t fence_value;
		ID3D12Fence* fence;

		// Descriptor heaps
		ID3D12DescriptorHeap* descriptor_heap_rtv;
		ID3D12DescriptorHeap* descriptor_heap_cbv_srv_uav;

		// DXC shader compiler
		IDxcCompiler* dxc_compiler;
		IDxcUtils* dxc_utils;
		IDxcIncludeHandler* dxc_include_handler;

		// Pipeline states
		RasterPipeline default_raster_pipeline;
		ID3D12Resource* vertex_buffer;
		Vertex* vertex_buffer_ptr;
	} static d3d_state;

	ID3D12CommandQueue* CreateCommandQueue(D3D12_COMMAND_LIST_TYPE type, D3D12_COMMAND_QUEUE_PRIORITY priority)
	{
		D3D12_COMMAND_QUEUE_DESC command_queue_desc = {};
		command_queue_desc.Type = type;
		command_queue_desc.Priority = priority;
		command_queue_desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
		command_queue_desc.NodeMask = 0;

		ID3D12CommandQueue* command_queue;
		DX_CHECK_HR(d3d_state.device->CreateCommandQueue(&command_queue_desc, IID_PPV_ARGS(&command_queue)));

		return command_queue;
	}

	ID3D12DescriptorHeap* CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t num_descriptors, D3D12_DESCRIPTOR_HEAP_FLAGS flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE)
	{
		D3D12_DESCRIPTOR_HEAP_DESC descriptor_heap_desc = {};
		descriptor_heap_desc.Type = type;
		descriptor_heap_desc.NumDescriptors = num_descriptors;
		descriptor_heap_desc.Flags = flags;
		descriptor_heap_desc.NodeMask = 0;

		ID3D12DescriptorHeap* descriptor_heap;
		d3d_state.device->CreateDescriptorHeap(&descriptor_heap_desc, IID_PPV_ARGS(&descriptor_heap));

		return descriptor_heap;
	}

	ID3D12RootSignature* CreateRootSignature()
	{
		D3D12_VERSIONED_ROOT_SIGNATURE_DESC versioned_root_sig_desc = {};
		versioned_root_sig_desc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
		versioned_root_sig_desc.Desc_1_1.NumParameters = 0;
		versioned_root_sig_desc.Desc_1_1.pParameters = nullptr;
		versioned_root_sig_desc.Desc_1_1.NumStaticSamplers = 0;
		versioned_root_sig_desc.Desc_1_1.pStaticSamplers = nullptr;
		// TODO: We want to use SM 6.6 ResourceHeap, so add this flag (there is also one for samplers if needed):
		// D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED;
		versioned_root_sig_desc.Desc_1_1.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

		ID3DBlob* serialized_root_sig, *error;
		DX_CHECK_HR_ERR(D3D12SerializeVersionedRootSignature(&versioned_root_sig_desc, &serialized_root_sig, &error), "Failed to serialize versioned root signature");
		if (error)
		{
			DX_ASSERT(false && (char*)error->GetBufferPointer());
		}
		DX_RELEASE_OBJECT(error);

		ID3D12RootSignature* root_sig;
		DX_CHECK_HR_ERR(d3d_state.device->CreateRootSignature(0, serialized_root_sig->GetBufferPointer(),
			serialized_root_sig->GetBufferSize(), IID_PPV_ARGS(&root_sig)), "Failed to create root signature");

		DX_RELEASE_OBJECT(serialized_root_sig);

		return root_sig;
	}

	IDxcBlob* CompileShader(const wchar_t* filepath, const wchar_t* entry_point, const wchar_t* target_profile)
	{
		// TODO: Use shader reflection to figure out the bindings/root signature?
		HRESULT hr;
		uint32_t codepage = 0;
		IDxcBlobEncoding* shader_text = nullptr;

		DX_CHECK_HR_ERR(d3d_state.dxc_utils->LoadFile(filepath, &codepage, &shader_text), "Failed to load shader from file");

		const wchar_t* compile_args[] =
		{
			DXC_ARG_WARNINGS_ARE_ERRORS,
			DXC_ARG_OPTIMIZATION_LEVEL3,
			DXC_ARG_PACK_MATRIX_ROW_MAJOR,
#ifdef _DEBUG
			DXC_ARG_DEBUG,
			DXC_ARG_SKIP_OPTIMIZATIONS
#endif
		};

		IDxcOperationResult* result;
		DX_CHECK_HR_ERR(d3d_state.dxc_compiler->Compile(
			shader_text, filepath, entry_point, target_profile,
			compile_args, DX_ARRAY_SIZE(compile_args), nullptr,
			0, d3d_state.dxc_include_handler, &result
		), "Failed to compile shader");

		result->GetStatus(&hr);
		if (FAILED(hr))
		{
			IDxcBlobEncoding* error;
			result->GetErrorBuffer(&error);
			DX_ASSERT(false && (char*)error->GetBufferPointer());

			return nullptr;
		}

		IDxcBlob* blob;
		result->GetResult(&blob);
		return blob;

		DX_RELEASE_OBJECT(shader_text);
		DX_RELEASE_OBJECT(result);
	}

	ID3D12PipelineState* CreatePipelineState(ID3D12RootSignature* root_sig, const wchar_t* vs_path, const wchar_t* ps_path)
	{
		D3D12_RENDER_TARGET_BLEND_DESC rt_blend_desc = {};
		rt_blend_desc.BlendEnable = TRUE;
		rt_blend_desc.LogicOpEnable = FALSE;
		rt_blend_desc.SrcBlend = D3D12_BLEND_SRC_ALPHA;
		rt_blend_desc.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
		rt_blend_desc.BlendOp = D3D12_BLEND_OP_ADD;
		rt_blend_desc.SrcBlendAlpha = D3D12_BLEND_SRC_ALPHA;
		rt_blend_desc.DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
		rt_blend_desc.BlendOpAlpha = D3D12_BLEND_OP_ADD;
		rt_blend_desc.LogicOp = D3D12_LOGIC_OP_NOOP;
		rt_blend_desc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

		D3D12_INPUT_ELEMENT_DESC input_element_desc[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		};

		IDxcBlob* vs_blob = CompileShader(vs_path, L"VSMain", L"vs_6_5");
		IDxcBlob* ps_blob = CompileShader(ps_path, L"PSMain", L"ps_6_5");

		D3D12_GRAPHICS_PIPELINE_STATE_DESC graphics_pipeline_state_desc = {};
		graphics_pipeline_state_desc.InputLayout.NumElements = DX_ARRAY_SIZE(input_element_desc);
		graphics_pipeline_state_desc.InputLayout.pInputElementDescs = input_element_desc;
		graphics_pipeline_state_desc.VS.BytecodeLength = vs_blob->GetBufferSize();
		graphics_pipeline_state_desc.VS.pShaderBytecode = vs_blob->GetBufferPointer();
		graphics_pipeline_state_desc.PS.BytecodeLength = ps_blob->GetBufferSize();
		graphics_pipeline_state_desc.PS.pShaderBytecode = ps_blob->GetBufferPointer();
		graphics_pipeline_state_desc.NumRenderTargets = 1;
		graphics_pipeline_state_desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
		graphics_pipeline_state_desc.DepthStencilState.DepthEnable = FALSE;
		graphics_pipeline_state_desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_NEVER;
		graphics_pipeline_state_desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
		graphics_pipeline_state_desc.DepthStencilState.StencilEnable = FALSE;
		graphics_pipeline_state_desc.DSVFormat = DXGI_FORMAT_UNKNOWN;
		graphics_pipeline_state_desc.BlendState.AlphaToCoverageEnable = FALSE;
		graphics_pipeline_state_desc.BlendState.IndependentBlendEnable = FALSE;
		graphics_pipeline_state_desc.BlendState.RenderTarget[0] = rt_blend_desc;
		graphics_pipeline_state_desc.SampleDesc.Count = 1;
		graphics_pipeline_state_desc.SampleDesc.Quality = 0;
		graphics_pipeline_state_desc.SampleMask = UINT32_MAX;
		graphics_pipeline_state_desc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
		graphics_pipeline_state_desc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
		graphics_pipeline_state_desc.NodeMask = 0;
		graphics_pipeline_state_desc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
		graphics_pipeline_state_desc.pRootSignature = root_sig;
		graphics_pipeline_state_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

		// TODO: Should be able to specify via parameters if this should be a graphics or compute pipeline state
		ID3D12PipelineState* pipeline_state;
		DX_CHECK_HR_ERR(d3d_state.device->CreateGraphicsPipelineState(&graphics_pipeline_state_desc, IID_PPV_ARGS(&pipeline_state)), "Failed to create pipeline state");

		DX_RELEASE_OBJECT(vs_blob);
		DX_RELEASE_OBJECT(ps_blob);

		return pipeline_state;
	}

	void InitD3DState(const RendererInitParams& params)
	{
#ifdef _DEBUG
		// Enable debug layer
		// TODO: Add GPU validation toggle
		ID3D12Debug* debug_controller;
		DX_CHECK_HR_BRANCH(D3D12GetDebugInterface(IID_PPV_ARGS(&debug_controller)))
		{
			debug_controller->EnableDebugLayer();
		}
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
		D3D_FEATURE_LEVEL d3d_min_feature_level = D3D_FEATURE_LEVEL_12_1;
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

		// Set info queue severity behavior
		ID3D12InfoQueue* info_queue;
		DX_CHECK_HR_BRANCH(d3d_state.device->QueryInterface(IID_PPV_ARGS(&info_queue)))
		{
			DX_CHECK_HR(info_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE));
			DX_CHECK_HR(info_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE));
			DX_CHECK_HR(info_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, TRUE));
		}

		// Create the command queue for the swap chain
		d3d_state.swapchain_command_queue = CreateCommandQueue(D3D12_COMMAND_LIST_TYPE_DIRECT, D3D12_COMMAND_QUEUE_PRIORITY_NORMAL);

		// Create the swap chain
		BOOL allow_tearing = FALSE;
		DX_CHECK_HR_BRANCH(dxgi_factory->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allow_tearing, sizeof(BOOL)))
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
		swap_chain_desc.BufferCount = 3;
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
		for (uint32_t back_buffer_idx = 0; back_buffer_idx < 3; ++back_buffer_idx)
		{
			DX_CHECK_HR_ERR(d3d_state.device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&d3d_state.command_allocator[back_buffer_idx])), "Failed to create command allocator");
			DX_CHECK_HR_ERR(d3d_state.device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, d3d_state.command_allocator[back_buffer_idx], nullptr, IID_PPV_ARGS(&d3d_state.command_list[back_buffer_idx])), "Failed to create command list");
		}
		DX_CHECK_HR_ERR(d3d_state.device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&d3d_state.fence)), "Failed to create fence");

		// Create descriptor heaps
		d3d_state.descriptor_heap_rtv = CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 1);
		d3d_state.descriptor_heap_cbv_srv_uav = CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE);

		// Create the DXC compiler state
		DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&d3d_state.dxc_compiler));
		DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&d3d_state.dxc_utils));
		d3d_state.dxc_utils->CreateDefaultIncludeHandler(&d3d_state.dxc_include_handler);
	}

	void InitPipelines()
	{
		d3d_state.default_raster_pipeline.root_sig = CreateRootSignature();
		d3d_state.default_raster_pipeline.pipeline_state = CreatePipelineState(d3d_state.default_raster_pipeline.root_sig,
			L"Include/Shaders/Default_VS_PS.hlsl", L"Include/Shaders/Default_VS_PS.hlsl");

		D3D12_HEAP_PROPERTIES heap_props = {};
		heap_props.Type = D3D12_HEAP_TYPE_UPLOAD;

		D3D12_RESOURCE_DESC resource_desc = {};
		resource_desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		resource_desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		resource_desc.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
		resource_desc.Format = DXGI_FORMAT_UNKNOWN;
		resource_desc.Width = sizeof(Vertex) * 6;
		resource_desc.Height = 1;
		resource_desc.DepthOrArraySize = 1;
		resource_desc.MipLevels = 1;
		resource_desc.SampleDesc.Count = 1;
		resource_desc.Flags = D3D12_RESOURCE_FLAG_NONE;

		d3d_state.device->CreateCommittedResource(&heap_props, D3D12_HEAP_FLAG_NONE, &resource_desc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&d3d_state.vertex_buffer));
		d3d_state.vertex_buffer->Map(0, nullptr, (void**)&d3d_state.vertex_buffer_ptr);
	}

	void InitDearImGui()
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
		d3d_state.swapchain->GetHwnd(&hWnd);
		d3d_state.swapchain->GetDesc(&swap_chain_desc);

		uint32_t cbv_srv_uav_increment_size = d3d_state.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		ImGui_ImplWin32_Init(hWnd);
		ImGui_ImplDX12_Init(d3d_state.device, 3, swap_chain_desc.BufferDesc.Format, d3d_state.descriptor_heap_cbv_srv_uav,
			{ d3d_state.descriptor_heap_cbv_srv_uav->GetCPUDescriptorHandleForHeapStart().ptr + ReservedDescriptor_DearImGui * cbv_srv_uav_increment_size },
			{ d3d_state.descriptor_heap_cbv_srv_uav->GetGPUDescriptorHandleForHeapStart().ptr + ReservedDescriptor_DearImGui * cbv_srv_uav_increment_size });
	}

	void ResizeRenderResolution(uint32_t new_width, uint32_t new_height)
	{
		d3d_state.render_width = new_width;
		d3d_state.render_height = new_height;

		Flush();

		DXGI_SWAP_CHAIN_DESC swap_chain_desc;
		d3d_state.swapchain->GetDesc(&swap_chain_desc);
		d3d_state.swapchain->ResizeBuffers(3, d3d_state.render_width, d3d_state.render_height, swap_chain_desc.BufferDesc.Format, swap_chain_desc.Flags);
	}

	void CheckForWindowResize()
	{
		RECT window_rect;
		HWND hWnd;
		d3d_state.swapchain->GetHwnd(&hWnd);
		::GetClientRect(hWnd, &window_rect);
		uint32_t window_width = DX_MAX(1u, window_rect.right - window_rect.left);
		uint32_t window_height = DX_MAX(1u, window_rect.bottom - window_rect.top);
		if (d3d_state.render_width != window_width ||
			d3d_state.render_height != window_height)
		{
			ResizeRenderResolution(window_width, window_height);
		}
	}

	void Init(const RendererInitParams& params)
	{
		// Creates the adapter, device, command queue and swapchain, etc.
		InitD3DState(params);
		InitPipelines();
		InitDearImGui();
	}

	void Exit()
	{
		ImGui_ImplDX12_Shutdown();
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();
	}

	void BeginFrame()
	{
		// ----------------------------------------------------------------------------------
		// Wait on the current back buffer until all commands on it have finished execution

		// We wait here right before actually using the back buffer again to minimize the amount of time
		// to maximize the amount of work the CPU can do before actually having to wait on the GPU.
		// Some examples on the internet wait right after presenting, which is not ideal
		if (!(d3d_state.fence->GetCompletedValue() >= d3d_state.back_buffer_fence_values[d3d_state.current_back_buffer_idx]))
		{
			HANDLE fence_event = ::CreateEvent(NULL, FALSE, FALSE, NULL);
			DX_ASSERT(fence_event && "Failed to create fence event handle");

			DX_CHECK_HR(d3d_state.fence->SetEventOnCompletion(d3d_state.back_buffer_fence_values[d3d_state.current_back_buffer_idx], fence_event));
			::WaitForSingleObject(fence_event, (DWORD)UINT32_MAX);

			// Do I need to close the fence event handle every time? Rather keep it around if I can
			::CloseHandle(fence_event);
		}

		// ----------------------------------------------------------------------------------
		// Reset the command allocator and command list for the current frame

		// If the command list has not been closed before, this will crash, so we need to check here if the fence value for the next back buffer is bigger than 0
		// which means that the command list was closed before once
		// ID3D12CommandList::Close will set the command list into non recording state
		// ID3D12CommandList::Reset will set the command list into recording state, this can be called even if the commands from this command list are still in-flight
		// ID3D12CommandAllocator::Reset can only be called when the commands from that allocator are no longer in-flight (one allocator per back buffer seems reasonable)
		// Only one command list associated with a command allocator can be in the recording state at any given time, which means that for each thread that populates
		// a command list, you need at least one command allocator and at least one command list
		if (d3d_state.back_buffer_fence_values[d3d_state.current_back_buffer_idx] > 0)
		{
			d3d_state.command_allocator[d3d_state.current_back_buffer_idx]->Reset();
			d3d_state.command_list[d3d_state.current_back_buffer_idx]->Reset(d3d_state.command_allocator[d3d_state.current_back_buffer_idx], nullptr);
		}

		// ----------------------------------------------------------------------------------
		// Begin a new frame for Dear ImGui

		ImGui_ImplWin32_NewFrame();
		ImGui_ImplDX12_NewFrame();
		ImGui::NewFrame();
	}

	void EndFrame()
	{
	}

	void RenderFrame()
	{
		// ----------------------------------------------------------------------------------
		// Create the render target view for the current back buffer

		D3D12_RENDER_TARGET_VIEW_DESC rtv_desc = {};
		rtv_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		rtv_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
		rtv_desc.Texture2D.MipSlice = 0;
		rtv_desc.Texture2D.PlaneSlice = 0;

		ID3D12Resource* back_buffer;
		d3d_state.swapchain->GetBuffer(d3d_state.current_back_buffer_idx, IID_PPV_ARGS(&back_buffer));
		d3d_state.device->CreateRenderTargetView(back_buffer, &rtv_desc, d3d_state.descriptor_heap_rtv->GetCPUDescriptorHandleForHeapStart());

		// ----------------------------------------------------------------------------------
		// Transition the back buffer to the render target state, and clear it

		D3D12_RESOURCE_BARRIER render_target_barrier = {};
		render_target_barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		render_target_barrier.Transition.pResource = back_buffer;
		render_target_barrier.Transition.Subresource = 0;
		render_target_barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
		render_target_barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;

		ID3D12GraphicsCommandList6* cmd_list = d3d_state.command_list[d3d_state.current_back_buffer_idx];
		cmd_list->ResourceBarrier(1, &render_target_barrier);

		float clear_color[4] = { 1, 0, 1, 1 };
		cmd_list->ClearRenderTargetView(d3d_state.descriptor_heap_rtv->GetCPUDescriptorHandleForHeapStart(), clear_color, 0, nullptr);

		// ----------------------------------------------------------------------------------
		// Draw a triangle

		D3D12_VIEWPORT viewport = { 0.0, 0.0, d3d_state.render_width, d3d_state.render_height, 0.0, 1.0 };
		D3D12_RECT scissor_rect = { 0, 0, LONG_MAX, LONG_MAX };

		cmd_list->RSSetViewports(1, &viewport);
		cmd_list->RSSetScissorRects(1, &scissor_rect);

		cmd_list->OMSetRenderTargets(1, &d3d_state.descriptor_heap_rtv->GetCPUDescriptorHandleForHeapStart(), FALSE, nullptr);

		cmd_list->SetGraphicsRootSignature(d3d_state.default_raster_pipeline.root_sig);
		cmd_list->SetPipelineState(d3d_state.default_raster_pipeline.pipeline_state);
		cmd_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		d3d_state.vertex_buffer_ptr[0].pos = {  0.5,  0.5, 0.0 };
		d3d_state.vertex_buffer_ptr[1].pos = {  0.5, -0.5, 0.0 };
		d3d_state.vertex_buffer_ptr[2].pos = { -0.5, -0.5, 0.0 };
		d3d_state.vertex_buffer_ptr[3].pos = { -0.5, -0.5, 0.0 };
		d3d_state.vertex_buffer_ptr[4].pos = { -0.5,  0.5, 0.0 };
		d3d_state.vertex_buffer_ptr[5].pos = {  0.5,  0.5, 0.0 };

		D3D12_VERTEX_BUFFER_VIEW vbv = {};
		vbv.BufferLocation = d3d_state.vertex_buffer->GetGPUVirtualAddress();
		vbv.StrideInBytes = sizeof(Vertex);
		vbv.SizeInBytes = vbv.StrideInBytes * 6;
		cmd_list->IASetVertexBuffers(0, 1, &vbv);
		cmd_list->DrawInstanced(6, 1, 0, 0);

		// ----------------------------------------------------------------------------------
		// Render Dear ImGui

		ImGui::Render();

		cmd_list->OMSetRenderTargets(1, &d3d_state.descriptor_heap_rtv->GetCPUDescriptorHandleForHeapStart(), FALSE, nullptr);
		cmd_list->SetDescriptorHeaps(1, &d3d_state.descriptor_heap_cbv_srv_uav);

		ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), cmd_list);

		// ----------------------------------------------------------------------------------
		// Transition the back buffer to the present state

		D3D12_RESOURCE_BARRIER present_barrier = {};
		present_barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		present_barrier.Transition.pResource = back_buffer;
		present_barrier.Transition.Subresource = 0;
		present_barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
		present_barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
		cmd_list->ResourceBarrier(1, &present_barrier);

		// ----------------------------------------------------------------------------------
		// Close the command list, and execute it on the command queue

		cmd_list->Close();
		ID3D12CommandList* const command_lists[] = { cmd_list };
		d3d_state.swapchain_command_queue->ExecuteCommandLists(1, command_lists);

		// ----------------------------------------------------------------------------------
		// Present the back buffer on the swap chain

		uint32_t sync_interval = d3d_state.vsync_enabled ? 1 : 0;
		uint32_t present_flags = d3d_state.tearing_supported && !d3d_state.vsync_enabled ? DXGI_PRESENT_ALLOW_TEARING : 0;
		DX_CHECK_HR(d3d_state.swapchain->Present(sync_interval, present_flags));

		// ----------------------------------------------------------------------------------
		// Signal the GPU with the fence value for the current frame, and set the current back buffer index to the next one
		
		d3d_state.back_buffer_fence_values[d3d_state.current_back_buffer_idx] = ++d3d_state.fence_value;
		d3d_state.swapchain_command_queue->Signal(d3d_state.fence, d3d_state.back_buffer_fence_values[d3d_state.current_back_buffer_idx]);
		d3d_state.current_back_buffer_idx = d3d_state.swapchain->GetCurrentBackBufferIndex();

		// ----------------------------------------------------------------------------------
		// Check if the window has been resized, and resize the render resolution accordingly
		
		CheckForWindowResize();
	}

	void Flush()
	{
		if (!(d3d_state.fence->GetCompletedValue() >= d3d_state.fence_value))
		{
			HANDLE fence_event = ::CreateEvent(NULL, FALSE, FALSE, NULL);
			DX_ASSERT(fence_event && "Failed to create fence event handle");

			DX_CHECK_HR(d3d_state.fence->SetEventOnCompletion(d3d_state.back_buffer_fence_values[d3d_state.current_back_buffer_idx], fence_event));
			::WaitForSingleObject(fence_event, (DWORD)UINT32_MAX);

			// Do I need to close the fence event handle every time? Rather keep it around if I can
			::CloseHandle(fence_event);
		}
	}

}
