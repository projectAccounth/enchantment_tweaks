#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace enchantment_tweaks::catalog {

enum class GroupMode : uint8_t { None, Prefix, Suffix };

GroupMode
groupModeFromString(const std::string &text);  // unknown text gives None
std::string groupModeToString(GroupMode mode); // "none", "prefix", "suffix"
std::string groupLabelFor(const std::string &displayName, GroupMode mode);

const std::vector<std::string> &prefixKeywords();
const std::vector<std::string> &suffixKeywords();

} // namespace enchantment_tweaks::catalog
