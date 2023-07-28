#pragma once

template<typename TKey, typename TValue>
class Hashmap
{
public:
    static constexpr const char* SLOT_UNUSED = "";

public:
    Hashmap(MemoryScope* alloc, size_t capacity = 1024)
        : m_allocator(alloc), m_capacity(capacity), m_size(0)
    {
        m_slots = m_allocator->Allocate<Node>(m_capacity);

        for (uint32_t i = 0; i < m_capacity; ++i)
        {
            m_slots[i].key = SLOT_UNUSED;
        }
    }

    Hashmap(const Hashmap& other) = delete;
    Hashmap(Hashmap&& other) = delete;
    const Hashmap& operator=(const Hashmap& other) = delete;
    Hashmap&& operator=(Hashmap&& other) = delete;

    void Insert(const char* key, TValue value)
    {
        // TODO: Automatically grow hashmap?
        DX_ASSERT(m_size < m_capacity && "Failed to insert key value pair into hashmap");

        Node temp = {
            .key = key,
            .value = value
        };
        uint32_t slot_index = HashSlotIndex(key);

        while (m_slots[slot_index].key != key &&
            m_slots[slot_index].key != SLOT_UNUSED)
        {
            slot_index++;
            slot_index %= m_capacity;
        }

        m_slots[slot_index] = temp;
        m_size++;
    }

    void Remove(const char* key)
    {
        uint32_t slot_index = HashSlotIndex(key);

        while (m_slots[slot_index].key != SLOT_UNUSED)
        {
            if (m_slots[slot_index].key == key)
            {
                Node* slot = &m_slots[slot_index];

                if constexpr (!std::is_trivially_destructible_v<TValue>)
                {
                    slot->value.~TValue();
                }

                slot->key = SLOT_UNUSED;
                slot->value = {};

                m_size--;
                break;
            }

            slot_index++;
            slot_index %= m_capacity;
        }
    }

    TValue* Find(const char* key)
    {
        TValue* value = nullptr;
        uint32_t slot_index = HashSlotIndex(key);
        uint32_t counter = 0;

        while (m_slots[slot_index].key != SLOT_UNUSED)
        {
            if (counter++ > m_capacity)
            {
                break;
            }

            if (m_slots[slot_index].key == key)
            {
                value = &m_slots[slot_index].value;
                break;
            }

            slot_index++;
            slot_index %= m_capacity;
        }

        return value;
    }

private:
    uint32_t HashSlotIndex(const char* key)
    {
        return Hash::DJB2(key, strlen(key)) % m_capacity;
    }

private:
    struct Node
    {
        TKey key;
        TValue value;
    };

    MemoryScope* m_allocator;
    size_t m_capacity;
    size_t m_size;

    Node* m_slots;

};
