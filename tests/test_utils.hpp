#pragma once

#include "core/util/debug.hpp"
#include "libraries/nlohmann/json.hpp"

#include <chrono>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <stdlib.h>
#include <string>
#include <utility>

#if defined(_WIN32) || defined(_WIN64)
#define PLATFORM_WINDOWS
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <psapi.h>
#pragma comment(lib, "psapi.lib")

#elif defined(__APPLE__) && defined(__MACH__)
#define PLATFORM_MACOS
#include <mach/mach.h>

#elif defined(__linux__) || defined(__linux) || defined(__unix__)
#define PLATFORM_LINUX
#include <fstream>
#include <sys/resource.h>
#include <unistd.h>
#else
#define PLATFORM_UNKNOWN
#endif

#ifndef TEST_FIXTURE_DIR
#define TEST_FIXTURE_DIR "tests"
#endif

namespace test_util {

inline static std::chrono::time_point TEST_BEGIN =
                                          std::chrono::steady_clock::now(),
                                      TEST_END =
                                          std::chrono::steady_clock::now();

inline static size_t peakMem() {
#if defined(PLATFORM_WINDOWS)
  PROCESS_MEMORY_COUNTERS pmc;
  if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
    return static_cast<size_t>(pmc.PeakWorkingSetSize); // bytes
  }
  return 0;

#elif defined(PLATFORM_MACOS) || defined(PLATFORM_LINUX)
  struct rusage usage;
  if (getrusage(RUSAGE_SELF, &usage) == 0) {
#if defined(PLATFORM_MACOS)

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

using nlohmann::json;
using namespace enchantment_tweaks;

// what should be tracked there, really?
struct TestDiag {
  size_t usedMemory;
  long long execTimeMs;
};

inline json readJson(const std::filesystem::path &path) {
  std::ifstream input(path);
  if (!input) {
    throw std::runtime_error("Unable to open fixture: " + path.string());
  }
  return json::parse(input);
}

inline void summary(const TestDiag &tst) {
  debug::Logger::info() << "total test time: " << tst.execTimeMs << "ms";
  debug::Logger::info() << "peak memory usage: " << tst.usedMemory / 1024.0
                        << "KB";
}

inline void begin() {
  TEST_BEGIN = std::chrono::steady_clock::now();
  debug::Logger::setLevel(debug::Logger::Level::Debug);
  //
}

inline void end() {
  TestDiag data;
  TEST_END = std::chrono::steady_clock::now();

  data.usedMemory = peakMem();
  data.execTimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                        TEST_END - TEST_BEGIN)
                        .count();
  summary(data);
}

inline void check(bool condition, const std::string &message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

inline void checkJsonEqual(const json &expected, const json &actual,
                           const std::string &context) {
  if (expected == actual) {
    return;
  }

  const json patch = json::diff(expected, actual);
  throw std::runtime_error("JSON mismatch: " + context +
                           "\ndiff (expected -> actual):\n" + patch.dump(2) +
                           "\nExpected JSON:\n" + expected.dump(2) +
                           "\nActual JSON:\n" + actual.dump(2));
}

void fail(const std::string &message) {
  debug::Logger::info() << "[fail] " << message << '\n';
}

struct AbortException : public std::exception {
private:
  std::string msg_;

public:
  explicit AbortException(std::string reason) : msg_(std::move(reason)){};
  const char *what() const noexcept override { return msg_.c_str(); };

  ~AbortException() override = default;
};

template <typename Test> bool runCase(const std::string &name, Test &&test) {
  try {
    std::forward<Test>(test)();
    debug::Logger::info() << "[pass] " << name << '\n';
    return true;
  } catch (const AbortException &e) {
    debug::Logger::warning() << "[aborted] " << name << ": " << e.what();
    return true;
  } catch (const std::exception &error) {
    debug::Logger::error() << "[fail] " << name << ": " << error.what();
    return false;
  }
}

// Runs named cases and reports "passed/total" at the end. Each test file uses
// one Suite:
//   test_util::Suite suite;
//   suite.run("name", [] { test_util::check(...); });
//   return suite.finish();
class Suite {
public:
  Suite() { begin(); }

  template <typename Fn> void run(const std::string &name, Fn &&fn) {
    ++total_;
    if (runCase(name, std::forward<Fn>(fn))) ++passed_;
  }

  // Prints the summary and returns the process exit code.
  int finish() {
    debug::Logger::info() << "[summary] " << passed_ << "/" << total_
                          << " tests passed\n";
    end();
    return passed_ == total_ ? 0 : 1;
  }

private:
  size_t passed_ = 0;
  size_t total_ = 0;
};

} // namespace test_util
