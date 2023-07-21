#pragma once

struct ResourceHandle
{
	union
	{
		uint64_t handle;
		struct
		{
			uint32_t index;
			uint32_t version;
		};
	};
};

#define DX_RESOURCE_HANDLE_NULL RenderResourceHandle{0}
#define DX_RESOURCE_HANDLE_VALID(handle) (handle.index != 0)

#define DX_RESOURCE_SLOTMAP_DEFAULT_CAPACITY 1024

template<typename TResource>
class ResourceSlotmap
{
public:
	static constexpr uint32_t SLOT_OCCUPIED = 0xFFFFFFFF;

public:
	ResourceSlotmap(LinearAllocator* alloc, size_t capacity = DX_RESOURCE_SLOTMAP_DEFAULT_CAPACITY)
		: m_allocator(alloc), m_capacity(capacity)
	{
		// Allocate from the given allocator
		m_slots = m_allocator->Allocate<Slot>(m_capacity);

		for (size_t slot = 0; slot < m_capacity - 1; ++slot)
		{
			m_slots[slot].next_free = (uint32_t)slot + 1;
			m_slots[slot].gen = 0;
		}
	}

	~ResourceSlotmap()
	{
		if constexpr (!std::is_trivially_destructible_v<TResource>)
		{
			for (size_t i = 0; i < m_capacity; ++i)
			{
				Slot* slot = &m_slots[i];
				
				if (slot->next_free == SLOT_OCCUPIED)
				{
					slot->resource.~TResource();
				}
			}
		}
	}

	ResourceSlotmap(const ResourceSlotmap& other) = delete;
	ResourceSlotmap(ResourceSlotmap&& other) = delete;
	const ResourceSlotmap& operator=(const ResourceSlotmap& other) = delete;
	ResourceSlotmap&& operator=(ResourceSlotmap&& other) = delete;

	template<typename... TArgs>
	ResourceHandle Insert(TArgs&&... args)
	{
		ResourceHandle handle = AllocateSlot();

		Slot* slot = &m_slots[handle.index];
		new (&slot->resource) TResource((args)...);

		return handle;
	}

	void Remove(ResourceHandle handle)
	{
		Slot* sentinel = &m_slots[0];

		if (DX_RESOURCE_HANDLE_VALID(handle))
		{
			Slot* slot = &m_slots[handle.index];

			if (handle.version == slot->gen)
			{
				slot->gen++;

				slot->next_free = sentinel->next_free;
				sentinel->next_free = handle.index;

				if constexpr (!std::is_trivially_destructible_v<TResource>)
				{
					slot->resource.~TResource();
				}
			}
		}
	}

	TResource* Find(ResourceHandle handle)
	{
		TResource* resource = nullptr;

		if (DX_RESOURCE_HANDLE_VALID(handle))
		{
			Slot* slot = &m_slots[handle.index];
			if (handle.version == slot->gen)
			{
				resource = &slot->resource;
			}
		}

		return resource;
	}

private:
	ResourceHandle AllocateSlot()
	{
		ResourceHandle handle = {};
		Slot* sentinel = &m_slots[0];

		if (sentinel->next_free)
		{
			uint32_t index = sentinel->next_free;
			Slot* slot = &m_slots[index];

			sentinel->next_free = slot->next_free;
			slot->next_free = SLOT_OCCUPIED;

			handle.index = index;
			handle.version = slot->gen;
		}

		return handle;
	}

private:
	struct Slot
	{
		uint32_t next_free;
		uint32_t gen;

		TResource resource;
	};

	LinearAllocator* m_allocator;
	Slot* m_slots;
	size_t m_capacity;

};
