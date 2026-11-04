#pragma once

#include "core/api/item_conversion.hpp"
#include "core/api/shader_overlays.hpp"
#include "core/util/zip_streaming.hpp"

#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <utility>

namespace enchantment_tweaks::conversion {
namespace fs = std::filesystem;

class PackStreamer {
public:
  PackStreamer(const fs::path &destination, std::string description,
               int packFormat);

  void addItem(const ConvertedItem &item);
  void addIfAbsent(const OutputFile &file);
  void setSupportedFormats(int minFormat, int maxFormat);
  void addOverlay(const ShaderOverlay &overlay, const fs::path &root);

  void close();

private:
  void write(const OutputFile &file);
  zip::StreamingZipWriter zip_;
  std::string description_;
  int packFormat_;
  std::optional<std::pair<int, int>> supported_;
  std::vector<ShaderOverlay> overlays_;
  std::map<std::string, std::pair<size_t, uint32_t>> written_;
};

} // namespace enchantment_tweaks::conversion
