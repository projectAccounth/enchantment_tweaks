#pragma once

#include <iostream>

namespace enchantment_tweaks::app {

enum class ExitStatus { OK, ERROR, ABORTED };

inline std::string statusToString(const ExitStatus &status) {
  switch (status) {
  case ExitStatus::OK:
    return "OK";
  case ExitStatus::ERROR:
    return "ERROR";
  case ExitStatus::ABORTED:
    return "ABORTED";
  }
}

inline void exitProgram(const ExitStatus &status,
                        const std::string &exitMsg = "") {
  std::clog << "Program completed with status " << statusToString(status)
            << '\n';
  if (exitMsg.length() > 0) {
    std::clog << "Note: " << exitMsg << '\n';
    std::cout << exitMsg << '\n';
  }
  exit(0);
}

} // namespace enchantment_tweaks::app