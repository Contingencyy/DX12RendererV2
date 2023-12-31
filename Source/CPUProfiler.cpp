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
			// NOTE: We use the thread allocator for timer allocations, otherwise we would need to do manual
			// cleanup, or keep a free-list of timers
			Timer* temp = (Timer*)g_thread_alloc.Allocate(sizeof(Timer), alignof(Timer));
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
		double min = DBL_MAX, max = DBL_MIN, avg_accumulator = 0.0, avg = 0.0;
		uint32_t min_index = 0, max_index = 0;
	};

	struct InternalData
	{
		LinearAllocator alloc;
		MemoryScope memory_scope;

		Hashmap<const char*, TimerStack>* timer_stacks = nullptr;
		int64_t timer_freq = 0;

		int32_t graph_data_size = 0;
		int32_t graph_current_data_index = 0;
		double* graph_xaxis_data = nullptr;
		float graph_history_length = DX_MIN(500, CPU_PROFILER_GRAPH_HISTORY_LENGTH);
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
		data.graph_data_size = DX_MIN(data.graph_data_size++, CPU_PROFILER_GRAPH_HISTORY_LENGTH);
		int32_t data_graph_next_index = (data.graph_current_data_index + 1) % CPU_PROFILER_GRAPH_HISTORY_LENGTH;

		for (uint32_t node_idx = 0; node_idx < data.timer_stacks->m_capacity; ++node_idx)
		{
			Hashmap<const char*, TimerStack>::Node* node = &data.timer_stacks->m_nodes[node_idx];

			if (node->key == Hashmap<const char*, TimerStack>::NODE_UNUSED)
			{
				continue;
			}

			TimerStack* stack = &node->value;
			double prev_value = stack->graph_data_buffer[data.graph_current_data_index];

			stack->graph_data_buffer[data.graph_current_data_index] = TimestampToMillis(stack->accumulator, data.timer_freq);
			stack->min = DX_MIN(stack->min, stack->graph_data_buffer[data.graph_current_data_index]);
			stack->max = DX_MAX(stack->max, stack->graph_data_buffer[data.graph_current_data_index]);
			stack->avg_accumulator -= prev_value;
			stack->avg_accumulator += stack->graph_data_buffer[data.graph_current_data_index];
			stack->avg = stack->avg_accumulator / (double)data.graph_data_size;
		}

		ImGui::Begin("CPU Profiler");

		// --------------------------------------------------------------------------------------------------------------------------
		// CPU Timer stats

		ImGui::SetNextItemOpen(true, ImGuiCond_Once);
		if (ImGui::CollapsingHeader("CPU Timer stats"))
		{
			// TODO: The CPU Profiler should be able to determine the current min and max values from the timer stack data buffers
			// instead of resetting them
			bool reset_min_max = false;
			reset_min_max = ImGui::Button("Reset Min/Max");

			if (ImGui::BeginTable("CPU Timer table", 4, ImGuiTableFlags_NoBordersInBody | ImGuiTableFlags_SizingFixedFit))
			{
				ImGui::TableNextColumn();
				ImGui::Text("Timer");
				ImGui::TableNextColumn();
				ImGui::Text("Min");
				ImGui::TableNextColumn();
				ImGui::Text("Avg");
				ImGui::TableNextColumn();
				ImGui::Text("Max");

				for (uint32_t node_idx = 0; node_idx < data.timer_stacks->m_capacity; ++node_idx)
				{
					Hashmap<const char*, TimerStack>::Node* node = &data.timer_stacks->m_nodes[node_idx];

					if (node->key == Hashmap<const char*, TimerStack>::NODE_UNUSED)
					{
						continue;
					}

					ImGui::TableNextRow();

					TimerStack* stack = &node->value;
					ImGui::TableNextColumn();
					ImGui::Text("%s", stack->name);
					ImGui::TableNextColumn();
					ImGui::Text("%.3f ms", stack->min);
					ImGui::TableNextColumn();
					ImGui::Text("%.3f ms", stack->avg);
					ImGui::TableNextColumn();
					ImGui::Text("%.3f ms", stack->max);

					if (reset_min_max)
					{
						stack->min = DBL_MAX;
						stack->max = DBL_MIN;
					}
				}

				ImGui::EndTable();
			}
		}

		// --------------------------------------------------------------------------------------------------------------------------
		// CPU Timer graph

		ImGui::SetNextItemOpen(true, ImGuiCond_Once);
		if (ImGui::CollapsingHeader("CPU Timer graph"))
		{
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
				// the entire map, which has a lot of empty space potentially. Or even better, keep pointers to timer stacks around for the current frame
				// in a linked list, so we do not need to hash again just to fetch data we could already have from before.
				for (uint32_t node_idx = 0; node_idx < data.timer_stacks->m_capacity; ++node_idx)
				{
					Hashmap<const char*, TimerStack>::Node* node = &data.timer_stacks->m_nodes[node_idx];

					if (node->key == Hashmap<const char*, TimerStack>::NODE_UNUSED)
					{
						continue;
					}

					TimerStack* stack = &node->value;

					// TODO: Triple buffer the timers properly, so that they match with the GPU timers we will add later
					ImPlot::SetNextFillStyle(IMPLOT_AUTO_COL, 0.3);
					ImPlot::PlotLine(stack->name, data.graph_xaxis_data, stack->graph_data_buffer, data.graph_data_size,
						ImPlotLineFlags_Shaded, data_graph_next_index - data.graph_data_size);
				}
				ImPlot::EndPlot();
			}
		}

		ImGui::End();

		data.graph_current_data_index = (data.graph_current_data_index + 1) % CPU_PROFILER_GRAPH_HISTORY_LENGTH;
	}

}
