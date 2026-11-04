#pragma once

#include "core/api/mesh.hpp"
#include "core/types/common.hpp"
#include "core/util/image.hpp"

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace enchantment_tweaks::conversion {

/// Alpha value used to mark unshaded pixels.
constexpr uint8_t kUnshadedAlpha = 252;

struct OutlineColor {
  uint8_t r = 255;
  uint8_t g = 255;
  uint8_t b = 255;
};

struct ItemVariantOptions {
  double thicknessPx = 1.0;

  /// Zero disables the outline.
  double outlineSizePx = 0.0;
  OutlineColor outlineColor;

  /// Merge horizontal runs of opaque pixels into one model element.
  bool mergeColumns = false;

  /// Add outlines to unenchanted models.
  bool unenchantedOutline = false;
  /// Apply the unshaded mask to the unenchanted outline.
  bool unenchantedUnshaded = false;
};

struct OutputFile {
  std::string path;
  std::vector<uint8_t> bytes;
};

struct ConvertedItem {
  std::string itemId;            // resource location
  json definition;               // contents of assets/<ns>/items/<id>.json
  std::vector<OutputFile> files; // models and generated textures
};

/// Marks every visible pixel as unshaded. Partial transparency is lost.
image::Image makeUnshadedSprite(const image::Image &source);

/// Creates a solid outline sprite, optionally marked as unshaded.
image::Image makeOutlineSprite(int width, int height, OutlineColor color,
                               bool unshaded);

/// Sprite and source model data used to generate an item model.
struct ModelSprite {
  std::string textureKey; // Texture key, e.g. "item/compass_00".
  image::Image sprite;
  /// Resolved custom model and its textures; empty for generated sprites.
  json customModel;
  std::map<std::string, image::Image> customTextures;

  [[nodiscard]] bool isCustom() const { return !customModel.is_null(); }
};

/// Rewrites an item definition and generates its models and textures.
ConvertedItem
convertItemDefinition(const std::string &itemNs, const std::string &itemId,
                      const json &definition,
                      const std::map<std::string, ModelSprite> &spritesByModel,
                      const ItemVariantOptions &options);

/// Converts a single-sprite item with a plain model definition.
ConvertedItem
convertSingleSpriteItem(const std::string &ns, const std::string &id,
                        const image::Image &sprite,
                        const ItemVariantOptions &options,
                        std::optional<json> baseNode = std::nullopt);

} // namespace enchantment_tweaks::conversion
