#pragma once

#define DX_RESOURCE_TRACKER_DEFAULT_CAPACITY 1024

namespace ResourceTracker
{

	void Init(MemoryScope* memory_scope, size_t capacity = DX_RESOURCE_TRACKER_DEFAULT_CAPACITY);
	void Exit();

	void TrackResource(ID3D12Resource* resource, D3D12_RESOURCE_STATES state);
	void ReleaseResource(ID3D12Resource* resource);
	D3D12_RESOURCE_STATES GetResourceState(ID3D12Resource* resource);

	D3D12_RESOURCE_BARRIER TransitionBarrier(ID3D12Resource* resource, D3D12_RESOURCE_STATES new_state);

}
