#include "core/api/item_groups.hpp"

#include "core/util/text.hpp"
#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

namespace enchantment_tweaks::catalog {

namespace {

// Longest first, so "Dark Oak" wins over "Oak" and "Pickaxe" over "Axe".
std::vector<std::string> longestFirst(std::vector<std::string> words) {
  std::ranges::stable_sort(words,
                           [](const std::string &a, const std::string &b) {
                             return a.size() > b.size();
                           });
  return words;
}

} // namespace

GroupMode groupModeFromString(const std::string &text) {
  if (text == "prefix") return GroupMode::Prefix;
  if (text == "suffix") return GroupMode::Suffix;
  return GroupMode::None;
}

std::string groupModeToString(GroupMode mode) {
  switch (mode) {
  case GroupMode::Prefix:
    return "prefix";
  case GroupMode::Suffix:
    return "suffix";
  case GroupMode::None:
    break;
  }
  return "none";
}

const std::vector<std::string> &prefixKeywords() {
  static const std::vector<std::string> words = longestFirst(
      {"Wooden",    "Stone",    "Iron",       "Diamond",   "Golden",
       "Netherite", "Copper",   "Leather",    "Chainmail", "Turtle",
       "Acacia",    "Oak",      "Spruce",     "Birch",     "Jungle",
       "Dark Oak",  "Mangrove", "Cherry",     "Bamboo",    "Crimson",
       "Warped",    "Glass",    "Music Disc", "Spawn Egg", "Pottery Sherd",
       "Banner",    "Candle",   "Terracotta", "Concrete",  "Wool",
       "Carpet"});
  return words;
}

const std::vector<std::string> &suffixKeywords() {
  static const std::vector<std::string> words = longestFirst(
      {"Sword",  "Pickaxe",    "Axe",      "Hoe",       "Shovel",
       "Helmet", "Chestplate", "Leggings", "Boots",     "Glass Pane",
       "Pane",   "Sapling",    "Trapdoor", "Door",      "Fence Gate",
       "Fence",  "Sign",       "Boat",     "Button",    "Pressure Plate",
       "Slab",   "Stairs",     "Wall",     "Spawn Egg", "Horse Armor",
       "Shield", "Bow",        "Crossbow", "Trident",   "Bucket"});
  return words;
}

std::string groupLabelFor(const std::string &displayName, GroupMode mode) {
  if (mode == GroupMode::None) return {};
  const std::string name = text::asciiLower(displayName);
  if (mode == GroupMode::Prefix) {
    for (const auto &word : prefixKeywords()) {
      const std::string w = text::asciiLower(word);
      if (name.size() >= w.size() && name.compare(0, w.size(), w) == 0 &&
          (name.size() == w.size() || name[w.size()] == ' ')) {
        return word;
      }
    }
  } else {
    for (const auto &word : suffixKeywords()) {
      const std::string w = text::asciiLower(word);
      if (name.size() >= w.size() && name.ends_with(w) &&
          (name.size() == w.size() ||
           name[name.size() - w.size() - 1] == ' ')) {
        return word;
      }
    }
  }
  return "Other";
}

} // namespace enchantment_tweaks::catalog
