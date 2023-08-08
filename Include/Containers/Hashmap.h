#pragma once

#define DX_HASHMAP_DEFAULT_CAPACITY 1024

template<typename TKey, typename TValue>
class Hashmap
{
public:
    static constexpr TKey NODE_UNUSED = 0;

public:
    Hashmap(MemoryScope* memory_scope, size_t capacity = DX_HASHMAP_DEFAULT_CAPACITY)
        : m_memory_scope(memory_scope), m_capacity(capacity), m_size(0)
    {
        m_nodes = m_memory_scope->Allocate<Node>(m_capacity);

        for (uint32_t i = 0; i < m_capacity; ++i)
        {
            m_nodes[i].key = NODE_UNUSED;
        }
    }

    Hashmap(const Hashmap& other) = delete;
    Hashmap(Hashmap&& other) = delete;
    const Hashmap& operator=(const Hashmap& other) = delete;
    Hashmap&& operator=(Hashmap&& other) = delete;

    void Insert(TKey key, TValue value)
    {
        // TODO: Automatically grow hashmap?
        DX_ASSERT(m_size < m_capacity && "Failed to insert key value pair into hashmap");

        Node temp = {
            .key = key,
            .value = value
        };
        uint32_t node_index = HashNodeIndex(key);

        while (m_nodes[node_index].key != key &&
            m_nodes[node_index].key != NODE_UNUSED)
        {
            node_index++;
            node_index %= m_capacity;
        }

        m_nodes[node_index] = temp;
        m_size++;
    }

    void Remove(TKey key)
    {
        uint32_t node_index = HashNodeIndex(key);
        uint32_t counter = 0;

        while (m_nodes[node_index].key != NODE_UNUSED || counter <= m_capacity)
        {
            if (m_nodes[node_index].key == key)
            {
                Node* node = &m_nodes[node_index];

                if constexpr (!std::is_trivially_destructible_v<TValue>)
                {
                    node->value.~TValue();
                }

                node->key = NODE_UNUSED;
                node->value = {};

                m_size--;
                break;
            }

            counter++;
            node_index++;
            node_index %= m_capacity;
        }
    }

    TValue* Find(TKey key)
    {
        TValue* value = nullptr;
        uint32_t node_index = HashNodeIndex(key);
        uint32_t counter = 0;

        while (m_nodes[node_index].key != NODE_UNUSED || counter <= m_capacity)
        {
            if (m_nodes[node_index].key == key)
            {
                value = &m_nodes[node_index].value;
                break;
            }

            counter++;
            node_index++;
            node_index %= m_capacity;
        }

        return value;
    }

    void Reset()
    {
        for (uint32_t i = 0; i < m_capacity; ++i)
        {
            if (m_nodes[i].key != NODE_UNUSED)
            {
                if constexpr (!std::is_trivially_destructible_v<TValue>)
                {
                    m_nodes[i].value.~TValue();
                }
            }

            m_nodes[i].key = NODE_UNUSED;
        }
    }

private:
    uint32_t HashNodeIndex(TKey key)
    {
        return Hash::DJB2(&key) % m_capacity;
    }

public:
    struct Node
    {
        TKey key;
        TValue value;
    };

    MemoryScope* m_memory_scope;
    size_t m_capacity;
    size_t m_size;

    Node* m_nodes;

};
