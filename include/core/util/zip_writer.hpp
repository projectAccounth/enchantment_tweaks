#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace enchantment_tweaks::zip {
namespace fs = std::filesystem;

class ZipWriter {
public:
  void addFile(const std::string &path, const std::vector<uint8_t> &bytes);
  void addFile(const std::string &path, const std::string &text);

  [[nodiscard]] std::vector<uint8_t> finish() const;
  void write(const fs::path &outPath) const;

private:
  struct Entry {
    std::string path;
    std::vector<uint8_t> data;
  };
  std::vector<Entry> entries_;
};

} // namespace enchantment_tweaks::zip
