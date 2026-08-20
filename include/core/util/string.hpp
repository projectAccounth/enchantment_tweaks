#pragma once

#include <string>
#include <vector>

namespace util {

inline std::vector<std::string> split(const std::string &str,
                                      const std::string &delimiter) {
  std::vector<std::string> tokens;
  size_t prev = 0, pos = 0;

  while ((pos = str.find(delimiter, prev)) != std::string::npos) {
    tokens.push_back(str.substr(prev, pos - prev));
    prev = pos + delimiter.length();
  }

  tokens.push_back(str.substr(prev));
  return tokens;
}

} // namespace util