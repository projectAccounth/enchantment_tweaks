#pragma once

#include <string>

namespace enchantment_tweaks::text {

/// Returns an ASCII lower-case copy of the text.
std::string asciiLower(std::string s);

/// Returns true for exactly six hexadecimal digits, the "rrggbb" colour form.
bool isHexColor(const std::string &s);

} // namespace enchantment_tweaks::text
