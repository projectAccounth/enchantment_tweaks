#pragma once

#include "core/api/item_conversion.hpp"
#include "core/api/pack_writer.hpp"
#include "core/types/common.hpp"
#include "core/util/image.hpp"

#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace enchantment_tweaks::catalog {
namespace fs = std::filesystem;

struct CatalogItem {
  std::string ns;
  std::string id;
  std::string name;                  // display name, from the id
  json definition;                   // the full item definition file
  std::vector<std::string> modelIds; // every model leaf, in natural name order
  std::map<std::string, conversion::ModelSprite>
      sprites;                     // convertible leaves only
  std::string representativeModel; // first convertible model in natural order

  /// False for custom 3D models.
  bool generated = true;
  image::Image sprite; // representative sprite

  [[nodiscard]] std::string key() const { return ns + ":" + id; }
};

enum class ExportScope : uint8_t {
  Representative,
  AllModels,
};

class ItemCatalog {
public:
  /// Loads a catalog from a game JAR or directory, with an optional overlay.
  /// Throws std::runtime_error if the base pack cannot be loaded.
  static ItemCatalog
  load(const fs::path &basePath,
       const std::optional<fs::path> &overlayPath = std::nullopt);

  [[nodiscard]] const std::vector<CatalogItem> &items() const noexcept {
    return items_;
  }
  [[nodiscard]] const CatalogItem *find(const std::string &key) const;
  [[nodiscard]] const std::string &gameVersion() const noexcept {
    return gameVersion_;
  }
  [[nodiscard]] int gameResourceFormat() const noexcept { return format_; }

  /// Writes one PNG per item and returns paths keyed by item key.
  std::map<std::string, std::string> writeIcons(const fs::path &dir) const;

  struct SaveResult {
    bool saved = false;
    fs::path path;
    std::string error;
  };

  [[nodiscard]] conversion::ConvertedItem
  convertItem(const std::string &key,
              const conversion::ItemVariantOptions &options,
              ExportScope scope = ExportScope::AllModels) const;

  [[nodiscard]] conversion::PackBuilder
  buildPack(const std::vector<std::string> &keys,
            const conversion::ItemVariantOptions &options,
            const std::string &description,
            ExportScope scope = ExportScope::Representative) const;

private:
  std::vector<CatalogItem> items_;
  std::string gameVersion_;
  int format_ = 0;
};

/// Creates a display name from an item ID.
std::string displayNameFromId(const std::string &id);

/// Compares strings using natural ordering.
bool naturalLess(const std::string &a, const std::string &b);

} // namespace enchantment_tweaks::catalog
