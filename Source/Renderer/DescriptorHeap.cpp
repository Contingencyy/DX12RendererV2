#include "Pch.h"
#include "Renderer/DescriptorHeap.h"
#include "Renderer/D3DState.h"
#include "Renderer/DX12.h"

DescriptorHeap::DescriptorHeap(LinearAllocator* allocator, D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t num_descriptors)
	: m_allocator(allocator), m_num_descriptors(num_descriptors)
{
	D3D12_DESCRIPTOR_HEAP_FLAGS flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	if (type == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV || type == D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER)
	{
		flags |= D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	}
	m_d3d_descriptor_heap = DX12::CreateDescriptorHeap(type, m_num_descriptors, flags);
	m_descriptor_increment_size = d3d_state.device->GetDescriptorHandleIncrementSize(type);

	// Create a single block that contains all of the descriptors
	m_blocks = allocator->Allocate<DescriptorBlock>();
	m_blocks[0].descriptor_heap_index = 0;
	m_blocks[0].num_descriptors = m_num_descriptors;
	m_free_blocks = nullptr;
}

DescriptorAllocation DescriptorHeap::Allocate(uint32_t num_descriptors)
{
	// Traverse the list of descriptor blocks and find a block that can satisfy the requested allocation
	DescriptorBlock* descriptor_block = m_blocks, *previous_descriptor_block = nullptr;
	while(descriptor_block)
	{
		if (descriptor_block->num_descriptors >= num_descriptors)
		{
			break;
		}

		previous_descriptor_block = descriptor_block;
		descriptor_block = descriptor_block->next;
	}

	// Optionally, if there are no descriptor blocks that could satisfy the allocation request, make the descriptor heap bigger, but that will probably never happen,
	// since we will be using a large descriptor heap with >1 million descriptors, so don't worry about it.
	DX_ASSERT(descriptor_block && "Descriptor heap was not able to satisfy the allocation request");
	
	// Create a new descriptor allocation from the descriptor block
	DescriptorAllocation allocation = {};
	allocation.descriptor_heap_index = descriptor_block->descriptor_heap_index;
	allocation.descriptor_increment_size = m_descriptor_increment_size;
	allocation.num_descriptors = num_descriptors;
	allocation.cpu = DX12::GetCPUDescriptorHandleAtOffset(m_d3d_descriptor_heap, descriptor_block->descriptor_heap_index);
	if (m_d3d_descriptor_heap->GetDesc().Flags & D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE)
	{
		allocation.gpu = DX12::GetGPUDescriptorHandleAtOffset(m_d3d_descriptor_heap, descriptor_block->descriptor_heap_index);
	}
	
	// Shrink the descriptor block by the allocated descriptors
	descriptor_block->descriptor_heap_index += num_descriptors;
	descriptor_block->num_descriptors -= num_descriptors;

	// Pop the descriptor block from the linked list if it is empty
	if (descriptor_block->num_descriptors == 0)
	{
		if (previous_descriptor_block)
		{
			previous_descriptor_block->next = descriptor_block->next;
		}
		else
		{
			m_blocks = descriptor_block->next;
		}

		// Push the now empty descriptor block onto the descriptor block freelist, so that it can be recycled in the future
		if (m_free_blocks)
		{
			m_free_blocks->next = m_free_blocks;
		}
		m_free_blocks = descriptor_block;
	}

	return allocation;
}

void DescriptorHeap::Release(const DescriptorAllocation& alloc)
{
	// Find the descriptor block next to the descriptor allocation
	DescriptorBlock* descriptor_block = m_blocks, * previous_descriptor_block = nullptr;
	while (descriptor_block)
	{
		// Release the descriptors into the block by growing the descriptor block
		if (descriptor_block->descriptor_heap_index + descriptor_block->num_descriptors == alloc.descriptor_heap_index)
		{
			descriptor_block->num_descriptors += alloc.num_descriptors;
			return;
		}

		if (descriptor_block->descriptor_heap_index == alloc.descriptor_heap_index + alloc.num_descriptors)
		{
			descriptor_block->descriptor_heap_index -= alloc.num_descriptors;
			descriptor_block->num_descriptors += alloc.num_descriptors;
			return;
		}
		
		previous_descriptor_block = descriptor_block;
		descriptor_block = descriptor_block->next;
	}
	
	// If there was no adjacent descriptor block found, create a new one
	DescriptorBlock* new_descriptor_block = nullptr;
	if (m_free_blocks)
	{
		// Pop from free list
		new_descriptor_block = m_free_blocks;
		m_free_blocks = m_free_blocks->next;
	}
	else
	{
		new_descriptor_block = m_allocator->Allocate<DescriptorBlock>();
	}

	new_descriptor_block->descriptor_heap_index = alloc.descriptor_heap_index;
	new_descriptor_block->num_descriptors = alloc.num_descriptors;

	// Push to descriptor block list head
	new_descriptor_block->next = m_blocks;
	m_blocks = new_descriptor_block;

	// Optionally, create null views for those freed descriptors
}
