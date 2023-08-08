#include "Pch.h"
#include "Renderer/ResourceTracker.h"
#include "Containers/Hashmap.h"
#include "Renderer/D3DState.h"
#include "Renderer/DX12.h"

namespace ResourceTracker
{

	struct TrackedResource
	{
		ID3D12Resource* resource;
		D3D12_RESOURCE_STATES state;
	};

	struct InternalData
	{
		MemoryScope* memory_scope;
		Hashmap<ID3D12Resource*, TrackedResource>* tracked_resources;
	} static data;

	void Init(MemoryScope* memory_scope, size_t capacity)
	{
		data.memory_scope = memory_scope;
		data.tracked_resources = memory_scope->New<Hashmap<ID3D12Resource*, TrackedResource>>(data.memory_scope);
	}

	void Exit()
	{
		// Loop over the tracked objects and resources, and unmap/release them
		for (uint32_t node_index = 0; node_index < data.tracked_resources->m_capacity; ++node_index)
		{
			Hashmap<ID3D12Resource*, TrackedResource>::Node* node = &data.tracked_resources->m_nodes[node_index];

			if (node->key == Hashmap<ID3D12Resource*, TrackedResource>::NODE_UNUSED)
			{
				continue;
			}
			
			DX_RELEASE_OBJECT(node->value.resource);
		}

		data.tracked_resources->Reset();
	}

	void TrackResource(ID3D12Resource* resource, D3D12_RESOURCE_STATES state)
	{
		TrackedResource* tracked_resource = data.tracked_resources->Find(resource);
		DX_ASSERT(!tracked_resource && "Tried to track a resource that is already tracked");

		if (!tracked_resource)
		{
			data.tracked_resources->Insert(resource,
				{ .resource = resource, .state = state }
			);
		}
	}

	void ReleaseResource(ID3D12Resource* resource)
	{
		TrackedResource* tracked_resource = data.tracked_resources->Find(resource);
		DX_ASSERT(tracked_resource && "Tried to release a tracked resource that was not tracked");

		if (tracked_resource)
		{
			// NOTE: We need to keep a local pointer to the resource to be released here
			// The hashmap remove will reset the value of the key value pair, so DX_RELEASE_OBJECT would try to release a nullptr
			ID3D12Resource* resource = tracked_resource->resource;
			data.tracked_resources->Remove(resource);
			DX_RELEASE_OBJECT(resource);
		}
	}

	D3D12_RESOURCE_STATES GetResourceState(ID3D12Resource* resource)
	{
		TrackedResource* tracked_resource = data.tracked_resources->Find(resource);
		DX_ASSERT(tracked_resource && "Tried to retrieve a tracked resource state for a resource that was not tracked");

		return tracked_resource->state;
	}

	D3D12_RESOURCE_BARRIER TransitionBarrier(ID3D12Resource* resource, D3D12_RESOURCE_STATES new_state)
	{
		TrackedResource* tracked_resource = data.tracked_resources->Find(resource);
		DX_ASSERT(tracked_resource && "Tried to transition a tracked resource for a resource that was not tracked");

		D3D12_RESOURCE_BARRIER transition_barrier = DX12::TransitionBarrier(resource, tracked_resource->state, new_state);
		tracked_resource->state = new_state;

		return transition_barrier;
	}

}
