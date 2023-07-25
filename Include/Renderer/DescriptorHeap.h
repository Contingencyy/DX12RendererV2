#pragma once

struct DescriptorAllocation
{
	D3D12_CPU_DESCRIPTOR_HANDLE cpu;
	D3D12_GPU_DESCRIPTOR_HANDLE gpu;

	uint32_t num_descriptors;
	uint32_t descriptor_heap_index;
	uint32_t descriptor_increment_size;

	D3D12_CPU_DESCRIPTOR_HANDLE GetCPUHandle(uint32_t offset = 0)
	{
		return { cpu.ptr + offset * descriptor_increment_size };
	}

	D3D12_GPU_DESCRIPTOR_HANDLE GetGPUHandle(uint32_t offset = 0)
	{
		return { gpu.ptr + offset * descriptor_increment_size };
	}
};

class DescriptorHeap
{
public:
	DescriptorHeap() = default;
	DescriptorHeap(LinearAllocator* allocator, D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t num_descriptors);

	DescriptorAllocation Allocate(uint32_t num_descriptors = 1);
	void Release(const DescriptorAllocation& alloc);

	ID3D12DescriptorHeap* GetD3D12DescriptorHeap() const { return m_d3d_descriptor_heap; }

private:
	struct DescriptorBlock
	{
		uint32_t descriptor_heap_index;
		uint32_t num_descriptors;

		DescriptorBlock* next;
	};

private:
	LinearAllocator* m_allocator;
	ID3D12DescriptorHeap* m_d3d_descriptor_heap;

	DescriptorBlock* m_blocks;
	DescriptorBlock* m_free_blocks;
	uint32_t m_num_descriptors;
	uint32_t m_descriptor_increment_size;

};
