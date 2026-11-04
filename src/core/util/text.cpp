#include "core/util/text.hpp"

#include <algorithm>
#include <cctype>
#include <string>
#include <utility>

namespace enchantment_tweaks::text {

std::string asciiLower(std::string s) {
  std::ranges::transform(s, s.begin(), [](const unsigned char &c) {
    return static_cast<char>(std::tolower(c));
  });
  return s;
}

bool isHexColor(const std::string &s) {
  if (s.size() != 6) return false;
  return std::ranges::all_of(
      s, [](const unsigned char &c) { return std::isxdigit(c) != 0; });
}


} // namespace enchantment_tweaks::text
