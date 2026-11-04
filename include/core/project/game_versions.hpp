#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace enchantment_tweaks::project {
namespace fs = std::filesystem;

struct InstalledGame {
  std::string id; // e.g. "1.21.4"
  fs::path jar;   // versions/<id>/<id>.jar
};

fs::path defaultMinecraftDirectory();

/// Lists installed versions newest first.
std::vector<InstalledGame> findInstalledGames(const fs::path &minecraftDir);

} // namespace enchantment_tweaks::project
