#include "Pch.h"
#include "CPUProfiler.h"
#include "Containers/Hashmap.h"

#include "imgui/imgui.h"
#include "implot/implot.h"

namespace CPUProfiler
{

	static int64_t GetTimestampCurrent()
	{
		LARGE_INTEGER result;
		QueryPerformanceCounter(&result);
		return result.QuadPart;
	}

	static double TimestampToMillis(int64_t timestamp, int64_t freq)
	{
		int64_t temp = timestamp * 1000000;
		temp /= freq;

		return (double)temp / 1000;
	}

	struct Timer
	{
		int64_t start;
		int64_t end;

		Timer* next;
	};

	struct TimerStack
	{
		TimerStack(const char* name)
			: name(name)
		{
			// NOTE: We can use the thread allocator to allocate these timers
			head = (Timer*)g_thread_alloc.Allocate(sizeof(Timer), alignof(Timer));
			head->next = nullptr;
		}

		// Pushes a new timer to the linked list head, and returns it
		Timer* PushTimer()
		{
			Timer* temp = (Timer*)alloc.Allocate(sizeof(Timer), alignof(Timer));
			temp->next = head;
			head = temp;

			return temp;
		}

		// Pops a timer from the linked list head, and returns it (Last In First Out)
		Timer* PopTimer()
		{
			Timer* temp = head;
			head = head->next;

			return temp;
		}

		LinearAllocator alloc;
		Timer* head = nullptr;
		const char* name = nullptr;
		int64_t accumulator = 0;
	};

	struct InternalData
	{
		LinearAllocator alloc;
		MemoryScope memory_scope;

		Hashmap<const char*, TimerStack>* timer_stacks;
		int64_t timer_freq;
	} static data;

	void Init()
	{
		data.memory_scope = MemoryScope(&data.alloc, data.alloc.at_ptr);
		data.timer_stacks = data.memory_scope.New<Hashmap<const char*, TimerStack>>(&data.memory_scope, MAX_UNIQUE_CPU_TIMERS);
		
		LARGE_INTEGER timer_freq;
		QueryPerformanceFrequency(&timer_freq);
		data.timer_freq = timer_freq.QuadPart;
	}

	void Exit()
	{
		data.memory_scope.~MemoryScope();
	}

	void StartTimer(const char* name)
	{
		// Get a node from the hashmap, inserting a new one if no timer stack with the name exists
		TimerStack* stack = data.timer_stacks->Find(name);
		if (!stack)
		{
			stack = data.timer_stacks->Insert(name, TimerStack(name));
		}

		// Push a timer on top of the timer stack, and update its starting timestamp
		Timer* timer = stack->PushTimer();
		timer->start = GetTimestampCurrent();
	}

	void EndTimer(const char* name)
	{
		// Get a node from the hashmap
		TimerStack* stack = data.timer_stacks->Find(name);
		if (stack)
		{
			Timer* timer = stack->PopTimer();
			timer->end = GetTimestampCurrent();
			stack->accumulator = timer->end - timer->start;
		}
	}

	void Reset()
	{
		data.timer_stacks->Reset();
	}

	void OnImGuiRender()
	{
		ImGui::Begin("CPU Profiler");

		// TODO: If we cache the used cpu profiling names for this frame, we can simply fetch all of them from the hashmap
		for (uint32_t node_idx = 0; node_idx < data.timer_stacks->m_capacity; ++node_idx)
		{
			Hashmap<const char*, TimerStack>::Node* node = &data.timer_stacks->m_nodes[node_idx];
			
			if (node->key == Hashmap<const char*, TimerStack>::NODE_UNUSED)
			{
				continue;
			}

			TimerStack* stack = &node->value;
			ImGui::Text("%s: %f ms", stack->name, TimestampToMillis(stack->accumulator, data.timer_freq));
		}

		/*if (ImPlot::BeginPlot("", ImVec2(-1, -1), ImPlotFlags_Crosshairs | ImPlotFlags_NoMouseText))
		{
			
			
			ImPlot::EndPlot();
		}*/

		ImGui::End();
	}

}
