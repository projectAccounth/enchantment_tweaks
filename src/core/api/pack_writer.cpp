#include "core/api/pack_writer.hpp"

#include "core/api/item_conversion.hpp"
#include "core/util/zip_writer.hpp"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

namespace enchantment_tweaks::conversion {

namespace {

uint64_t fnv1a(const std::vector<uint8_t> &bytes) {
  uint64_t h = 0xcbf29ce484222325ULL;
  for (uint8_t b : bytes) {
    h ^= b;
    h *= 0x100000001b3ULL;
  }
  return h;
}

} // namespace

PackBuilder::PackBuilder(std::string description, int packFormat)
    : description_(std::move(description)), packFormat_(packFormat) {}

void PackBuilder::add(const OutputFile &file) {
  if (file.path.empty()) {
    throw std::invalid_argument("pack file path cannot be empty");
  }
  const auto print = std::make_pair(file.bytes.size(), fnv1a(file.bytes));
  const auto existing = paths_.find(file.path);
  if (existing != paths_.end()) {
    if (existing->second == print) return; // shared by two items, written
    throw std::invalid_argument("conflicting pack file: " + file.path);
  }
  paths_.emplace(file.path, print);
  files_.push_back(file);
}

void PackBuilder::put(const OutputFile &file) {
  if (file.path.empty()) {
    throw std::invalid_argument("pack file path cannot be empty");
  }
  if (file.path == "pack.mcmeta") return;
  const auto print = std::make_pair(file.bytes.size(), fnv1a(file.bytes));
  paths_[file.path] = print;
  for (auto &existing : files_) {
    if (existing.path == file.path) {
      existing.bytes = file.bytes;
      return;
    }
  }
  files_.push_back(file);
}

void PackBuilder::setSupportedFormats(int minFormat, int maxFormat) {
  if (minFormat < 0 || maxFormat < minFormat) {
    throw std::invalid_argument("invalid supported format range");
  }
  supported_ = std::make_pair(minFormat, maxFormat);
}

void PackBuilder::addItem(const ConvertedItem &item) {
  const auto colon = item.itemId.find(':');
  if (colon == std::string::npos) {
    throw std::invalid_argument("item id must be namespace:id, got " +
                                item.itemId);
  }
  const std::string ns = item.itemId.substr(0, colon);
  const std::string id = item.itemId.substr(colon + 1);
  const std::string text = item.definition.dump(2);
  add({"assets/" + ns + "/items/" + id + ".json", {text.begin(), text.end()}});
  for (const auto &f : item.files) {
    add(f);
  }
}

// TODO: convert to use json, this is just baseline JSON
std::vector<OutputFile> PackBuilder::files() const {
  std::string mcmeta =
      "{\n  \"pack\": {\n    \"description\": \"" + description_ +
      "\",\n    \"pack_format\": " + std::to_string(packFormat_);
  if (supported_) {
    mcmeta += ",\n    \"supported_formats\": {\"min_inclusive\": " +
              std::to_string(supported_->first) +
              ", \"max_inclusive\": " + std::to_string(supported_->second) +
              "}";
  }
  mcmeta += "\n  }\n}\n";
  std::vector<OutputFile> all;
  all.push_back({"pack.mcmeta", {mcmeta.begin(), mcmeta.end()}});
  all.insert(all.end(), files_.begin(), files_.end());
  return all;
}

void PackBuilder::writeDirectory(const fs::path &root) const {
  for (const auto &f : files()) {
    const auto target = root / f.path;
    fs::create_directories(target.parent_path());
    std::ofstream out(target, std::ios::binary | std::ios::trunc);
    if (!out) {
      throw std::runtime_error("cannot write pack file: " + target.string());
    }
    out.write(reinterpret_cast<const char *>(f.bytes.data()),
              static_cast<std::streamsize>(f.bytes.size()));
    if (!out) {
      throw std::runtime_error("failed writing pack file: " + target.string());
    }
  }
}

void PackBuilder::writeZip(const fs::path &zipPath) const {
  zip::ZipWriter zw;
  for (const auto &f : files()) {
    zw.addFile(f.path, f.bytes);
  }
  zw.write(zipPath);
}

} // namespace enchantment_tweaks::conversion
