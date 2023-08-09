#pragma once
#include "Shaders/Shared.hlsl.h"
#include "Renderer/DescriptorHeap.h"

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

#define DX_RELEASE_INTERFACE(object) object->Release()

enum ReservedDescriptorRTV : uint32_t
{
	ReservedDescriptorRTV_HDRRenderTarget,
	ReservedDescriptorRTV_SDRRenderTarget,
	ReservedDescriptorRTV_Count
};

enum ReservedDescriptorDSV : uint32_t
{
	ReservedDescriptorDSV_DepthBuffer,
	ReservedDescriptorDSV_Count
};

enum ReservedDescriptorCBVSRVUAV : uint32_t
{
	ReservedDescriptorSRV_DearImGui,
	ReservedDescriptorSRV_HDRRenderTarget,
	ReservedDescriptorUAV_SDRRenderTarget,
	ReservedDescriptorCBVSRVUAV_Count
};

struct PipelineState
{
	ID3D12RootSignature* d3d_root_sig;
	ID3D12PipelineState* d3d_pso;
};

struct InstanceData
{
	float4x4 transform;
	uint base_color_texture_index;
	uint normal_texture_index;
	uint metallic_roughness_texture_index;
	float metallic_factor;
	float roughness_factor;
};

struct D3DState
{

#define DX_BACK_BUFFER_COUNT 3
#define DX_DESCRIPTOR_HEAP_SIZE_RTV ReservedDescriptorRTV_Count
#define DX_DESCRIPTOR_HEAP_SIZE_DSV ReservedDescriptorDSV_Count
#define DX_DESCRIPTOR_HEAP_SIZE_CBV_SRV_UAV ReservedDescriptorCBVSRVUAV_Count + 1024

	// Adapter and device
	IDXGIAdapter4* adapter;
	DXGI_ADAPTER_DESC1 adapter_desc;
	ID3D12Device8* device;

	// Swap chain
	IDXGISwapChain4* swapchain;
	ID3D12CommandQueue* swapchain_command_queue;
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

		// Render settings constant buffer
		ID3D12Resource* render_settings_cb;
		RenderSettings* render_settings_ptr;

		// Scene constant buffer
		ID3D12Resource* scene_cb;
		SceneData* scene_cb_ptr;
	} frame_ctx[DX_BACK_BUFFER_COUNT];

	// Render resolution
	// TODO: Should separate some of these into an API agnostic renderer state, since these do not depend on D3D, same goes for vsync
	uint32_t render_width;
	uint32_t render_height;
	uint64_t frame_index;

	// Render targets
	ID3D12Resource* hdr_render_target;
	ID3D12Resource* sdr_render_target;
	ID3D12Resource* depth_buffer;

	// Fences
	ID3D12Fence* frame_fence;
	uint64_t frame_fence_value;

	// Descriptor heaps
	DescriptorHeap* descriptor_heap_rtv;
	DescriptorHeap* descriptor_heap_dsv;
	DescriptorHeap* descriptor_heap_cbv_srv_uav;

	// Descriptor allocations
	DescriptorAllocation reserved_rtvs;
	DescriptorAllocation reserved_dsvs;
	DescriptorAllocation reserved_cbv_srv_uavs;

	// DXC shader compiler
	IDxcCompiler* dxc_compiler;
	IDxcUtils* dxc_utils;
	IDxcIncludeHandler* dxc_include_handler;

	// Pipeline states
	PipelineState default_raster_pipeline;
	PipelineState post_process_pipeline;

	// Upload buffer
	ID3D12Resource* upload_buffer;
	uint8_t* upload_buffer_ptr;

	bool initialized;
};

extern D3DState d3d_state;
