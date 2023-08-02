#include "Pch.h"
#include "LinearAllocator.h"

MemoryStatistics g_global_memory_stats;
thread_local LinearAllocator g_thread_alloc;

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

#ifdef TRACK_GLOBAL_MEMORY_STATISTICS
			g_global_memory_stats.total_committed_bytes += commit_chunk_size;
#endif
#ifdef TRACK_LOCAL_MEMORY_STATISTICS
			allocator->memory_stats.total_committed_bytes += commit_chunk_size;
#endif
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


#ifdef TRACK_GLOBAL_MEMORY_STATISTICS
		g_global_memory_stats.total_allocated_bytes += at_ptr - alloc;
#endif
#ifdef TRACK_LOCAL_MEMORY_STATISTICS
		memory_stats.total_allocated_bytes += at_ptr - alloc;
#endif
	}

	return alloc;
}

void LinearAllocator::Reset()
{
	// Reset the current at pointer to the beginning
#ifdef TRACK_GLOBAL_MEMORY_STATISTICS
	g_global_memory_stats.total_deallocated_bytes += at_ptr - base_ptr;
#endif
#ifdef TRACK_LOCAL_MEMORY_STATISTICS
	memory_stats.total_deallocated_bytes += at_ptr - base_ptr;
#endif
	at_ptr = base_ptr;
}

void LinearAllocator::Reset(void* ptr)
{
	if (ptr)
	{
		// Reset the current at pointer to a previous state
#ifdef TRACK_GLOBAL_MEMORY_STATISTICS
		g_global_memory_stats.total_deallocated_bytes += at_ptr - ptr;
#endif
#ifdef TRACK_LOCAL_MEMORY_STATISTICS
		memory_stats.total_deallocated_bytes += at_ptr - ptr;
#endif
		at_ptr = (uint8_t*)ptr;
	}
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

#ifdef TRACK_GLOBAL_MEMORY_STATISTICS
	g_global_memory_stats.total_decommitted_bytes += decommit_bytes;
#endif
#ifdef TRACK_LOCAL_MEMORY_STATISTICS
	memory_stats.total_decommitted_bytes += decommit_bytes;
#endif
}

void LinearAllocator::Release()
{
	VirtualMemory::Release(base_ptr);
	base_ptr = at_ptr = end_ptr = committed_ptr = nullptr;
}
