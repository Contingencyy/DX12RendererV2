#pragma once

namespace DX12
{

	wchar_t* UTF16FromUTF8(LinearAllocator* alloc, const char* utf8);

	// ------------------------------------------------------------------------------------------------
	// Command queues, descriptor heaps

	ID3D12CommandQueue* CreateCommandQueue(D3D12_COMMAND_LIST_TYPE type, D3D12_COMMAND_QUEUE_PRIORITY priority);
	ID3D12DescriptorHeap* CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t num_descriptors, D3D12_DESCRIPTOR_HEAP_FLAGS flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE);
	D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandleAtOffset(ID3D12DescriptorHeap* descriptor_heap, uint32_t offset);
	D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandleAtOffset(ID3D12DescriptorHeap* descriptor_heap, uint32_t offset);

	// ------------------------------------------------------------------------------------------------
	// Root signatures, shaders, pipeline states

	ID3D12RootSignature* CreateRootSignature();
	IDxcBlob* CompileShader(const wchar_t* filepath, const wchar_t* entry_point, const wchar_t* target_profile);
	ID3D12PipelineState* CreatePipelineState(ID3D12RootSignature* root_sig, const wchar_t* vs_path, const wchar_t* ps_path);

	// ------------------------------------------------------------------------------------------------
	// Buffers

	ID3D12Resource* CreateBuffer(const wchar_t* name, uint64_t size_in_bytes);
	ID3D12Resource* CreateUploadBuffer(const wchar_t* name, uint64_t size_in_bytes);

	// ------------------------------------------------------------------------------------------------
	// Textures

	ID3D12Resource* CreateTexture(const wchar_t* name, DXGI_FORMAT format, uint32_t width, uint32_t height,
		D3D12_RESOURCE_STATES initial_state = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
		const D3D12_CLEAR_VALUE* clear_value = nullptr, D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE);

	// ------------------------------------------------------------------------------------------------
	// Resource views

	void CreateBufferCBV(ID3D12Resource* resource, D3D12_CPU_DESCRIPTOR_HANDLE descriptor_handle);
	void CreateBufferSRV(ID3D12Resource* resource, D3D12_CPU_DESCRIPTOR_HANDLE descriptor_handle, uint32_t num_elements, uint64_t first_element, uint32_t byte_stride);
	void CreateBufferUAV(ID3D12Resource* resource, D3D12_CPU_DESCRIPTOR_HANDLE descriptor_handle, uint32_t num_elements, uint64_t first_element, uint32_t byte_stride);
	void CreateTextureSRV(ID3D12Resource* resource, D3D12_CPU_DESCRIPTOR_HANDLE descriptor_handle, DXGI_FORMAT format, uint32_t num_mips = UINT32_MAX, uint32_t mip_bias = 0);
	void CreateTextureUAV(ID3D12Resource* resource, D3D12_CPU_DESCRIPTOR_HANDLE descriptor_handle, DXGI_FORMAT format);
	void CreateTextureRTV(ID3D12Resource* resource, D3D12_CPU_DESCRIPTOR_HANDLE descriptor_handle, DXGI_FORMAT format);
	void CreateTextureDSV(ID3D12Resource* resource, D3D12_CPU_DESCRIPTOR_HANDLE descriptor_handle, DXGI_FORMAT format);

	// ------------------------------------------------------------------------------------------------
	// Barriers

	D3D12_RESOURCE_BARRIER TransitionBarrier(ID3D12Resource* resource, D3D12_RESOURCE_STATES state_before, D3D12_RESOURCE_STATES state_after);

	// ------------------------------------------------------------------------------------------------
	// Executing command lists, waiting for fence

	void ExecuteCommandList(ID3D12CommandQueue* cmd_queue, ID3D12GraphicsCommandList6* cmd_list);
	void SignalCommandQueue(ID3D12CommandQueue* cmd_queue, ID3D12Fence* fence, uint64_t fence_value);
	void WaitOnFence(ID3D12CommandQueue* cmd_queue, ID3D12Fence* fence, uint64_t fence_value);

}
