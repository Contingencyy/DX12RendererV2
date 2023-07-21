#pragma once

typedef unsigned char uint8_t;

struct LinearAllocator
{

#define ALLOCATOR_DEFAULT_RESERVE_SIZE DX_GB(4ull)
#define ALLOCATOR_DEFAULT_COMMIT_CHUNK_SIZE DX_KB(4ull)
#define ALLOCATOR_DEFAULT_DECOMMIT_LEFTOVER_SIZE ALLOCATOR_DEFAULT_COMMIT_CHUNK_SIZE

	uint8_t* base_ptr;
	uint8_t* at_ptr;
	uint8_t* end_ptr;
	uint8_t* committed_ptr;

	void* Allocate(size_t num_bytes, size_t align);
	template<typename T>
	T* Allocate(size_t count = 1)
	{
		return (T*)Allocate(sizeof(T) * count, alignof(T));
	}
	template<typename T, typename... TArgs>
	T* AllocateConstruct(TArgs&&... args)
	{
		T* obj = (T*)Allocate(sizeof(T), alignof(T));
		new (obj) T((args)...);
		return obj;
	}
	void Reset();
	void Reset(void* ptr);
	void Decommit();

};

extern thread_local LinearAllocator g_thread_alloc;
