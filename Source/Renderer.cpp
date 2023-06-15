#include "Pch.h"
#include "Renderer.h"

// Should add HR error explanation to these macros as well
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

		// Command queue, list and allocator
		ID3D12CommandQueue* command_queue_direct;
		ID3D12GraphicsCommandList6* command_list[3];
		ID3D12CommandAllocator* command_allocator[3];
		uint64_t fence_value;
		ID3D12Fence* fence;

		// Descriptor heaps
		ID3D12DescriptorHeap* descriptor_heap_rtv;
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

	ID3D12DescriptorHeap* CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t num_descriptors)
	{
		D3D12_DESCRIPTOR_HEAP_DESC descriptor_heap_desc = {};
		descriptor_heap_desc.Type = type;
		descriptor_heap_desc.NumDescriptors = num_descriptors;
		descriptor_heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		descriptor_heap_desc.NodeMask = 0;

		ID3D12DescriptorHeap* descriptor_heap;
		d3d_state.device->CreateDescriptorHeap(&descriptor_heap_desc, IID_PPV_ARGS(&descriptor_heap));

		return descriptor_heap;
	}

	void CreateD3DState(const RendererInitParams& params)
	{
#ifdef _DEBUG
		// Enable debug layer
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
	}

	void Init(const RendererInitParams& params)
	{
		// Creates the adapter, device, command queue and swapchain, etc.
		CreateD3DState(params);
	}

	void Exit()
	{
	}

	void Render()
	{
		D3D12_RENDER_TARGET_VIEW_DESC rtv_desc = {};
		rtv_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		rtv_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
		rtv_desc.Texture2D.MipSlice = 0;
		rtv_desc.Texture2D.PlaneSlice = 0;

		ID3D12Resource* back_buffer;
		d3d_state.swapchain->GetBuffer(d3d_state.current_back_buffer_idx, IID_PPV_ARGS(&back_buffer));
		d3d_state.device->CreateRenderTargetView(back_buffer, &rtv_desc, d3d_state.descriptor_heap_rtv->GetCPUDescriptorHandleForHeapStart());

		D3D12_RESOURCE_BARRIER render_target_barrier = {};
		render_target_barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		render_target_barrier.Transition.pResource = back_buffer;
		render_target_barrier.Transition.Subresource = 0;
		render_target_barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
		render_target_barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
		d3d_state.command_list[d3d_state.current_back_buffer_idx]->ResourceBarrier(1, &render_target_barrier);

		float clear_color[4] = { 1, 0, 1, 1 };
		d3d_state.command_list[d3d_state.current_back_buffer_idx]->ClearRenderTargetView(d3d_state.descriptor_heap_rtv->GetCPUDescriptorHandleForHeapStart(), clear_color, 0, nullptr);

		D3D12_RESOURCE_BARRIER present_barrier = {};
		present_barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		present_barrier.Transition.pResource = back_buffer;
		present_barrier.Transition.Subresource = 0;
		present_barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
		present_barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
		d3d_state.command_list[d3d_state.current_back_buffer_idx]->ResourceBarrier(1, &present_barrier);

		d3d_state.command_list[d3d_state.current_back_buffer_idx]->Close();
		ID3D12CommandList* const command_lists[] = { d3d_state.command_list[d3d_state.current_back_buffer_idx] };
		d3d_state.swapchain_command_queue->ExecuteCommandLists(1, command_lists);

		uint32_t sync_interval = d3d_state.vsync_enabled ? 1 : 0;
		uint32_t present_flags = d3d_state.tearing_supported && !d3d_state.vsync_enabled ? DXGI_PRESENT_ALLOW_TEARING : 0;
		DX_CHECK_HR(d3d_state.swapchain->Present(sync_interval, present_flags));
		
		d3d_state.back_buffer_fence_values[d3d_state.current_back_buffer_idx] = ++d3d_state.fence_value;
		d3d_state.swapchain_command_queue->Signal(d3d_state.fence, d3d_state.back_buffer_fence_values[d3d_state.current_back_buffer_idx]);
		d3d_state.current_back_buffer_idx = d3d_state.swapchain->GetCurrentBackBufferIndex();
		
		if (!(d3d_state.fence->GetCompletedValue() >= d3d_state.back_buffer_fence_values[d3d_state.current_back_buffer_idx]))
		{
			printf("Wait for fence..\n");
			HANDLE fence_event = ::CreateEvent(NULL, FALSE, FALSE, NULL);
			DX_ASSERT(fence_event && "Failed to create fence event handle");

			DX_CHECK_HR(d3d_state.fence->SetEventOnCompletion(d3d_state.back_buffer_fence_values[d3d_state.current_back_buffer_idx], fence_event));
			::WaitForSingleObject(fence_event, (DWORD)UINT32_MAX);

			// Do I need to close the fence event handle every time? Rather keep it around if I can
			::CloseHandle(fence_event);
		}

		// If the command list has not been closed before, this will crash, so we need to check here if the fence value for the next back buffer is bigger than 0
		// which means that the command list was closed before once
		if (d3d_state.back_buffer_fence_values[d3d_state.current_back_buffer_idx] > 0)
		{
			d3d_state.command_allocator[d3d_state.current_back_buffer_idx]->Reset();
			d3d_state.command_list[d3d_state.current_back_buffer_idx]->Reset(d3d_state.command_allocator[d3d_state.current_back_buffer_idx], nullptr);
		}
	}

}
