#pragma once

#include "core/api/item_conversion.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace enchantment_tweaks::conversion {
namespace fs = std::filesystem;

class PackBuilder {
public:
  PackBuilder(std::string description, int packFormat);

  void add(const OutputFile &file);
  void put(const OutputFile &file);
  void setSupportedFormats(int minFormat, int maxFormat);
  void addItem(const ConvertedItem &item);

  [[nodiscard]] std::vector<OutputFile> files() const;

  void writeDirectory(const fs::path &root) const;
  void writeZip(const fs::path &zipPath) const;

private:
  std::string description_;
  int packFormat_;
  std::optional<std::pair<int, int>> supported_;
  std::vector<OutputFile> files_;
  std::map<std::string, std::pair<size_t, uint64_t>> paths_;
};

} // namespace enchantment_tweaks::conversion
