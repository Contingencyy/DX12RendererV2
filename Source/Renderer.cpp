#include "Pch.h"
#include "Renderer.h"

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
		IDXGIAdapter4* adapter;
		ID3D12Device8* device;
		IDXGISwapChain4* swapchain;
		ID3D12CommandQueue* swapchain_command_queue;

		bool tearing_supported;
		uint32_t current_back_buffer_idx;
	} static d3d_state;

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
			DXGI_ADAPTER_DESC1 dxgi_adapter_desc = {};
			DX_CHECK_HR(dxgi_adapter->GetDesc1(&dxgi_adapter_desc));

			if ((dxgi_adapter_desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) == 0 &&
				SUCCEEDED(D3D12CreateDevice(dxgi_adapter, d3d_min_feature_level, __uuidof(ID3D12Device), nullptr)) &&
				dxgi_adapter_desc.DedicatedVideoMemory > max_dedicated_video_memory)
			{
				max_dedicated_video_memory = dxgi_adapter_desc.DedicatedVideoMemory;
				DX_CHECK_HR(dxgi_adapter->QueryInterface(IID_PPV_ARGS(&d3d_state.adapter)));
			}
		}

		// Create the device
		DX_CHECK_HR_ERR(D3D12CreateDevice(d3d_state.adapter, d3d_min_feature_level, IID_PPV_ARGS(&d3d_state.device)), "Failed to create D3D12 device");

		// Set info queue severity behavior
		ID3D12InfoQueue* d3d_info_queue;
		DX_CHECK_HR_BRANCH(d3d_state.device->QueryInterface(IID_PPV_ARGS(&d3d_info_queue)))
		{
			DX_CHECK_HR(d3d_info_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE));
			DX_CHECK_HR(d3d_info_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE));
			DX_CHECK_HR(d3d_info_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, TRUE));
		}

		// Create the command queue for the swap chain
		D3D12_COMMAND_QUEUE_DESC d3d_command_queue_desc = {};
		d3d_command_queue_desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
		d3d_command_queue_desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
		d3d_command_queue_desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
		d3d_command_queue_desc.NodeMask = 0;

		DX_CHECK_HR_ERR(d3d_state.device->CreateCommandQueue(&d3d_command_queue_desc, IID_PPV_ARGS(&d3d_state.swapchain_command_queue)), "Failed to create D3D12 command queue for swap chain");

		// Create the swap chain
		BOOL allow_tearing = FALSE;
		DX_CHECK_HR_BRANCH(dxgi_factory->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allow_tearing, sizeof(BOOL)))
		{
			d3d_state.tearing_supported = (allow_tearing == TRUE);
		}

		DXGI_SWAP_CHAIN_DESC1 dxgi_swap_chain_desc = {};
		dxgi_swap_chain_desc.Width = params.width;
		dxgi_swap_chain_desc.Height = params.height;
		dxgi_swap_chain_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		dxgi_swap_chain_desc.Stereo = FALSE;
		dxgi_swap_chain_desc.SampleDesc = { 1, 0 };
		dxgi_swap_chain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		dxgi_swap_chain_desc.BufferCount = 3;
		dxgi_swap_chain_desc.Scaling = DXGI_SCALING_STRETCH;
		dxgi_swap_chain_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		dxgi_swap_chain_desc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
		dxgi_swap_chain_desc.Flags = d3d_state.tearing_supported ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;

		IDXGISwapChain1* dxgi_swap_chain;
		DX_CHECK_HR_ERR(dxgi_factory->CreateSwapChainForHwnd(d3d_state.swapchain_command_queue, params.hWnd, &dxgi_swap_chain_desc, nullptr, nullptr, &dxgi_swap_chain), "Failed to create DXGI swap chain");
		DX_CHECK_HR(dxgi_swap_chain->QueryInterface(&d3d_state.swapchain));
		DX_CHECK_HR(dxgi_factory->MakeWindowAssociation(params.hWnd, DXGI_MWA_NO_ALT_ENTER));
		d3d_state.current_back_buffer_idx = d3d_state.swapchain->GetCurrentBackBufferIndex();
	}

	void Init(const RendererInitParams& params)
	{
		// Creates the adapter, device, command queue and swapchain, etc.
		CreateD3DState(params);
	}

	void Exit()
	{

	}

}
