#pragma once

#include "core/api/item_conversion.hpp"

#include <filesystem>
#include <vector>

namespace enchantment_tweaks::pack_files {
namespace fs = std::filesystem;

/// Reads files from a ZIP or directory pack, preserving paths relative to its
/// root. Throws std::runtime_error if the pack cannot be read.
std::vector<conversion::OutputFile>
readPackFiles(const fs::path &pack);

} // namespace enchantment_tweaks::pack_files
