#include "core/project/game_versions.hpp"

#include "core/api/item_catalog.hpp"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

namespace enchantment_tweaks::project {

namespace fs = std::filesystem;

fs::path defaultMinecraftDirectory() {
#if defined(_WIN32)
  if (const char *appData = std::getenv("APPDATA"))
    return fs::path(appData) / ".minecraft";
  return {};
#elif defined(__APPLE__)
  if (const char *home = std::getenv("HOME"))
    return fs::path(home) / "Library" / "Application Support" / "minecraft";
  return {};
#else
  if (const char *home = std::getenv("HOME"))
    return fs::path(home) / ".minecraft";
  return {};
#endif
}

std::vector<InstalledGame> findInstalledGames(const fs::path &minecraftDir) {
  std::vector<InstalledGame> games;
  const auto versions = minecraftDir / "versions";
  std::error_code ec;
  if (!fs::is_directory(versions, ec)) return games;

  for (const auto &entry : fs::directory_iterator(versions, ec)) {
    if (!entry.is_directory(ec)) continue;
    const std::string id =
        entry.path().filename().string(); // we gettin functional
    const auto jar = entry.path() / (id + ".jar");
    if (fs::is_regular_file(jar, ec)) {
      games.push_back({.id = id, .jar = jar});
    }
  }
  std::ranges::sort(games, [](const auto &a, const auto &b) {
    return catalog::naturalLess(b.id, a.id); // newest first
  });
  return games;
}

} // namespace enchantment_tweaks::project
