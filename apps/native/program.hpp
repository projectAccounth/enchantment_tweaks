#pragma once

#include <cstdint>
#include <iostream>

namespace enchantment_tweaks::app {

enum class ExitStatus : uint8_t { Ok = 0, Error = 1, Aborted = 16 };

inline std::string statusToString(const ExitStatus &status) {
  switch (status) {
  case ExitStatus::Ok:
    return "OK";
  case ExitStatus::Error:
    return "ERROR";
  case ExitStatus::Aborted:
    return "ABORTED";
  }
}

class TerminationException : public std::runtime_error {
private:
  ExitStatus status;

public:
  TerminationException(const std::string &message, ExitStatus st)
      : std::runtime_error(message), status(st) {}

  int getExitCode() const { return static_cast<uint8_t>(status); }
};

[[noreturn]] inline void exitProgram(const ExitStatus &status,
                                     const std::string &exitMsg = "") {
  std::clog << "Program completed with status " << statusToString(status)
            << '\n';
  throw TerminationException(exitMsg, status);
}

} // namespace enchantment_tweaks::app
