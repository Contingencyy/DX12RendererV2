#include "Pch.h"
#include "Renderer/DX12.h"
#include "Renderer/D3DState.h"

namespace DX12
{

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

	ID3D12RootSignature* CreateRootSignature()
	{
		D3D12_ROOT_PARAMETER1 root_params[1] = {};
		root_params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		root_params[0].Descriptor.ShaderRegister = 0;
		root_params[0].Descriptor.RegisterSpace = 0;
		root_params[0].Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_NONE;
		root_params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

		D3D12_STATIC_SAMPLER_DESC static_samplers[1] = {};
		static_samplers[0].Filter = D3D12_FILTER_COMPARISON_MIN_LINEAR_MAG_MIP_POINT;
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

		D3D12_VERSIONED_ROOT_SIGNATURE_DESC versioned_root_sig_desc = {};
		versioned_root_sig_desc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
		versioned_root_sig_desc.Desc_1_1.NumParameters = DX_ARRAY_SIZE(root_params);
		versioned_root_sig_desc.Desc_1_1.pParameters = root_params;
		versioned_root_sig_desc.Desc_1_1.NumStaticSamplers = DX_ARRAY_SIZE(static_samplers);
		versioned_root_sig_desc.Desc_1_1.pStaticSamplers = static_samplers;
		versioned_root_sig_desc.Desc_1_1.Flags =
			D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED |
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

		ID3DBlob* serialized_root_sig, * error;
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
			// TODO: Logging
			MessageBoxA(nullptr, (char*)error->GetBufferPointer(), "Failed", MB_OK);
			DX_ASSERT(false && (char*)error->GetBufferPointer());

			return nullptr;
		}

		IDxcBlob* blob;
		result->GetResult(&blob);

		DX_RELEASE_OBJECT(shader_text);
		DX_RELEASE_OBJECT(result);

		return blob;
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
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TRANSFORM", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 0, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
			{ "TRANSFORM", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 16, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
			{ "TRANSFORM", 2, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 32, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
			{ "TRANSFORM", 3, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 48, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
			{ "BASE_COLOR_TEXTURE", 0, DXGI_FORMAT_R32_UINT, 1, 64, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
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
		pipeline_desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
		pipeline_desc.DepthStencilState.DepthEnable = TRUE;
		pipeline_desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
		pipeline_desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
		pipeline_desc.DepthStencilState.StencilEnable = FALSE;
		pipeline_desc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
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
			IID_PPV_ARGS(&pipeline_state)), "Failed to create pipeline state");

		DX_RELEASE_OBJECT(vs_blob);
		DX_RELEASE_OBJECT(ps_blob);

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

		return buffer;
	}

	ID3D12Resource* CreateTexture(const wchar_t* name, DXGI_FORMAT format, uint32_t width, uint32_t height,
		const D3D12_CLEAR_VALUE* clear_value, D3D12_RESOURCE_STATES initial_state, D3D12_RESOURCE_FLAGS flags)
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

		return texture;
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

	void WaitOnFence(ID3D12CommandQueue* cmd_queue, ID3D12Fence* fence, uint64_t fence_value)
	{
		cmd_queue->Signal(fence, fence_value);

		if (fence->GetCompletedValue() < fence_value)
		{
			HANDLE fence_event = ::CreateEvent(NULL, FALSE, FALSE, NULL);
			DX_ASSERT(fence_event && "Failed to create fence event handle");

			DX_CHECK_HR(d3d_state.fence->SetEventOnCompletion(fence_value, fence_event));
			::WaitForSingleObject(fence_event, (DWORD)UINT32_MAX);

			::CloseHandle(fence_event);
		}
	}

}
