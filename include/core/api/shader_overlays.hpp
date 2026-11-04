#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace enchantment_tweaks::conversion {
namespace fs = std::filesystem;

struct ShaderOverlay {
  std::string directory;
  int minFormat = 0;
  int maxFormat = 0;
  std::vector<std::string> files;
};

std::vector<ShaderOverlay> loadShaderOverlays(const fs::path &root);

} // namespace enchantment_tweaks::conversion
