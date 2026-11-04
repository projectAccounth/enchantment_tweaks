#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace enchantment_tweaks::file_io {
namespace fs = std::filesystem;


[[nodiscard]] std::optional<std::string>
readText(const fs::path &path);

[[nodiscard]] std::optional<std::vector<uint8_t>>
readBytes(const fs::path &path);

} // namespace enchantment_tweaks::file_io
