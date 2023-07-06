#pragma once
#include "Shaders/Shared.hlsl.h"

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

#define DX_SUCCESS_HR(hr) \
if (SUCCEEDED(hr))

#define DX_FAILED_HR(hr) \
if (FAILED(hr)) \
{ \
	DX_ASSERT(false); \
} \
else

#if 0
#define DX_RELEASE_OBJECT(object) \
if ((object)) \
{ \
	ULONG ref_count = (object)->Release(); \
	DX_ASSERT(ref_count == 0); \
} \
(object) = nullptr
#else
#define DX_RELEASE_OBJECT(object) \
if ((object)) \
{ \
	ULONG ref_count = (object)->Release(); \
	while (ref_count > 0) \
	{ \
/* Add a log warning here if it has to release more than once */ \
		ref_count = (object)->Release(); \
	} \
} \
(object) = nullptr
#endif

struct RasterPipeline
{
	ID3D12RootSignature* root_sig;
	ID3D12PipelineState* pipeline_state;
};

struct InstanceData
{
	float4x4 transform;
	uint base_color_index;
};

struct D3DState
{

#define DX_BACK_BUFFER_COUNT 3
#define DX_DESCRIPTOR_HEAP_SIZE_RTV 1
#define DX_DESCRIPTOR_HEAP_SIZE_DSV 1
#define DX_DESCRIPTOR_HEAP_SIZE_CBV_SRV_UAV 1024
#define DX_TIMESTAMP_HEAP_MAX_QUERIES_PER_FRAME 64

	// Adapter and device
	IDXGIAdapter4* adapter;
	ID3D12Device8* device;

	// Swap chain
	IDXGISwapChain4* swapchain;
	ID3D12CommandQueue* swapchain_command_queue;
	ID3D12Resource* depth_buffer;
	bool tearing_supported;
	bool vsync_enabled;
	uint32_t current_back_buffer_idx;

	struct FrameContext
	{
		ID3D12Resource* back_buffer;
		uint64_t back_buffer_fence_value;

		ID3D12CommandAllocator* command_allocator;
		ID3D12GraphicsCommandList6* command_list;

		ID3D12Resource* instance_buffer;
		InstanceData* instance_buffer_ptr;
		D3D12_VERTEX_BUFFER_VIEW instance_vbv;
	} frame_ctx[DX_BACK_BUFFER_COUNT];

	// Render resolution
	// TODO: Should separate some of these into an API agnostic renderer state, since these do not depend on D3D, same goes for vsync
	uint32_t render_width;
	uint32_t render_height;
	uint64_t frame_index;

	// Command queue, list and allocator
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

	bool initialized;
};

extern D3DState d3d_state;
