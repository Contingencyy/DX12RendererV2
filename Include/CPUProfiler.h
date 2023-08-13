#pragma once

namespace CPUProfiler
{

#define CPU_PROFILER_MAX_CPU_TIMERS 64
#define CPU_PROFILER_GRAPH_HISTORY_LENGTH 10000

	void Init();
	void Exit();

	void StartTimer(const char* name);
	void EndTimer(const char* name);
	void Reset();

	void OnImGuiRender();

	struct ProfileScope
	{
		ProfileScope(const char* name)
			: name(name)
		{
			CPUProfiler::StartTimer(name);
		}

		~ProfileScope()
		{
			CPUProfiler::EndTimer(name);
		}

		const char* name;
	};

}

#define DX_PERF_SCOPE(name) CPUProfiler::ProfileScope __profile_scope(name)
#define DX_PERF_START(name) CPUProfiler::StartTimer(name)
#define DX_PERF_END(name) CPUProfiler::EndTimer(name)
