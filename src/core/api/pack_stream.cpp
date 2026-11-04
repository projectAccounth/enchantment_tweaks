#include "core/api/pack_stream.hpp"

#include "core/util/file_io.hpp"
#include <fstream>
#include <iterator>

#include <stdexcept>

namespace fs = std::filesystem;

namespace enchantment_tweaks::conversion {

PackStreamer::PackStreamer(const fs::path &destination,
                           std::string description, int packFormat)
    : zip_(destination), description_(std::move(description)),
      packFormat_(packFormat) {}

void PackStreamer::write(const OutputFile &file) {
  if (file.path == "pack.mcmeta") return;
  const auto crc = zip::crc32Of(file.bytes);
  const auto known = written_.find(file.path);
  if (known != written_.end()) {
    if (known->second == std::make_pair(file.bytes.size(), crc))
      return; // same file, already written
    throw std::invalid_argument("conflicting pack file: " + file.path);
  }
  zip_.add(file.path, file.bytes);
  written_.emplace(file.path, std::make_pair(file.bytes.size(), crc));
}

void PackStreamer::addItem(const ConvertedItem &item) {
  const auto colon = item.itemId.find(':');
  if (colon == std::string::npos)
    throw std::invalid_argument("item id must be namespace:id, got " +
                                item.itemId);
  const std::string ns = item.itemId.substr(0, colon);
  const std::string id = item.itemId.substr(colon + 1);
  const std::string text = item.definition.dump();
  write(
      {"assets/" + ns + "/items/" + id + ".json", {text.begin(), text.end()}});
  for (const auto &file : item.files)
    write(file);
}

void PackStreamer::addIfAbsent(const OutputFile &file) {
  if (file.path == "pack.mcmeta" || written_.count(file.path) > 0) return;
  write(file);
}

void PackStreamer::addOverlay(const ShaderOverlay &overlay,
                              const fs::path &root) {
  for (const auto &file : overlay.files) {
    auto bytes = file_io::readBytes(root / overlay.directory / file);
    if (!bytes)
      throw std::runtime_error("shader file missing: " +
                               (root / overlay.directory / file).string());
    write({overlay.directory + "/" + file, std::move(*bytes)});
  }
  overlays_.push_back(overlay);
}

void PackStreamer::setSupportedFormats(int minFormat, int maxFormat) {
  if (minFormat < 0 || maxFormat < minFormat)
    throw std::invalid_argument("invalid supported format range");
  supported_ = std::make_pair(minFormat, maxFormat);
}

void PackStreamer::close() {
  std::string mcmeta =
      "{\n  \"pack\": {\n    \"description\": \"" + description_ +
      "\",\n    \"pack_format\": " + std::to_string(packFormat_);
  if (supported_) {
    mcmeta += ",\n    \"supported_formats\": {\"min_inclusive\": " +
              std::to_string(supported_->first) +
              ", \"max_inclusive\": " + std::to_string(supported_->second) +
              "}";
  }
  mcmeta += "\n  }";
  if (!overlays_.empty()) {
    mcmeta += ",\n  \"overlays\": {\n    \"entries\": [";
    for (size_t i = 0; i < overlays_.size(); ++i) {
      const auto &o = overlays_[i];
      mcmeta += std::string(i == 0 ? "" : ",") + "\n      {\"directory\": \"" +
                o.directory + "\", \"formats\": {\"min_inclusive\": " +
                std::to_string(o.minFormat) +
                ", \"max_inclusive\": " + std::to_string(o.maxFormat) +
                "}, \"min_format\": " + std::to_string(o.minFormat) +
                ", \"max_format\": " + std::to_string(o.maxFormat) + "}";
    }
    mcmeta += "\n    ]\n  }";
  }
  mcmeta += "\n}\n";
  zip_.add("pack.mcmeta", std::vector<uint8_t>(mcmeta.begin(), mcmeta.end()));
  zip_.close();
}

} // namespace enchantment_tweaks::conversion
