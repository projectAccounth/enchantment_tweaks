#pragma once

#include "core/util/debug.hpp"
#include "core/util/platform/mem.hpp"
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

#ifndef TEST_FIXTURE_DIR
#define TEST_FIXTURE_DIR "tests"
#endif

namespace test_util {

inline static std::chrono::time_point TEST_BEGIN =
                                          std::chrono::steady_clock::now(),
                                      TEST_END =
                                          std::chrono::steady_clock::now();

inline static size_t peakMem() { return enchantment_tweaks::util::peakMem(); }

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
  explicit AbortException(std::string reason) : msg_(std::move(reason)) {};
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
