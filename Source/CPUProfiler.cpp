#include "Pch.h"
#include "CPUProfiler.h"
#include "Containers/Hashmap.h"
#include "Renderer/D3DState.h"

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
		TimerStack(MemoryScope* mem_scope, const char* name)
			: name(name)
		{
			graph_data_buffer = mem_scope->Allocate<double>(CPU_PROFILER_GRAPH_HISTORY_LENGTH);
		}

		// Pushes a new timer to the linked list head, and returns it
		Timer* PushTimer()
		{
			// NOTE: We can use the thread allocator to allocate these timers
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
		double* graph_data_buffer = nullptr;
	};

	struct InternalData
	{
		LinearAllocator alloc;
		MemoryScope memory_scope;

		Hashmap<const char*, TimerStack>* timer_stacks = nullptr;
		int64_t timer_freq = 0;

		int graph_data_size = 0;
		int graph_current_data_index = 0;
		double* graph_xaxis_data = nullptr;
		float graph_history_length = CPU_PROFILER_GRAPH_HISTORY_LENGTH;
	} static data;

	void Init()
	{
		data.memory_scope = MemoryScope(&data.alloc, data.alloc.at_ptr);
		data.timer_stacks = data.memory_scope.New<Hashmap<const char*, TimerStack>>(&data.memory_scope, CPU_PROFILER_MAX_CPU_TIMERS);
		
		LARGE_INTEGER timer_freq;
		QueryPerformanceFrequency(&timer_freq);
		data.timer_freq = timer_freq.QuadPart;

		data.graph_xaxis_data = data.memory_scope.Allocate<double>(CPU_PROFILER_GRAPH_HISTORY_LENGTH);
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
			stack = data.timer_stacks->Insert(name, TimerStack(&data.memory_scope, name));
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
		data.graph_xaxis_data[data.graph_current_data_index] = (double)d3d_state.frame_index;
		data.graph_data_size = DX_MIN(++data.graph_data_size, CPU_PROFILER_GRAPH_HISTORY_LENGTH);
		int data_graph_next_index = (data.graph_current_data_index + CPU_PROFILER_GRAPH_HISTORY_LENGTH + 1) % CPU_PROFILER_GRAPH_HISTORY_LENGTH;

		ImGui::Begin("CPU Profiler");

		ImGui::SliderFloat("Graph history length", &data.graph_history_length, 10.0, CPU_PROFILER_GRAPH_HISTORY_LENGTH, "%.f", ImGuiSliderFlags_AlwaysClamp);

		if (ImPlot::BeginPlot("CPU Timers", ImVec2(-1, -1), ImPlotFlags_Crosshairs | ImPlotFlags_NoMouseText))
		{
			ImPlot::SetupAxisFormat(ImAxis_X1, "%.0f");
			ImPlot::SetupAxis(ImAxis_X1, "Frame index", ImPlotAxisFlags_RangeFit | ImPlotAxisFlags_Foreground);
			ImPlot::SetupAxisLimits(ImAxis_X1, data.graph_xaxis_data[data.graph_current_data_index] - data.graph_history_length,
				data.graph_xaxis_data[data.graph_current_data_index], ImPlotCond_Always);
			
			ImPlot::SetupAxisFormat(ImAxis_Y1, "%.3f ms");
			ImPlot::SetupAxis(ImAxis_Y1, "Timers", ImPlotAxisFlags_AutoFit | ImPlotAxisFlags_RangeFit | ImPlotAxisFlags_Foreground);

			// TODO: If we cache the used cpu profiling names for this frame, we can simply fetch all of them from the hashmap instead of traversing
			// the entire map, which has a lot of empty space potentially.
			for (uint32_t node_idx = 0; node_idx < data.timer_stacks->m_capacity; ++node_idx)
			{
				Hashmap<const char*, TimerStack>::Node* node = &data.timer_stacks->m_nodes[node_idx];
			
				if (node->key == Hashmap<const char*, TimerStack>::NODE_UNUSED)
				{
					continue;
				}

				TimerStack* stack = &node->value;
				stack->graph_data_buffer[data.graph_current_data_index] = TimestampToMillis(stack->accumulator, data.timer_freq);

				// TODO: Triple buffer the timers properly, so that they match with the GPU timers we will add later
				ImPlot::SetNextFillStyle(IMPLOT_AUTO_COL, 0.3);
				ImPlot::PlotLine(stack->name, data.graph_xaxis_data, stack->graph_data_buffer, data.graph_data_size,
					ImPlotLineFlags_None, data_graph_next_index);
				ImPlot::SetNextFillStyle(IMPLOT_AUTO_COL, 0.3);
				ImPlot::PlotShaded(stack->name, data.graph_xaxis_data, stack->graph_data_buffer, data.graph_data_size,
					0.0, ImPlotShadedFlags_None, data_graph_next_index);
				//ImGui::Text("%s: %.3f ms", stack->name, TimestampToMillis(stack->accumulator, data.timer_freq));
			}

			ImPlot::EndPlot();
		}

		ImGui::End();

		data.graph_current_data_index = (data.graph_current_data_index + 1) % CPU_PROFILER_GRAPH_HISTORY_LENGTH;
	}

}
