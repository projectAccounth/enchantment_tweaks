#pragma once

#include <filesystem>

//-------------------
// global constants
//-------------------

namespace enchantment_tweaks::app {
namespace fs = std::filesystem;

inline const fs::path ASSETS = fs::current_path() / "assets";
} // namespace enchantment_tweaks::app