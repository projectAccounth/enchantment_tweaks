#pragma once

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <set>
#include <string>
#include <vector>

namespace enchantment_tweaks::zip {
namespace fs = std::filesystem;


/// Writes a stored (uncompressed) ZIP one entry at a time.
class StreamingZipWriter {
public:
  /// Throws std::runtime_error if the destination cannot be opened.
  explicit StreamingZipWriter(const fs::path &destination);

  [[nodiscard]] bool contains(const std::string &path) const {
    return names_.count(path) > 0;
  }

  /// Throws std::invalid_argument for empty or duplicate paths, and
  /// std::runtime_error if writing fails.
  void add(const std::string &path, const std::vector<uint8_t> &bytes);

  /// Writes the central directory and closes the file. Repeated calls do
  /// nothing. Throws std::runtime_error if writing fails.
  void close();

private:
  struct Entry {
    std::string path;
    uint32_t crc;
    uint32_t size;
    uint32_t offset;
  };
  std::ofstream out_;
  fs::path destination_;
  std::vector<Entry> entries_;
  std::set<std::string> names_;
  uint64_t offset_ = 0;
  bool closed_ = false;
};

/// Computes CRC-32.
uint32_t crc32Of(const std::vector<uint8_t> &bytes);

} // namespace enchantment_tweaks::zip
