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

template <class T> inline void hash_combine(std::size_t &seed, const T &v) {
  std::hash<T> hasher;
  seed ^= hasher(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

template <typename... Args> std::size_t hash_multiple(const Args &...args) {
  std::size_t seed = 0;
  (hash_combine(seed, args), ...);
  return seed;
}

} // namespace util