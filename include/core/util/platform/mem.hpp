#pragma once

#include <cstdio>
#include <cstdlib>
// clang-format off
#if defined(_WIN32) || defined(_WIN64)
#define ET_PLATFORM_WINDOWS
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <psapi.h>
#if defined(_MSC_VER)
#pragma comment(lib, "psapi.lib")
#endif
// clang-format on
#elif defined(__APPLE__) && defined(__MACH__)
#define ET_PLATFORM_MACOS
#include <mach/mach.h>

#elif defined(__linux__) || defined(__linux) || defined(__unix__)
#define ET_PLATFORM_LINUX
#include <sys/resource.h>
#include <unistd.h>
#else
#define ET_PLATFORM_UNKNOWN
#endif

namespace enchantment_tweaks::util {
inline static size_t peakMem() {
#if defined(ET_PLATFORM_WINDOWS)
  PROCESS_MEMORY_COUNTERS pmc;
  if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
    return static_cast<size_t>(pmc.PeakWorkingSetSize); // bytes
  }
  return 0;

#elif defined(ET_PLATFORM_MACOS) || defined(ET_PLATFORM_LINUX)
  struct rusage usage;
  if (getrusage(RUSAGE_SELF, &usage) == 0) {
#if defined(ET_PLATFORM_MACOS)

    // macOS returns #bytes
    return static_cast<size_t>(usage.ru_maxrss);
#else
    // Linux returns kBs
    return static_cast<size_t>(usage.ru_maxrss) * 1024;
#endif
  }
  return 0;

#else
  return 0;
#endif
}

// Returns the current RSS of the program, in bytes
inline static size_t currentMem() {
#if defined(ET_PLATFORM_WINDOWS)
  PROCESS_MEMORY_COUNTERS pmc;
  if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
    return static_cast<size_t>(pmc.WorkingSetSize);
  }
  return 0;

#elif defined(ET_PLATFORM_MACOS)
  mach_task_basic_info info;
  mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;

  if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO,
                reinterpret_cast<task_info_t>(&info), &count) == KERN_SUCCESS) {
    return static_cast<size_t>(info.resident_size);
  }
  return 0;

#elif defined(ET_PLATFORM_LINUX)
  FILE *file = std::fopen("/proc/self/statm", "r");
  if (file) {
    size_t totalPages, residentPages;
    if (std::fscanf(file, "%zu %zu", &totalPages, &residentPages) == 2) {
      (void)std::fclose(file);
      return residentPages * static_cast<size_t>(sysconf(_SC_PAGESIZE));
    }
    (void)std::fclose(file);
  }
  return 0;

#else
  return 0;
#endif
}

} // namespace enchantment_tweaks::util
