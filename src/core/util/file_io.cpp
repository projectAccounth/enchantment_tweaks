#include "core/util/file_io.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <iterator>
#include <optional>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace enchantment_tweaks::file_io {

std::optional<std::string> readText(const fs::path &path) {
  std::ifstream in(path, std::ios::binary);
  if (!in) return std::nullopt;

  return std::string(std::istreambuf_iterator<char>(in),
                     std::istreambuf_iterator<char>());
}

std::optional<std::vector<uint8_t>>
readBytes(const fs::path &path) {
  std::ifstream in(path, std::ios::binary);

  if (!in) return std::nullopt;

  in.seekg(0, std::ios::end);

  const auto size = static_cast<std::streamsize>(in.tellg());
  in.seekg(0, std::ios::beg);

  std::vector<uint8_t> bytes(static_cast<size_t>(size));

  if (size > 0 && !in.read(reinterpret_cast<char *>(bytes.data()), size))
    return std::nullopt;

  return bytes;
}

} // namespace enchantment_tweaks::file_io
