#include "core/util/pack_files.hpp"

#include "core/api/item_conversion.hpp"
#include "core/util/file_io.hpp"

#include "libraries/miniz/miniz.h"
#include "libraries/miniz/miniz_common.h"
#include "libraries/miniz/miniz_zip.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace enchantment_tweaks::pack_files {

namespace fs = std::filesystem;

namespace {

std::vector<uint8_t> readWhole(const fs::path &file) {
  auto bytes = file_io::readBytes(file);
  if (!bytes) throw std::runtime_error("cannot read " + file.string());
  return std::move(*bytes);
}

std::vector<conversion::OutputFile> readZip(const fs::path &zipPath) {
  std::vector<uint8_t> bytes = readWhole(zipPath);
  mz_zip_archive zip{};
  if (!mz_zip_reader_init_mem(&zip, bytes.data(), bytes.size(), 0)) {
    throw std::runtime_error("not a valid zip: " + zipPath.string());
  }
  std::vector<conversion::OutputFile> files;
  const mz_uint count = mz_zip_reader_get_num_files(&zip);
  for (mz_uint i = 0; i < count; ++i) {
    mz_zip_archive_file_stat stat{};
    if (!mz_zip_reader_file_stat(&zip, i, &stat) ||
        mz_zip_reader_is_file_a_directory(&zip, i)) {
      continue;
    }
    size_t size = 0;
    void *data = mz_zip_reader_extract_to_heap(&zip, i, &size, 0);
    if (!data) {
      mz_zip_reader_end(&zip);
      throw std::runtime_error("could not extract " +
                               std::string(stat.m_filename) + " from " +
                               zipPath.string());
    }
    const auto *first = static_cast<const uint8_t *>(data);
    files.push_back(
        {stat.m_filename, std::vector<uint8_t>(first, first + size)});
    mz_free(data);
  }
  mz_zip_reader_end(&zip);
  return files;
}

std::vector<conversion::OutputFile> readFolder(const fs::path &root) {
  std::vector<conversion::OutputFile> files;
  std::error_code ec;
  for (auto it = fs::recursive_directory_iterator(root, ec);
       it != fs::recursive_directory_iterator(); it.increment(ec)) {
    if (ec)
      throw std::runtime_error("cannot read " + root.string() + ": " +
                               ec.message());
    if (!it->is_regular_file(ec)) continue;
    const std::string rel = fs::relative(it->path(), root, ec).generic_string();
    files.push_back({rel, readWhole(it->path())});
  }
  return files;
}

} // namespace

std::vector<conversion::OutputFile> readPackFiles(const fs::path &pack) {
  std::error_code ec;
  if (fs::is_directory(pack, ec)) return readFolder(pack);
  if (fs::is_regular_file(pack, ec)) return readZip(pack);
  throw std::runtime_error("pack not found: " + pack.string());
}

} // namespace enchantment_tweaks::pack_files
