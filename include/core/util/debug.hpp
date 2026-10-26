#pragma once

#include <chrono>
#include <cstdint>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>

namespace enchantment_tweaks::debug {

inline void trim(std::string &s) {
  const std::string whitespace = " \t\n\r\f\v";
  size_t end = s.find_last_not_of(whitespace);

  if (end != std::string::npos) {
    s.erase(end + 1);
  } else {
    s.clear();
  }
}
#include <chrono>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>

class Logger {
public:
  enum class Level : uint8_t {
    Debug = 0,
    Info,
    Warning,
    Error,
    None // disables all
  };

  Logger(Level level) : level_(level) {}

  template <typename T> Logger &operator<<(const T &value) {
    if (shouldLog()) stream_ << value;
    return *this;
  }

  Logger &operator<<(std::ostream &(*manip)(std::ostream &)) {
    if (shouldLog()) manip(stream_);
    return *this;
  }

  ~Logger() {
    if (shouldLog()) {
      std::lock_guard<std::mutex> lock(getMutex());
      auto s = stream_.str();
      trim(s);

      const std::string timestamp = getTimestamp();
      const std::string lvlStr = levelToString(level_);

      // msg w/ ANSI color
      const std::string colorStart = levelToColor(level_);
      const std::string colorEnd = colorStart.empty() ? "" : "\x1b[0m";
      const std::string consoleMessage = "[" + timestamp + "] [" + colorStart +
                                         lvlStr + colorEnd + "] " + s + "\n";

      std::clog << consoleMessage;

      if (auto &f = getFileStream(); f && f->is_open()) {
        const std::string fileMessage =
            "[" + timestamp + "] [" + lvlStr + "] " + s + "\n";
        (*f) << fileMessage;
      }
    }
  }

  static Logger debug() { return {Level::Debug}; }
  static Logger info() { return {Level::Info}; }
  static Logger warning() { return {Level::Warning}; }
  static Logger error() { return {Level::Error}; }

  static void setLevel(Level level) {
    std::lock_guard<std::mutex> lock(getMutex());
    getGlobalLevel() = level;
  }

  static Level getLevel() {
    std::lock_guard<std::mutex> lock(getMutex());
    return getGlobalLevel();
  }

  static bool setLogFile(const std::string &path, bool append = false) {
    std::lock_guard<std::mutex> lock(getMutex());
    auto &f = getFileStream();
    f = std::make_unique<std::ofstream>(path, append ? std::ios::app
                                                     : std::ios::trunc);
    return f->is_open();
  }

  static void closeLogFile() {
    std::lock_guard<std::mutex> lock(getMutex());
    getFileStream().reset();
  }

private:
  std::ostringstream stream_;
  Level level_;

  static const char *levelToString(Level lvl) {
    switch (lvl) {
    case Level::Debug:
      return "debug";
    case Level::Info:
      return "info";
    case Level::Warning:
      return "warning";
    case Level::Error:
      return "error";
    default:
      return "";
    }
  }

  static const char *levelToColor(Level lvl) {
    switch (lvl) {
    case Level::Debug:
      return "\x1b[36m"; // Cyan
    case Level::Info:
      return "\x1b[32m"; // Green
    case Level::Warning:
      return "\x1b[33m"; // Yellow
    case Level::Error:
      return "\x1b[31m"; // Red
    default:
      return "";
    }
  }

  bool shouldLog() const { return level_ >= getGlobalLevel(); }

  static Level &getGlobalLevel() {
    static Level currentLevel = Level::Debug;
    return currentLevel;
  }

  static std::unique_ptr<std::ofstream> &getFileStream() {
    static std::unique_ptr<std::ofstream> file;
    return file;
  }

  static std::mutex &getMutex() {
    static std::mutex m;
    return m;
  }

  static std::string getTimestamp() {
    using namespace std::chrono;
    auto now = system_clock::now();
    auto time = system_clock::to_time_t(now);
    auto ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;

    std::tm localTime{};
#ifdef _WIN32
    (void)localtime_s(&localTime, &time);
#else
    (void)localtime_r(&time, &localTime);
#endif

    std::ostringstream oss;
    oss << std::put_time(&localTime, "%Y-%m-%d %H:%M:%S") << '.'
        << std::setfill('0') << std::setw(3) << ms.count();
    return oss.str();
  }
};

} // namespace enchantment_tweaks::debug
