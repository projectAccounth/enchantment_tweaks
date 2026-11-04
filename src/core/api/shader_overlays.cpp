#include "core/api/shader_overlays.hpp"

#include "core/types/common.hpp"
#include "core/util/file_io.hpp"

#include <filesystem>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

namespace enchantment_tweaks::conversion {

std::vector<ShaderOverlay>
loadShaderOverlays(const fs::path &root) {
  const auto text = file_io::readText(root / "manifest.json");
  if (!text)
    throw std::runtime_error("shader manifest not found: " +
                             (root / "manifest.json").string());
  const json j = json::parse(*text, nullptr, /*allow_exceptions=*/false);
  if (!j.is_object() || !j.contains("overlays") || !j["overlays"].is_array()) {
    throw std::runtime_error("shader manifest has no overlays list");
  }
  std::vector<ShaderOverlay> out;
  for (const auto &entry : j["overlays"]) {
    ShaderOverlay o;
    o.directory = entry.value("directory", std::string());
    o.minFormat = entry.value("minFormat", 0);
    o.maxFormat = entry.value("maxFormat", 0);
    if (o.directory.empty() || o.directory.find("..") != std::string::npos ||
        o.maxFormat < o.minFormat) {
      throw std::runtime_error("shader manifest: bad overlay entry");
    }
    for (const auto &file : entry.value("files", json::array())) {
      if (!file.is_string())
        throw std::runtime_error("shader manifest: file name is not a string");
      o.files.push_back(file.get<std::string>());
    }
    out.push_back(std::move(o));
  }
  return out;
}

} // namespace enchantment_tweaks::conversion
