#pragma once

struct RasterPipeline
{
	ID3D12RootSignature* root_sig;
	ID3D12PipelineState* pipeline_state;
};

struct SceneData
{
	Mat4x4 view;
	Mat4x4 projection;
	Mat4x4 view_projection;
};

struct InstanceData
{
	Mat4x4 transform;
};

struct D3DState
{

#define DX_BACK_BUFFER_COUNT 3
#define DX_DESCRIPTOR_HEAP_SIZE_RTV 1
#define DX_DESCRIPTOR_HEAP_SIZE_DSV 1
#define DX_DESCRIPTOR_HEAP_SIZE_CBV_SRV_UAV 1024

	// Adapter and device
	IDXGIAdapter4* adapter;
	ID3D12Device8* device;

	// Swap chain
	IDXGISwapChain4* swapchain;
	ID3D12CommandQueue* swapchain_command_queue;
	ID3D12Resource* back_buffers[DX_BACK_BUFFER_COUNT];
	ID3D12Resource* depth_buffers[DX_BACK_BUFFER_COUNT];
	bool tearing_supported;
	bool vsync_enabled;
	uint32_t current_back_buffer_idx;
	uint64_t back_buffer_fence_values[DX_BACK_BUFFER_COUNT];

	// Render resolution
	// TODO: Should separate some of these into an API agnostic renderer state, since these do not depend on D3D, same goes for vsync
	uint32_t render_width;
	uint32_t render_height;
	uint64_t frame_index;

	// Command queue, list and allocator
	//ID3D12CommandQueue* command_queue_direct;
	ID3D12GraphicsCommandList6* command_lists[DX_BACK_BUFFER_COUNT];
	ID3D12CommandAllocator* command_allocators[DX_BACK_BUFFER_COUNT];
	uint64_t fence_value;
	ID3D12Fence* fence;

	// Descriptor heaps
	ID3D12DescriptorHeap* descriptor_heap_rtv;
	ID3D12DescriptorHeap* descriptor_heap_dsv;
	ID3D12DescriptorHeap* descriptor_heap_cbv_srv_uav;
	uint32_t cbv_srv_uav_current_index;

	// DXC shader compiler
	IDxcCompiler* dxc_compiler;
	IDxcUtils* dxc_utils;
	IDxcIncludeHandler* dxc_include_handler;

	// Pipeline states
	RasterPipeline default_raster_pipeline;

	// Scene constant buffer
	ID3D12Resource* scene_cb;
	SceneData* scene_cb_ptr;

	// Upload buffer
	ID3D12Resource* upload_buffer;
	uint8_t* upload_buffer_ptr;

	ID3D12Resource* instance_buffer;
	InstanceData* instance_buffer_ptr;
	D3D12_VERTEX_BUFFER_VIEW instance_vbv;

	bool initialized;
};

extern D3DState d3d_state;
