#pragma once
#include <new>

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
	void Reset();
	void Reset(void* ptr);
	void Decommit();

};

template<typename T>
void GetDestructorFunc(void* ptr)
{
	static_cast<T*>(ptr)->~T();
}

class MemoryScope
{
public:
	MemoryScope() = default;
	MemoryScope(LinearAllocator* alloc, uint8_t* reset_ptr)
		: m_alloc(alloc), m_reset_ptr(reset_ptr), m_destructor_list(nullptr)
	{
	}

	~MemoryScope()
	{
		// Call constructors
		while (m_destructor_list)
		{
			Destructor* destructor = m_destructor_list;
			m_destructor_list = destructor->next;

			void* obj = GetObjectFromDestructor(destructor);
			destructor->func(obj);
		}

		// Reset allocator
		m_alloc->Reset(m_reset_ptr);
		m_alloc->Decommit();
	}

	template<typename T>
	T* Allocate(size_t count = 1)
	{
		return (T*)m_alloc->Allocate(sizeof(T) * count, alignof(T));
	}

	template<typename T, typename... TArgs>
	T* AllocateConstruct(TArgs&&... args)
	{
		if constexpr (!std::is_trivially_destructible_v<T>)
		{
			Destructor* destructor = (Destructor*)m_alloc->Allocate(sizeof(Destructor), alignof(Destructor));
			destructor->func = &GetDestructorFunc<T>;
			destructor->next = m_destructor_list;
			m_destructor_list = destructor;
		}

		T* result = (T*)m_alloc->Allocate(sizeof(T), alignof(T));
		result = new (result) T((args)...);

		return result;
	}

private:
	struct Destructor
	{
		void (*func) (void* ptr);
		Destructor* next;
	};

private:
	void* GetObjectFromDestructor(Destructor* destructor)
	{
		return destructor + 1;
	}

private:
	LinearAllocator* m_alloc;
	void* m_reset_ptr;
	Destructor* m_destructor_list;

};

extern thread_local LinearAllocator g_thread_alloc;
