#include "core/api/item_conversion.hpp"

#include "core/api/mesh.hpp"
#include "core/types/common.hpp"
#include "core/util/image.hpp"
#include "core/types/identifier.hpp"
#include "core/util/text.hpp"

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace enchantment_tweaks::conversion {

namespace {

constexpr const char *kEnchantedSuffix = "_enchanted";
constexpr const char *kUnenchantedSuffix = "_unenchanted";
constexpr const char *kUnshadedSuffix = "_unshaded";
constexpr const char *kOutlineSuffix = "_outline";

json hasComponentNode(const std::string &component, json onTrue, json onFalse) {
  return {
      {"type", "minecraft:condition"}, {"property", "minecraft:has_component"},
      {"component", component},        {"ignore_default", true},
      {"on_true", std::move(onTrue)},  {"on_false", std::move(onFalse)}};
}

// "ns:path" -> ns, path. A bare id uses the minecraft namespace.
std::vector<uint8_t>
modelBytes(const image::Image &sprite, const std::string &ns,
           const std::string &textureKey, const ItemVariantOptions &options,
           bool withOutline, const std::string &outlineKey) {
  mesh::BuildOptions build;
  build.thicknessPx = options.thicknessPx;
  build.textureNamespace = ns;
  build.textureKey = textureKey;
  build.outlineOnly = false;
  build.cullTransparent = true;
  build.mergeColumns = options.mergeColumns;

  if (withOutline) {
    build.outlineSizePx = options.outlineSizePx;
    build.outlineTextureNamespace = ns;
    build.outlineTextureKey = outlineKey;
  }

  // image must outlive
  const mesh::MeshBuilder builder(sprite);
  // no prettyprint
  const std::string text = builder.build(build).serialize().dump();
  return {text.begin(), text.end()};
}

class DefinitionRewriter {
public:
  DefinitionRewriter(const std::map<std::string, ModelSprite> &sprites,
                     const ItemVariantOptions &options, ConvertedItem &out)
      : sprites_(sprites), options_(options), out_(out) {}

  json rewrite(const json &node) {
    if (node.is_object()) {
      if (isModelLeaf(node) &&
          sprites_.count(node.at("model").get<std::string>()) > 0) {
        return wrapLeaf(node);
      }
      json copy = json::object();
      for (const auto &[key, value] : node.items())
        copy[key] = rewrite(value);
      return copy;
    }
    if (node.is_array()) {
      json copy = json::array();
      for (const auto &value : node)
        copy.push_back(rewrite(value));
      return copy;
    }
    return node;
  }

private:
  static bool isModelLeaf(const json &node) {
    return node.value("type", "") == "minecraft:model" &&
           node.contains("model") && node.at("model").is_string();
  }

  void addOnce(const std::string &path, std::vector<uint8_t> bytes) {
    if (!generated_.insert(path).second) return;
    out_.files.push_back({path, std::move(bytes)});
  }

  //  a custom 3D model keeps its elements, each texture gets an
  // unshaded copy, and the glowing model points at those copies.
  // TODO: Add outline support for custom 3D models.
  void generateCustomModel(const std::string &modelId,
                           const ModelSprite &source) {
    if (!generatedModels_.insert(modelId).second) return;
    const Identifier nsId = Identifier::parse(modelId);
    const std::string &ns = nsId.getNamespace();
    const std::string modelPath = nsId.getPath();
    for (const auto &[ref, image] : source.customTextures) {
      const Identifier texNsId = Identifier::parse(ref);
    const std::string &texNs = texNsId.getNamespace();
    const std::string texPath = texNsId.getPath();
      std::string path = "assets/";
      path += texNs;
      path += "/textures/";
      path += texPath;
      path += kUnshadedSuffix;
      path += ".png";
      addOnce(path, makeUnshadedSprite(image).encodePng());
    }
    json glow = source.customModel;
    for (auto &[variable, value] : glow.at("textures").items()) {
      if (!value.is_string()) continue;
      const std::string ref = value.get<std::string>();
      if (ref.empty() || ref[0] == '#' || source.customTextures.count(ref) == 0)
        continue;
      value = ref + kUnshadedSuffix;
    }
    const std::string text = glow.dump();
    addOnce("assets/" + ns + "/models/" + modelPath + kEnchantedSuffix +
                ".json",
            {text.begin(), text.end()});
  }

  void generateModel(const std::string &modelId, const ModelSprite &source) {
    if (source.isCustom()) {
      generateCustomModel(modelId, source);
      return;
    }
    if (!generatedModels_.insert(modelId).second) return;

    const Identifier nsId = Identifier::parse(modelId);
    const std::string &ns = nsId.getNamespace();
    const std::string modelPath = nsId.getPath();
    const std::string texKey = source.textureKey;
    const std::string unshadedKey = texKey + kUnshadedSuffix;
    const std::string outlineKey = texKey + kOutlineSuffix;
    const std::string unshadedOutlineKey = outlineKey + kUnshadedSuffix;
    const bool hasOutline = options_.outlineSizePx > 0.0;

    addOnce("assets/" + ns + "/textures/" + unshadedKey + ".png",
            makeUnshadedSprite(source.sprite).encodePng());
    if (hasOutline) {
      addOnce("assets/" + ns + "/textures/" + unshadedOutlineKey + ".png",
              makeOutlineSprite(source.sprite.width(), source.sprite.height(),
                                options_.outlineColor, true)
                  .encodePng());
    }
    addOnce("assets/" + ns + "/models/" + modelPath + kEnchantedSuffix +
                ".json",
            modelBytes(source.sprite, ns, unshadedKey, options_, hasOutline,
                       unshadedOutlineKey));

    if (options_.unenchantedOutline) {
      const std::string key =
          options_.unenchantedUnshaded ? unshadedKey : texKey;
      const std::string outKey =
          options_.unenchantedUnshaded ? unshadedOutlineKey : outlineKey;
      if (hasOutline && !options_.unenchantedUnshaded) {
        addOnce("assets/" + ns + "/textures/" + outlineKey + ".png",
                makeOutlineSprite(source.sprite.width(), source.sprite.height(),
                                  options_.outlineColor, false)
                    .encodePng());
      }
      addOnce("assets/" + ns + "/models/" + modelPath + kUnenchantedSuffix +
                  ".json",
              modelBytes(source.sprite, ns, key, options_, hasOutline, outKey));
    }
  }

  json wrapLeaf(const json &leaf) {
    const std::string modelId = leaf.at("model").get<std::string>();
    const ModelSprite &source = sprites_.at(modelId);
    generateModel(modelId, source);

    const Identifier nsId = Identifier::parse(modelId);
    const std::string &ns = nsId.getNamespace();
    const std::string modelPath = nsId.getPath();
    json glow = leaf;
    glow["model"] = ns + ":" + modelPath + kEnchantedSuffix;
    json plain = leaf;
    if (options_.unenchantedOutline && !source.isCustom()) {
      plain["model"] = ns + ":" + modelPath + kUnenchantedSuffix;
    }

    return hasComponentNode(
        "minecraft:enchantment_glint_override", glow,
        hasComponentNode("minecraft:enchantments", glow, plain));
  }

  const std::map<std::string, ModelSprite> &sprites_;
  const ItemVariantOptions &options_;
  ConvertedItem &out_;
  std::set<std::string> generated_;       // output paths already written
  std::set<std::string> generatedModels_; // model ids already generated
};

} // namespace

image::Image makeUnshadedSprite(const image::Image &source) {
  if (source.empty()) {
    throw std::invalid_argument("makeUnshadedSprite: empty sprite");
  }
  image::Image out(source.width(), source.height(), source.pixels());
  auto &px = out.pixels();
  for (size_t i = 0; i + 3 < px.size(); i += 4) {
    if (px[i + 3] != 0) {
      px[i + 3] = kUnshadedAlpha;
    }
  }
  return out;
}

image::Image makeOutlineSprite(int width, int height, OutlineColor color,
                               bool unshaded) {
  if (width <= 0 || height <= 0) {
    throw std::invalid_argument("makeOutlineSprite: invalid size");
  }
  image::Image out(width, height);
  const uint8_t alpha = unshaded ? kUnshadedAlpha : 255;
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      out.setPixel(x, y, {color.r, color.g, color.b, alpha});
    }
  }
  return out;
}

