#include "ParallelFibHeap.h"

#ifdef _WIN32
#include <array>
#include <cstdint>

using HANDLE = void*;
using DWORD_PTR = std::uintptr_t;

extern "C" HANDLE __stdcall GetCurrentProcess();
extern "C" HANDLE __stdcall GetCurrentThread();
extern "C" int __stdcall SetProcessAffinityMask(HANDLE hProcess, DWORD_PTR dwProcessAffinityMask);
extern "C" DWORD_PTR __stdcall SetThreadAffinityMask(HANDLE hThread, DWORD_PTR dwThreadAffinityMask);
#endif

#include <unistd.h>

namespace parallel_affinity {

void restrict_process_to_efficiency_cores() {
#ifdef _WIN32
	constexpr std::array<int, 8> kEfficiencyLogicalProcessors = {12, 13, 14, 15, 16, 17, 18, 19};
	DWORD_PTR mask = 0;
	for (int cpu : kEfficiencyLogicalProcessors) {
		mask |= static_cast<DWORD_PTR>(1) << cpu;
	}

	SetProcessAffinityMask(GetCurrentProcess(), mask);
#endif
}

void pin_current_thread_to_efficiency_core() {
#ifdef _WIN32
	constexpr std::array<int, 8> kEfficiencyLogicalProcessors = {12, 13, 14, 15, 16, 17, 18, 19};
	const int thread_index = omp_get_thread_num();
	const int cpu = kEfficiencyLogicalProcessors[thread_index % static_cast<int>(kEfficiencyLogicalProcessors.size())];
	const DWORD_PTR mask = static_cast<DWORD_PTR>(1) << cpu;

	SetThreadAffinityMask(GetCurrentThread(), mask);
#endif
}

void restrict_process_to_performance_cores() {
#ifdef _WIN32
    constexpr std::array<int, 12> kPerformanceLogicalProcessors = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
    DWORD_PTR mask = 0;
    for (int cpu : kPerformanceLogicalProcessors) {
        mask |= static_cast<DWORD_PTR>(1) << cpu;
    }

    SetProcessAffinityMask(GetCurrentProcess(), mask);
#endif
}

void pin_current_thread_to_performance_core() {
#ifdef _WIN32
    constexpr std::array<int, 12> kPerformanceLogicalProcessors = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
    const int thread_index = omp_get_thread_num();
    const int cpu = kPerformanceLogicalProcessors[thread_index % static_cast<int>(kPerformanceLogicalProcessors.size())];
    const DWORD_PTR mask = static_cast<DWORD_PTR>(1) << cpu;

    SetThreadAffinityMask(GetCurrentThread(), mask);
#endif
}

}  // namespace parallel_affinity

