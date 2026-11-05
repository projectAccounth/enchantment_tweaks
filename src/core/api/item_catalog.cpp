#include "core/api/item_catalog.hpp"

#include "core/api/item_conversion.hpp"
#include "core/api/pack_writer.hpp"
#include "core/resource/resource_pack.hpp"
#include "core/types/common.hpp"
#include "core/types/identifier.hpp"
#include "core/types/mesh/types.hpp"
#include "core/util/debug.hpp"
#include "core/util/image.hpp"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <map>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <tuple>
#include <unordered_set>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

namespace enchantment_tweaks::catalog {

namespace {

void collectModelIds(const json &node, std::vector<std::string> &out) {
  if (node.is_object()) {
    if (node.value("type", "") == "minecraft:model" && node.contains("model") &&
        node.at("model").is_string()) {
      out.push_back(node.at("model").get<std::string>());
    }
    for (const auto &[key, value] : node.items())
      collectModelIds(value, out);
  } else if (node.is_array()) {
    for (const auto &value : node)
      collectModelIds(value, out);
  }
}

const resource::TextureEntry *
findTexture(const resource::ResourcePackLoader &loader, const std::string &ns,
            const std::string &name) {
  for (const auto &t : loader.getTextures()) {
    if (t.nmsp_ == ns && t.name == name) return &t;
  }
  return nullptr;
}

} // namespace

std::string hashPixels(const image::Image &img) {
  uint64_t hash = 0xcbf29ce484222325ULL;
  const auto mix = [&hash](uint8_t byte) {
    hash ^= byte;
    hash *= 0x100000001b3ULL;
  };
  for (const int dim : {img.width(), img.height()}) {
    for (int shift = 0; shift < 32; shift += 8)
      mix(static_cast<uint8_t>(dim >> shift));
  }
  for (const uint8_t byte : img.pixels())
    mix(byte);
  char text[17];
  (void)std::snprintf(text, sizeof(text), "%016llx",
                      static_cast<unsigned long long>(hash));
  return text;
}

std::string displayNameFromId(const std::string &id) {
  std::string out;
  bool startOfWord = true;
  for (char c : id) {
    if (c == '_') {
      out += ' ';
      startOfWord = true;
    } else if (startOfWord) {
      out += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
      startOfWord = false;
    } else {
      out += c;
    }
  }
  return out;
}

ItemCatalog ItemCatalog::load(const fs::path &basePath,
                              const std::optional<fs::path> &overlayPath) {
  resource::ResourcePackLoader base(basePath);
  if (!base.load()) {
    throw std::runtime_error("could not load base pack: " + basePath.string());
  }

  std::optional<resource::ResourcePackLoader> overlay;
  if (overlayPath) {
    overlay.emplace(*overlayPath);
    if (!overlay->load()) {
      throw std::runtime_error("could not load pack: " + overlayPath->string());
    }
    overlay->setBasePack(base);
  }

  const resource::ResourcePackLoader &top = overlay ? *overlay : base;
  ItemCatalog out;
  out.gameVersion_ = base.getPack().gameVersion;
  out.format_ = base.getPack().gameResourceFormat;

  // items come from definition, models will point to textures
  // fixes random UI sprites displaying on custom packs
  std::unordered_set<std::string> packModels;
  std::unordered_set<std::string> packTextures;
  if (overlay) {
    for (const auto &model : overlay->getModels())
      packModels.insert(model.nmsp_ + ":" + model.name);
    for (const auto &tex : overlay->getTextures())
      packTextures.insert(tex.nmsp_ + ":" + tex.name);
  }
  const auto packChangesModel = [&](const std::string &modelId) {
    try {
      const Identifier parsed = Identifier::parse(modelId);
      return packModels.count(parsed.getNamespace() + ":" + parsed.getPath()) >
             0;
    } catch (const std::runtime_error &) {
      return false;
    }
  };
  const auto packChangesTexture = [&](const std::string &ref) {
    try {
      const Identifier parsed = Identifier::parse(ref);
      return packTextures.count(parsed.getNamespace() + ":" +
                                parsed.getPath()) > 0;
    } catch (const std::runtime_error &) {
      return false;
    }
  };
  const auto packChangesItem = [&](const json &baseDef) {
    std::vector<std::string> leaves;
    collectModelIds(baseDef, leaves);
    for (const auto &leaf : leaves) {
      if (packChangesModel(leaf)) return true;
      const auto model = base.getModelJson(leaf);
      if (!model || !model->contains("textures")) continue;
      for (const auto &[variable, value] : model->at("textures").items()) {
        if (value.is_string() && !value.get<std::string>().starts_with('#') &&
            packChangesTexture(value.get<std::string>())) {
          return true;
        }
      }
    }
    return false;
  };

  std::vector<std::string> itemIds;
  std::unordered_set<std::string> seen;
  if (overlay) {
    for (const auto &def : overlay->getDef()) {
      const std::string id = def.nmsp + ":" + def.name;
      if (seen.insert(id).second) itemIds.push_back(id);
    }
  }
  for (const auto &def : base.getDef()) {
    const std::string id = def.nmsp + ":" + def.name;
    if (seen.count(id) > 0) continue;
    if (overlay && !packChangesItem(def.rawJson)) continue;
    seen.insert(id);
    itemIds.push_back(id);
  }

  resource::ModelResolver resolver(top);
  for (const auto &id : itemIds) {
    const auto def = top.getItemDefinitionJson(id);
    if (!def || !def->contains("model")) continue;

    std::vector<std::string> leaves;
    collectModelIds(*def, leaves);
    if (leaves.empty()) continue;

    CatalogItem item;
    try {
      const Identifier parsed = Identifier::parse(id);
      item.ns = parsed.getNamespace();
      item.id = parsed.getPath();
    } catch (const std::runtime_error &error) {
      debug::Logger::warning()
          << "skipping item with an invalid id: " << error.what();
      continue;
    }
    item.name = displayNameFromId(item.id);
    item.definition = *def;
    std::set<std::string> unique(leaves.begin(), leaves.end());
    item.modelIds.assign(unique.begin(), unique.end());
    std::ranges::sort(item.modelIds, naturalLess);

    for (const auto &modelId : item.modelIds) {
      try {
        const mesh::Model model = resolver.resolve(modelId);
        if (!model.elements.empty()) {

          // Experimental support for custom 3d models

          conversion::ModelSprite custom;
          custom.customModel = model.serialize();
          custom.customModel.erase(
              "parent"); // the textures and elements are already resolved
          for (const auto &[variable, ref] : model.textures) {
            if (ref.empty() || ref[0] == '#') continue;
            const Identifier texture = Identifier::parse(ref);
            const std::string &texNs = texture.getNamespace();
            const std::string &texName = texture.getPath();
            const resource::TextureEntry *tex =
                findTexture(top, texNs, texName);
            if (!tex && overlay) tex = findTexture(base, texNs, texName);
            if (tex && tex->width > 0 && tex->height > 0) {
              custom.customTextures.emplace(
                  ref, image::Image(tex->width, tex->height, tex->pixels));
            }
          }
          if (custom.customTextures.empty())
            continue; // nothing to show or convert
          if (item.sprite.empty())
            item.sprite = custom.customTextures.begin()->second; // icon
          item.generated = false;
          item.sprites.emplace(modelId, std::move(custom));
          continue;
        }

        const auto layer = model.textures.find("layer0");
        if (layer == model.textures.end() || layer->second.empty() ||
            layer->second[0] == '#')
          continue;

        const Identifier texture = Identifier::parse(layer->second);
        const std::string &texNs = texture.getNamespace();
        const std::string &texName = texture.getPath();
        const resource::TextureEntry *tex = findTexture(top, texNs, texName);
        if (!tex && overlay) tex = findTexture(base, texNs, texName);
        if (!tex || tex->width <= 0 || tex->height <= 0) continue;

        image::Image sprite(tex->width, tex->height, tex->pixels);
        if (item.sprite.empty()) item.sprite = sprite;
        item.sprites.emplace(
            modelId, conversion::ModelSprite{texName, std::move(sprite)});
      } catch (const std::exception &e) {
        debug::Logger::warning()
            << "skipping model " << modelId << " of " << id << ": " << e.what();
      }
    }
    if (item.sprites.empty()) continue; // nothing convertible: not offered
    item.representativeModel =
        item.modelIds.empty() ? std::string() : item.modelIds.front();
    for (const auto &modelId : item.modelIds) {
      if (item.sprites.count(modelId) > 0) {
        item.representativeModel = modelId;
        break;
      }
    }
    out.items_.push_back(std::move(item));
  }
  return out;
}

bool naturalLess(const std::string &a, const std::string &b) {
  size_t i = 0, j = 0;
  while (i < a.size() && j < b.size()) {
    if (std::isdigit(static_cast<unsigned char>(a[i])) &&
        std::isdigit(static_cast<unsigned char>(b[j]))) {
      size_t ie = i, je = j;

      while (ie < a.size() && std::isdigit(static_cast<unsigned char>(a[ie])))
        ++ie;

      while (je < b.size() && std::isdigit(static_cast<unsigned char>(b[je])))
        ++je;

      const std::string na = a.substr(i, ie - i), nb = b.substr(j, je - j);
      const std::string sa =
          na.substr(std::min(na.find_first_not_of('0'), na.size() - 1));
      const std::string sb =
          nb.substr(std::min(nb.find_first_not_of('0'), nb.size() - 1));

      if (sa.size() != sb.size()) return sa.size() < sb.size();

      if (sa != sb) return sa < sb;

      i = ie;
      j = je;
    } else {
      if (a[i] != b[j]) return a[i] < b[j];
      ++i;
      ++j;
    }
  }
  return (a.size() - i) < (b.size() - j);
}

const CatalogItem *ItemCatalog::find(const std::string &key) const {
  for (const auto &item : items_) {
    if (item.key() == key) return &item;
  }
  return nullptr;
}

std::map<std::string, std::string>
ItemCatalog::writeIcons(const fs::path &dir) const {
  constexpr int kIconPixels = 96; // the grid shows icons at about this size
  fs::create_directories(dir);

  std::map<std::string, std::string> paths;
  for (const auto &item : items_) {
    const int largest = std::max(item.sprite.width(), item.sprite.height());
    const int factor = std::max(1, kIconPixels / std::max(1, largest));

    // The file name includes a hash of the pixels, so a changed texture (from a
    // pack) will use a different texture
    // Previously the icons will always be picked up from the central cache for
    // a version
    const auto file = dir / (item.ns + "_" + item.id + "_" +
                             hashPixels(item.sprite) + ".png");

    if (!fs::exists(file)) {
      item.sprite.scaledNearest(factor).save(file.string());
    }
    paths[item.key()] = file.string();
  }
  return paths;
}

conversion::ConvertedItem
ItemCatalog::convertItem(const std::string &key,
                         const conversion::ItemVariantOptions &options,
                         ExportScope scope) const {
  const CatalogItem *item = find(key);
  if (!item) throw std::invalid_argument("unknown catalog item: " + key);
  if (scope == ExportScope::AllModels) {
    return conversion::convertItemDefinition(
        item->ns, item->id, item->definition, item->sprites, options);
  }
  std::map<std::string, conversion::ModelSprite> representative;
  const auto it = item->sprites.find(item->representativeModel);
  if (it != item->sprites.end()) representative.insert(*it);
  return conversion::convertItemDefinition(item->ns, item->id, item->definition,
                                           representative, options);
}

conversion::PackBuilder
ItemCatalog::buildPack(const std::vector<std::string> &keys,
                       const conversion::ItemVariantOptions &options,
                       const std::string &description,
                       ExportScope scope) const {
  conversion::PackBuilder pack(description, format_);
  for (const auto &key : keys) {
    const CatalogItem *item = find(key);
    if (!item) throw std::invalid_argument("unknown catalog item: " + key);

    if (scope == ExportScope::AllModels) {
      pack.addItem(conversion::convertItemDefinition(
          item->ns, item->id, item->definition, item->sprites, options));
    } else {
      std::map<std::string, conversion::ModelSprite> representative;
      const auto it = item->sprites.find(item->representativeModel);
      if (it != item->sprites.end()) representative.insert(*it);
      pack.addItem(conversion::convertItemDefinition(
          item->ns, item->id, item->definition, representative, options));
    }
  }
  return pack;
}

} // namespace enchantment_tweaks::catalog