ConvertedItem
convertItemDefinition(const std::string &itemNs, const std::string &itemId,
                      const json &definition,
                      const std::map<std::string, ModelSprite> &spritesByModel,
                      const ItemVariantOptions &options) {
  if (options.thicknessPx <= 0.0) {
    throw std::invalid_argument(
        "convertItemDefinition: thickness must be positive");
  }
  for (const auto &[modelId, source] : spritesByModel) {
    // TODO: handle model that uses multiple textures (like someone decides to
    // go mental and make a custom compass model)
    if (!source.isCustom() && source.sprite.empty()) {
      throw std::invalid_argument("convertItemDefinition: empty sprite for " +
                                  modelId);
    }
    if (source.isCustom() && source.customTextures.empty()) {
      throw std::invalid_argument(
          "convertItemDefinition: custom model without textures: " + modelId);
    }
  }

  ConvertedItem result;
  result.itemId = itemNs + ":" + itemId;
  DefinitionRewriter rewriter(spritesByModel, options, result);
  result.definition = rewriter.rewrite(definition);
  return result;
}

ConvertedItem convertSingleSpriteItem(const std::string &ns,
                                      const std::string &id,
                                      const image::Image &sprite,
                                      const ItemVariantOptions &options,
                                      std::optional<json> baseNode) {
  if (sprite.empty()) {
    throw std::invalid_argument("convertSingleSpriteItem: empty sprite for " +
                                ns + ":" + id);
  }
  const std::string modelId = ns + ":item/" + id;
  json node = baseNode ? *baseNode
                       : json{{"type", "minecraft:model"}, {"model", modelId}};
  const std::map<std::string, ModelSprite> sprites{
      {modelId, {"item/" + id, sprite}}};
  return convertItemDefinition(ns, id, json{{"model", node}}, sprites, options);
}

} // namespace enchantment_tweaks::conversion
