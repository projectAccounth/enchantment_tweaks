#pragma once

#include "core/types/items/item_model_definition.hpp"
#include "core/types/mesh/types.hpp"
#include "core/util/image.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace enchantment_tweaks::resource {

namespace fs = std::filesystem;
using nlohmann::json;

struct ResourcePack {
  std::string name;
  std::string description;
  int packFormat = 0;
  fs::path rootPath;
  bool isDirectory = false;
  std::vector<fs::path> searchPaths;
  /// Resource format from version.json (0 if absent; client JARs lack
  /// pack.mcmeta).
  int gameResourceFormat = 0;
  /// Game version from version.json, or empty if absent.
  std::string gameVersion;
};

struct ModelEntry {
  fs::path path;
  std::string nmsp_;
  std::string name;
  json rawJson;
};

struct TextureEntry {
  fs::path path;
  std::string nmsp_;
  std::string name;
  std::vector<uint8_t> pixels;
  int width = 0;
  int height = 0;
};

struct ModelDefinitionEntry {
  fs::path path;
  std::string nmsp;
  std::string name;
  json rawJson;
};

class ResourcePackLoader {
public:
  explicit ResourcePackLoader(const fs::path &packRoot);

  bool load();
  bool loadFromDirectory(const fs::path &dir);
  bool loadFromZip(const fs::path &jarPath);

  const ResourcePack &getPack() const { return pack_; }
  const std::vector<ModelEntry> &getModels() const { return models_; }
  const std::vector<TextureEntry> &getTextures() const { return textures_; }
  const std::vector<ModelDefinitionEntry> &getDef() const { return def_; }

  /// Collects model IDs referenced by item definitions, including nested
  /// models. Unqualified IDs use the "minecraft" namespace.
  std::vector<std::string> collectReferencedModelIds() const;

  /// Loads only the requested models, re-scanning the pack source as needed.
  void loadModels(const std::vector<std::string> &modelIds);

  /// Set a base pack to fall back to when models aren't found in this pack.
  void setBasePack(const ResourcePackLoader &basePack);

  std::optional<json> getModelJson(const std::string &nmsp,
                                   const std::string &name) const;
  std::optional<json> getModelJson(const std::string &fullId) const;
  std::optional<json> getItemDefinitionJson(const std::string &itemId) const;

  std::optional<std::vector<uint8_t>>
  getTexturePixels(const std::string &nmsp, const std::string &name) const;

  bool unpack(const fs::path &outDir) const;

private:
  void scanModelDef(const fs::path &defPath, const std::string &nmsp);
  void scanModels(const fs::path &modelsDir, const std::string &nmsp,
                  const std::unordered_set<std::string> *filter = nullptr);
  void scanTextures(const fs::path &texturesDir, const std::string &nmsp);

  // Crawl a single item definition JSON for referenced model IDs.
  void collectModelRefsFromDef(const json &defJson,
                               std::unordered_set<std::string> &refs) const;

  // Recursively crawl an ItemModel JSON tree for "model"/"base" string refs.
  static void
  collectModelRefsFromItemModel(const json &modelJson,
                                std::unordered_set<std::string> &refs);

  ResourcePack pack_;
  std::vector<ModelEntry> models_;
  std::vector<TextureEntry> textures_;
  std::vector<ModelDefinitionEntry> def_;
  std::unordered_map<std::string, size_t> modelIndex_;
  std::unordered_map<std::string, size_t> textureIndex_;
  const ResourcePackLoader *basePack_ = nullptr;
};

class ModelResolver {
public:
  explicit ModelResolver(const ResourcePackLoader &loader);

  mesh::Model resolve(const std::string &modelId) const;
  mesh::Model resolve(const json &modelJson) const;

  std::unique_ptr<item::ItemModelDefinition>
  resolveItemDefinition(const std::string &itemId) const;
  std::unique_ptr<item::ItemModelDefinition>
  resolveItemDefinition(const json &itemJson) const;

  void setTextureResolver(
      std::function<
          std::optional<enchantment_tweaks::image::Image>(const std::string &)>
          resolver);

private:
  mesh::Model
  resolveModelRecursive(const json &modelJson,
                        std::unordered_set<std::string> &visited) const;

  json resolveParent(const json &modelJson,
                     std::unordered_set<std::string> &visited) const;

  json resolveTextures(const json &modelJson) const;

  json resolveModel(const json &modelJson) const;

  const ResourcePackLoader &loader_;
  std::function<std::optional<enchantment_tweaks::image::Image>(
      const std::string &)>
      textureResolver_;
};

class ModelTree {
public:
  explicit ModelTree(const ResourcePackLoader &loader);

  void build();

  const mesh::Model *getModel(const std::string &id) const;
  const item::ItemModelDefinition *
  getItemDefinition(const std::string &id) const;

  std::vector<std::string> getAllModelIds() const;
  std::vector<std::string> getAllItemDefinitionIds() const;

  void resolveDependencies();

private:
  void discoverModels();
  void discoverItemDefinitions();

  const ResourcePackLoader &loader_;
  ModelResolver resolver_;
  std::unordered_map<std::string, mesh::Model> models_;
  std::unordered_map<std::string, item::ItemModelDefinition> itemDefs_;
  std::unordered_map<std::string, std::vector<std::string>> dependencies_;
};

} // namespace enchantment_tweaks::resource
