#pragma once

#include <cstdint>
#include <cstdlib>
#include <functional>
#include <string>
#include <vector>

namespace enchantment_tweaks::util {

uint64_t fnv1a(const std::vector<uint8_t> &bytes) {
  uint64_t h = 0xcbf29ce484222325ULL;
  for (uint8_t b : bytes) {
    h ^= b;
    h *= 0x100000001b3ULL;
  }
  return h;
}

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

template <class T> inline void hashCombine(size_t &seed, const T &v) {
  std::hash<T> hasher;
  seed ^= hasher(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

template <typename... Args> size_t hashMultiple(const Args &...args) {
  size_t seed = 0;
  (hashCombine(seed, args), ...);
  return seed;
}

} // namespace enchantment_tweaks::util
