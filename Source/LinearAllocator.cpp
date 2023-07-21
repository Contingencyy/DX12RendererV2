#include "Pch.h"
#include "LinearAllocator.h"

namespace VirtualMemory
{

	void* Reserve(size_t reserve_byte_size)
	{
		void* reserve = VirtualAlloc(NULL, reserve_byte_size, MEM_RESERVE, PAGE_NOACCESS);
		DX_ASSERT(reserve && "Failed to reserve virtual memory");
		return reserve;
	}

	bool Commit(void* address, size_t num_bytes)
	{
		void* committed = VirtualAlloc(address, num_bytes, MEM_COMMIT, PAGE_READWRITE);
		DX_ASSERT(committed && "Failed to commit virtual memory");
		return committed;
	}

	void Decommit(void* address, size_t num_bytes)
	{
		int status = VirtualFree(address, num_bytes, MEM_DECOMMIT);
		DX_ASSERT(status > 0 && "Failed to decommit virtual memory");
	}

	void Release(void* address)
	{
		int status = VirtualFree(address, 0, MEM_RELEASE);
		DX_ASSERT(status > 0 && "Failed to release virtual memory");
	}

}

size_t GetAlignedByteSizeLeft(LinearAllocator* allocator, size_t align)
{
	size_t byte_size_left = allocator->end_ptr - allocator->at_ptr;
	size_t aligned_byte_size_left = DX_ALIGN_DOWN_POW2(byte_size_left, align);
	return aligned_byte_size_left;
}

void AdvancePointer(LinearAllocator* allocator, uint8_t* new_at_ptr)
{
	if (new_at_ptr >= allocator->base_ptr && new_at_ptr < allocator->end_ptr)
	{
		if (new_at_ptr > allocator->committed_ptr)
		{
			// We ran out of committed memory, so we need to commit some more
			size_t commit_chunk_size = DX_ALIGN_POW2(new_at_ptr - allocator->committed_ptr, ALLOCATOR_DEFAULT_COMMIT_CHUNK_SIZE);
			VirtualMemory::Commit(allocator->committed_ptr, commit_chunk_size);
			allocator->committed_ptr += commit_chunk_size;
		}

		allocator->at_ptr = new_at_ptr;
	}
}

void* LinearAllocator::Allocate(size_t num_bytes, size_t align)
{
	if (!base_ptr)
	{
		// We actually have not reserved any virtual memory yet, so lets do that
		base_ptr = (uint8_t*)VirtualMemory::Reserve(ALLOCATOR_DEFAULT_RESERVE_SIZE);
		at_ptr = base_ptr;
		end_ptr = base_ptr + ALLOCATOR_DEFAULT_RESERVE_SIZE;
		committed_ptr = base_ptr;
	}

	uint8_t* alloc = nullptr;

	if (GetAlignedByteSizeLeft(this, align) >= num_bytes)
	{
		alloc = (uint8_t*)DX_ALIGN_POW2(at_ptr, align);
		AdvancePointer(this, alloc + num_bytes);
		// We initialize the allocated bytes to 0
		memset(alloc, 0, num_bytes);
	}

	return alloc;
}

void LinearAllocator::Reset()
{
	// Reset the current at pointer to the beginning
	at_ptr = base_ptr;
}

void LinearAllocator::Reset(void* ptr)
{
	// Reset the current at pointer to a previous state
	// TODO: Scope stacks, call finalizers when resetting the pointer
	at_ptr = (uint8_t*)ptr;
}

void LinearAllocator::Decommit()
{
	// This will decommit all of the memory in the allocator, except for the last ALLOCATOR_DEFAULT_DECOMMIT_LEFTOVER_SIZE bytes
	uint8_t* at_aligned = (uint8_t*)DX_ALIGN_POW2(at_ptr, ALLOCATOR_DEFAULT_COMMIT_CHUNK_SIZE);
	uint8_t* decommit_from = DX_MAX(at_aligned, base_ptr + ALLOCATOR_DEFAULT_DECOMMIT_LEFTOVER_SIZE);
	size_t decommit_bytes = DX_MAX(0, committed_ptr - decommit_from);

	if (decommit_bytes > 0)
	{
		VirtualMemory::Decommit(decommit_from, decommit_bytes);
		committed_ptr = decommit_from;
	}
}

thread_local LinearAllocator g_thread_alloc;
