#include "Pch.h"
#include "Renderer/DX12.h"
#include "Renderer/D3DState.h"
#include "Renderer/ResourceTracker.h"

namespace DX12
{
	wchar_t* UTF16FromUTF8(LinearAllocator* alloc, const char* utf8)
	{
		size_t src_len = strlen(utf8);

		if (src_len == 0)
			return nullptr;

		int dst_len = MultiByteToWideChar(CP_UTF8, 0, utf8, (int)src_len, NULL, 0);
		wchar_t* utf16 = (wchar_t*)alloc->Allocate(sizeof(wchar_t) * (dst_len + 1), alignof(wchar_t));

		MultiByteToWideChar(CP_UTF8, 0, utf8, (int)src_len, (wchar_t*)utf16, dst_len);
		utf16[dst_len] = 0;

		return utf16;
	}

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

	ID3D12DescriptorHeap* CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t num_descriptors, D3D12_DESCRIPTOR_HEAP_FLAGS flags)
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

	D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandleAtOffset(ID3D12DescriptorHeap* descriptor_heap, uint32_t offset)
	{
		size_t descriptor_increment_size = d3d_state.device->GetDescriptorHandleIncrementSize(descriptor_heap->GetDesc().Type);
		return { descriptor_heap->GetCPUDescriptorHandleForHeapStart().ptr + offset * descriptor_increment_size };
	}

	D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandleAtOffset(ID3D12DescriptorHeap* descriptor_heap, uint32_t offset)
	{
		DX_ASSERT(descriptor_heap->GetDesc().Flags == D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE &&
			"Tried to get a GPU descriptor handle from a descriptor heap that is not flagged shader visible");
		size_t descriptor_increment_size = d3d_state.device->GetDescriptorHandleIncrementSize(descriptor_heap->GetDesc().Type);
		return { descriptor_heap->GetGPUDescriptorHandleForHeapStart().ptr + offset * descriptor_increment_size };
	}

	ID3D12RootSignature* CreateRootSignature(const D3D12_VERSIONED_ROOT_SIGNATURE_DESC& root_sig_desc)
	{
		ID3DBlob* serialized_root_sig, *error;
		DX_CHECK_HR_ERR(D3D12SerializeVersionedRootSignature(&root_sig_desc, &serialized_root_sig, &error), "Failed to serialize versioned root signature");
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

		// Open the shader's source file
		IDxcBlobEncoding* source_blob = nullptr;
		DX_CHECK_HR_ERR(d3d_state.dxc_utils->LoadFile(filepath, &codepage, &source_blob), "Failed to load shader from file");
		DxcBuffer dxc_source_buffer = {};
		dxc_source_buffer.Encoding = DXC_CP_ACP;
		dxc_source_buffer.Ptr = source_blob->GetBufferPointer();
		dxc_source_buffer.Size = source_blob->GetBufferSize();

		// Define the shader compilation arguments
		const wchar_t* compile_args[] =
		{
			L"",
			L"-E", L"",
			L"-T", L"",
			DXC_ARG_WARNINGS_ARE_ERRORS,
			DXC_ARG_OPTIMIZATION_LEVEL3,
			DXC_ARG_PACK_MATRIX_ROW_MAJOR,
#ifdef _DEBUG
			DXC_ARG_DEBUG,
			DXC_ARG_SKIP_OPTIMIZATIONS
#endif
		};

		compile_args[0] = filepath;
		compile_args[2] = entry_point;
		compile_args[4] = target_profile;

		// Compile the shader
		IDxcResult* result = nullptr;
		DX_CHECK_HR_ERR(d3d_state.dxc_compiler->Compile(
			&dxc_source_buffer, compile_args, DX_ARRAY_SIZE(compile_args),
			d3d_state.dxc_include_handler, IID_PPV_ARGS(&result)
			), "Failed to compile shader"
		);

		// Check the result of the compilation, and exit if it failed
		result->GetStatus(&hr);
		if (FAILED(hr))
		{
			IDxcBlobEncoding* error;
			result->GetErrorBuffer(&error);
			
			// TODO: Logging
			MessageBoxA(nullptr, (char*)error->GetBufferPointer(), "Failed", MB_OK);
			DX_ASSERT(false && (char*)error->GetBufferPointer());
			DX_RELEASE_OBJECT(error);

			return nullptr;
		}

		// Get the shader binary
		IDxcBlob* shader_binary = nullptr;
		IDxcBlobUtf16* shader_name = nullptr;
		result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shader_binary), &shader_name);
		DX_ASSERT(shader_binary, "Failed to retrieve the shader binary");

		DX_RELEASE_OBJECT(source_blob);
		DX_RELEASE_OBJECT(shader_name);
		DX_RELEASE_OBJECT(result);

		return shader_binary;
	}

	ID3D12PipelineState* CreateGraphicsPipelineState(ID3D12RootSignature* root_sig, DXGI_FORMAT rt_format, DXGI_FORMAT ds_format,
		const wchar_t* vs_path, const wchar_t* ps_path)
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
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TANGENT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TRANSFORM", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 0, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
			{ "TRANSFORM", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 16, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
			{ "TRANSFORM", 2, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 32, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
			{ "TRANSFORM", 3, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 48, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
			{ "BASE_COLOR_TEXTURE", 0, DXGI_FORMAT_R32_UINT, 1, 64, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
			{ "NORMAL_TEXTURE", 0, DXGI_FORMAT_R32_UINT, 1, 68, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
			{ "METALLIC_ROUGHNESS_TEXTURE", 0, DXGI_FORMAT_R32_UINT, 1, 72, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
			{ "METALLIC_FACTOR", 0, DXGI_FORMAT_R32_FLOAT, 1, 76, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
			{ "ROUGHNESS_FACTOR", 0, DXGI_FORMAT_R32_FLOAT, 1, 80, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
		};

		IDxcBlob* vs_blob = CompileShader(vs_path, L"VSMain", L"vs_6_6");
		IDxcBlob* ps_blob = CompileShader(ps_path, L"PSMain", L"ps_6_6");

		D3D12_GRAPHICS_PIPELINE_STATE_DESC pipeline_desc = {};
		pipeline_desc.InputLayout.NumElements = DX_ARRAY_SIZE(input_element_desc);
		pipeline_desc.InputLayout.pInputElementDescs = input_element_desc;
		pipeline_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		pipeline_desc.VS.BytecodeLength = vs_blob->GetBufferSize();
		pipeline_desc.VS.pShaderBytecode = vs_blob->GetBufferPointer();
		pipeline_desc.PS.BytecodeLength = ps_blob->GetBufferSize();
		pipeline_desc.PS.pShaderBytecode = ps_blob->GetBufferPointer();
		pipeline_desc.NumRenderTargets = 1;
		pipeline_desc.RTVFormats[0] = rt_format;
		pipeline_desc.DepthStencilState.DepthEnable = TRUE;
		pipeline_desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
		pipeline_desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
		pipeline_desc.DepthStencilState.StencilEnable = FALSE;
		pipeline_desc.DSVFormat = ds_format;
		pipeline_desc.BlendState.AlphaToCoverageEnable = FALSE;
		pipeline_desc.BlendState.IndependentBlendEnable = TRUE;
		pipeline_desc.BlendState.RenderTarget[0] = rt_blend_desc;
		pipeline_desc.SampleDesc.Count = 1;
		pipeline_desc.SampleDesc.Quality = 0;
		pipeline_desc.SampleMask = UINT32_MAX;
		pipeline_desc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
		pipeline_desc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
		pipeline_desc.NodeMask = 0;
		pipeline_desc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
		pipeline_desc.pRootSignature = root_sig;

		// TODO: Should be able to specify via parameters if this should be a graphics or compute pipeline state
		ID3D12PipelineState* pipeline_state;
		DX_CHECK_HR_ERR(d3d_state.device->CreateGraphicsPipelineState(&pipeline_desc,
			IID_PPV_ARGS(&pipeline_state)), "Failed to create graphics pipeline state");

		DX_RELEASE_OBJECT(vs_blob);
		DX_RELEASE_OBJECT(ps_blob);

		return pipeline_state;
	}

	ID3D12PipelineState* CreateComputePipelineState(ID3D12RootSignature* root_sig, const wchar_t* cs_path)
	{
		IDxcBlob* cs_blob = CompileShader(cs_path, L"main", L"cs_6_6");

		D3D12_COMPUTE_PIPELINE_STATE_DESC pipeline_desc = {};
		pipeline_desc.pRootSignature = root_sig;
		pipeline_desc.CS.BytecodeLength = cs_blob->GetBufferSize();
		pipeline_desc.CS.pShaderBytecode = cs_blob->GetBufferPointer();
		pipeline_desc.NodeMask = 0;
		pipeline_desc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

		ID3D12PipelineState* pipeline_state;
		DX_CHECK_HR_ERR(d3d_state.device->CreateComputePipelineState(&pipeline_desc,
			IID_PPV_ARGS(&pipeline_state)), "Failed to create compute pipeline state");

		DX_RELEASE_OBJECT(cs_blob);

		return pipeline_state;
	}

	ID3D12Resource* CreateBuffer(const wchar_t* name, uint64_t size_in_bytes)
	{
		D3D12_HEAP_PROPERTIES heap_props = {};
		heap_props.Type = D3D12_HEAP_TYPE_DEFAULT;

		D3D12_RESOURCE_DESC resource_desc = {};
		resource_desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		resource_desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		resource_desc.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
		resource_desc.Format = DXGI_FORMAT_UNKNOWN;
		resource_desc.Width = size_in_bytes;
		resource_desc.Height = 1;
		resource_desc.DepthOrArraySize = 1;
		resource_desc.MipLevels = 1;
		resource_desc.SampleDesc.Count = 1;
		resource_desc.Flags = D3D12_RESOURCE_FLAG_NONE;

		ID3D12Resource* buffer;
		DX_CHECK_HR(d3d_state.device->CreateCommittedResource(&heap_props, D3D12_HEAP_FLAG_NONE,
			&resource_desc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&buffer)));
		buffer->SetName(name);

		ResourceTracker::TrackResource(buffer, D3D12_RESOURCE_STATE_COMMON);

		return buffer;
	}

	ID3D12Resource* CreateUploadBuffer(const wchar_t* name, uint64_t size_in_bytes)
	{
		D3D12_HEAP_PROPERTIES heap_props = {};
		heap_props.Type = D3D12_HEAP_TYPE_UPLOAD;

		D3D12_RESOURCE_DESC resource_desc = {};
		resource_desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		resource_desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		resource_desc.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
		resource_desc.Format = DXGI_FORMAT_UNKNOWN;
		resource_desc.Width = size_in_bytes;
		resource_desc.Height = 1;
		resource_desc.DepthOrArraySize = 1;
		resource_desc.MipLevels = 1;
		resource_desc.SampleDesc.Count = 1;
		resource_desc.Flags = D3D12_RESOURCE_FLAG_NONE;

		ID3D12Resource* buffer;
		DX_CHECK_HR(d3d_state.device->CreateCommittedResource(&heap_props, D3D12_HEAP_FLAG_NONE,
			&resource_desc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&buffer)));
		buffer->SetName(name);

		ResourceTracker::TrackResource(buffer, D3D12_RESOURCE_STATE_GENERIC_READ);

		return buffer;
	}

	ID3D12Resource* CreateTexture(const wchar_t* name, DXGI_FORMAT format, uint32_t width, uint32_t height,
		D3D12_RESOURCE_STATES initial_state, const D3D12_CLEAR_VALUE* clear_value, D3D12_RESOURCE_FLAGS flags)
	{
		D3D12_HEAP_PROPERTIES heap_props = {};
		heap_props.Type = D3D12_HEAP_TYPE_DEFAULT;

		D3D12_RESOURCE_DESC resource_desc = {};
		resource_desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		resource_desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		resource_desc.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
		resource_desc.Format = format;
		resource_desc.Width = (uint64_t)width;
		resource_desc.Height = height;
		resource_desc.DepthOrArraySize = 1;
		resource_desc.MipLevels = 1;
		resource_desc.SampleDesc.Count = 1;
		resource_desc.Flags = flags;

		ID3D12Resource* texture;
		DX_CHECK_HR(d3d_state.device->CreateCommittedResource(&heap_props, D3D12_HEAP_FLAG_NONE,
			&resource_desc, initial_state, clear_value, IID_PPV_ARGS(&texture)));
		texture->SetName(name);

		ResourceTracker::TrackResource(texture, initial_state);

		return texture;
	}

	void CreateBufferCBV(ID3D12Resource* resource, D3D12_CPU_DESCRIPTOR_HANDLE descriptor_handle)
	{
		D3D12_RESOURCE_DESC resource_desc = resource->GetDesc();
		uint64_t resource_byte_size = 0;
		d3d_state.device->GetCopyableFootprints(&resource_desc, 0, 1,
			0, nullptr, nullptr, nullptr, &resource_byte_size);

		D3D12_CONSTANT_BUFFER_VIEW_DESC cbv_desc = {};
		cbv_desc.BufferLocation = resource->GetGPUVirtualAddress();
		cbv_desc.SizeInBytes = resource_byte_size;

		d3d_state.device->CreateConstantBufferView(&cbv_desc, descriptor_handle);
	}

	void CreateBufferSRV(ID3D12Resource* resource, D3D12_CPU_DESCRIPTOR_HANDLE descriptor_handle, uint32_t num_elements, uint64_t first_element, uint32_t byte_stride)
	{
		D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
		srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srv_desc.Buffer.NumElements = num_elements;
		srv_desc.Buffer.FirstElement = first_element;
		srv_desc.Buffer.StructureByteStride = byte_stride;

		// Create view for a ByteAddressBuffer
		if (byte_stride == 0)
		{
			srv_desc.Format = DXGI_FORMAT_R32_TYPELESS;
			srv_desc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
		}
		// Create view for a StructuredBuffer
		else
		{
			srv_desc.Format = DXGI_FORMAT_UNKNOWN;
			srv_desc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
		}

		d3d_state.device->CreateShaderResourceView(resource, &srv_desc, descriptor_handle);
	}

	void CreateBufferUAV(ID3D12Resource* resource, D3D12_CPU_DESCRIPTOR_HANDLE descriptor_handle, uint32_t num_elements, uint64_t first_element, uint32_t byte_stride)
	{
		D3D12_UNORDERED_ACCESS_VIEW_DESC uav_desc = {};
		uav_desc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
		uav_desc.Buffer.NumElements = num_elements;
		uav_desc.Buffer.FirstElement = first_element;
		uav_desc.Buffer.StructureByteStride = byte_stride;
		uav_desc.Buffer.CounterOffsetInBytes = 0;

		// Create view for a ByteAddressBuffer
		if (byte_stride == 0)
		{
			uav_desc.Format = DXGI_FORMAT_R32_TYPELESS;
			uav_desc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;
		}
		// Create view for a StructuredBuffer
		else
		{
			uav_desc.Format = DXGI_FORMAT_UNKNOWN;
			uav_desc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
		}

		d3d_state.device->CreateUnorderedAccessView(resource, nullptr, &uav_desc, descriptor_handle);
	}

	void CreateTextureSRV(ID3D12Resource* resource, D3D12_CPU_DESCRIPTOR_HANDLE descriptor_handle, DXGI_FORMAT format, uint32_t num_mips, uint32_t mip_bias)
	{
		D3D12_RESOURCE_DESC resource_desc = resource->GetDesc();

		D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
		srv_desc.Format = format;
		// TODO: Support other view dims once required
		srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srv_desc.Texture2D.MipLevels = num_mips == UINT32_MAX ? resource_desc.MipLevels : num_mips;
		srv_desc.Texture2D.MostDetailedMip = mip_bias;
		srv_desc.Texture2D.PlaneSlice = 0;
		srv_desc.Texture2D.ResourceMinLODClamp = 0;

		d3d_state.device->CreateShaderResourceView(resource, &srv_desc, descriptor_handle);
	}
	
	void CreateTextureUAV(ID3D12Resource* resource, D3D12_CPU_DESCRIPTOR_HANDLE descriptor_handle, DXGI_FORMAT format)
	{
		D3D12_UNORDERED_ACCESS_VIEW_DESC uav_desc = {};
		uav_desc.Format = format;
		uav_desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
		uav_desc.Texture2D.MipSlice = 0;
		uav_desc.Texture2D.PlaneSlice = 0;

		d3d_state.device->CreateUnorderedAccessView(resource, nullptr, &uav_desc, descriptor_handle);
	}

	void CreateTextureRTV(ID3D12Resource* resource, D3D12_CPU_DESCRIPTOR_HANDLE descriptor_handle, DXGI_FORMAT format)
	{
		D3D12_RENDER_TARGET_VIEW_DESC rtv_desc = {};
		rtv_desc.Format = format;
		rtv_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
		rtv_desc.Texture2D.MipSlice = 0;
		rtv_desc.Texture2D.PlaneSlice = 0;

		d3d_state.device->CreateRenderTargetView(resource, &rtv_desc, descriptor_handle);
	}

	void CreateTextureDSV(ID3D12Resource* resource, D3D12_CPU_DESCRIPTOR_HANDLE descriptor_handle, DXGI_FORMAT format)
	{
		D3D12_DEPTH_STENCIL_VIEW_DESC dsv_desc = {};
		dsv_desc.Format = format;
		dsv_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
		dsv_desc.Texture2D.MipSlice = 0;
		dsv_desc.Flags = D3D12_DSV_FLAG_NONE;

		d3d_state.device->CreateDepthStencilView(resource, &dsv_desc, descriptor_handle);
	}

	D3D12_RESOURCE_BARRIER TransitionBarrier(ID3D12Resource* resource, D3D12_RESOURCE_STATES state_before, D3D12_RESOURCE_STATES state_after)
	{
		D3D12_RESOURCE_BARRIER barrier = {};
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Transition.pResource = resource;
		barrier.Transition.Subresource = 0;
		barrier.Transition.StateBefore = state_before;
		barrier.Transition.StateAfter = state_after;
		barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;

		return barrier;
	}

	void ExecuteCommandList(ID3D12CommandQueue* cmd_queue, ID3D12GraphicsCommandList6* cmd_list)
	{
		cmd_list->Close();
		ID3D12CommandList* const command_lists[] = { cmd_list };
		cmd_queue->ExecuteCommandLists(1, command_lists);
	}

	void SignalCommandQueue(ID3D12CommandQueue* cmd_queue, ID3D12Fence* fence, uint64_t fence_value)
	{
		cmd_queue->Signal(fence, fence_value);
	}

	void WaitOnFence(ID3D12CommandQueue* cmd_queue, ID3D12Fence* fence, uint64_t fence_value)
	{
		if (fence->GetCompletedValue() < fence_value)
		{
			HANDLE fence_event = ::CreateEvent(NULL, FALSE, FALSE, NULL);
			DX_ASSERT(fence_event && "Failed to create fence event handle");

			DX_CHECK_HR(d3d_state.frame_fence->SetEventOnCompletion(fence_value, fence_event));
			::WaitForSingleObjectEx(fence_event, UINT32_MAX, FALSE);

			::CloseHandle(fence_event);
		}
	}

}
