#include "core/resource/resource_pack.hpp"
#include "core/types/items/item_model_definition.hpp"
#include "core/types/mesh/types.hpp"
#include "core/util/debug.hpp"
#include "core/util/image.hpp"
#include "libraries/miniz/miniz.h"
#include "libraries/miniz/miniz_common.h"
#include "libraries/miniz/miniz_zip.h"
#include "libraries/stb/stb_image.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <exception>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#define ZIP_READER_INIT_FILE mz_zip_reader_init_file
#define ZIP_READER_GET_NUM_FILES mz_zip_reader_get_num_files
#define ZIP_READER_FILE_STAT mz_zip_reader_file_stat
#define ZIP_READER_EXTRACT_TO_HEAP(zip, index, size, flags)                    \
  mz_zip_reader_extract_to_heap(&(zip), index, size, flags)
#define ZIP_READER_END(zip) mz_zip_reader_end(&(zip))
#define ZIP_FREE mz_free
using ZipArchive = mz_zip_archive;
using ZipStat = mz_zip_archive_file_stat;
#define ZIP_STAT_FILENAME(stat) (stat).m_filename
#define ZIP_UINT mz_uint

namespace enchantment_tweaks::resource {

namespace {

// Reads pack.mcmeta. Used for directory packs and zip packs alike. Newer
// metadata may omit pack_format in favor of min_format, and descriptions may be
// text objects.
void applyPackMeta(ResourcePack &pack, const json &meta) {
  const auto it = meta.find("pack");
  if (it == meta.end() || !it->is_object()) {
    return;
  }
  if (const auto d = it->find("description");
      d != it->end() && d->is_string()) {
    pack.description = d->get<std::string>();
  }
  if (const auto f = it->find("pack_format");
      f != it->end() && f->is_number_integer()) {
    pack.packFormat = f->get<int>();
  } else if (const auto m = it->find("min_format");
             m != it->end() && m->is_number_integer()) {
    pack.packFormat = m->get<int>();
  }
}

} // namespace

static bool isItemTypeBuiltin(const std::string &id) {
  if (id.starts_with("builtin/")) return true;
  if (id.starts_with("minecraft:item/")) return true;
  if (id.starts_with("minecraft:block/")) return true;
  if (id.starts_with("item/")) return true;
  if (id.starts_with("block/")) return true;
  if (id.find("generated") != std::string::npos) return true;

  return false;
}

using json = nlohmann::json;

ResourcePackLoader::ResourcePackLoader(const fs::path &packRoot) {
  pack_.rootPath = packRoot;
}

void ResourcePackLoader::setBasePack(const ResourcePackLoader &basePack) {
  basePack_ = &basePack;
}

bool ResourcePackLoader::load() {
  if (!fs::exists(pack_.rootPath)) {
    return false;
  }

  debug::Logger::info() << "Current path: " << pack_.rootPath;

  if (fs::is_directory(pack_.rootPath)) {
    return loadFromDirectory(pack_.rootPath);
  } else {
    return loadFromZip(pack_.rootPath);
  }
}

bool ResourcePackLoader::loadFromDirectory(const fs::path &dir) {
  pack_.isDirectory = true;
  pack_.name = dir.filename().string();

  fs::path packMeta = dir / "pack.mcmeta";
  if (fs::exists(packMeta)) {
    try {
      std::ifstream f(packMeta);

      const json meta = json::parse(f);
      applyPackMeta(pack_, meta);
    } catch (const std::exception &e) {
      debug::Logger::error() << "error reading file metadata: " << packMeta;
      debug::Logger::error() << "error info: " << e.what();
      throw e;
    }
  }

  fs::path assetsDir = dir / "assets";
  if (!fs::exists(assetsDir)) {
    return false;
  }

  for (const auto &nsEntry : fs::directory_iterator(assetsDir)) {
    if (!nsEntry.is_directory()) continue;
    std::string ns = nsEntry.path().filename().string();

    scanModelDef(nsEntry.path() / "items", ns);
    scanTextures(nsEntry.path() / "textures", ns);

    pack_.searchPaths.push_back(nsEntry.path());
  }

  // Crawl item definitions for referenced model ids, then load only those.
  auto modelIds = collectReferencedModelIds();
  if (!modelIds.empty()) {
    std::unordered_set<std::string> idSet(modelIds.begin(), modelIds.end());
    for (const auto &nsEntry : fs::directory_iterator(assetsDir)) {
      if (!nsEntry.is_directory()) continue;
      std::string ns = nsEntry.path().filename().string();
      fs::path modelsDir = nsEntry.path() / "models";
      if (!fs::exists(modelsDir)) continue;
      for (const auto &entry : fs::recursive_directory_iterator(modelsDir)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".json")
          continue;
        std::string name = entry.path()
                               .lexically_relative(modelsDir)
                               .replace_extension("")
                               .generic_string();
        std::string fullId = ns;
        fullId += ':';
        fullId += name;
        if (!idSet.count(fullId)) continue;
        try {
          std::ifstream f(entry.path());
          json j = json::parse(f);
          models_.push_back({.path = entry.path(),
                             .nmsp_ = ns,
                             .name = std::move(name),
                             .rawJson = std::move(j)});
        } catch (const std::exception &error) {
          debug::Logger::warning()
              << "skipped an unreadable model: " << error.what();
        }
      }
    }
  }

  for (size_t i = 0; i < models_.size(); ++i) {
    modelIndex_[models_[i].nmsp_ + ":" + models_[i].name] = i;
  }
  for (size_t i = 0; i < textures_.size(); ++i) {
    textureIndex_[textures_[i].nmsp_ + ":" + textures_[i].name] = i;
  }

  return true;
}

bool ResourcePackLoader::loadFromZip(const fs::path &zipPath) {
  pack_.isDirectory = false;
  pack_.name = zipPath.stem().string();

  // Read into memory, MINIZ_NO_STDIO disables standard I/O (emscripten
  // shenanigans)
  std::ifstream file(zipPath, std::ios::binary | std::ios::ate);
  if (!file) {
    debug::Logger::error() << "failed to open zip file: " << zipPath;
    return false;
  }
  std::streamsize size = file.tellg();
  file.seekg(0, std::ios::beg);
  std::vector<uint8_t> buffer(size);
  if (!file.read(reinterpret_cast<char *>(buffer.data()), size)) {
    debug::Logger::error() << "failed to read zipfile: " << zipPath;
    return false;
  }

  mz_zip_archive zip;
  memset(&zip, 0, sizeof(zip));

  if (!mz_zip_reader_init_mem(&zip, buffer.data(), static_cast<size_t>(size),
                              0)) {
    debug::Logger::error() << "failed to init zip reader (miniz): " << zipPath;
    return false;
  }

  ZIP_UINT numEntries = mz_zip_reader_get_num_files(&zip);

  for (ZIP_UINT i = 0; i < numEntries; ++i) {
    ZipStat stat;
    if (!mz_zip_reader_file_stat(&zip, i, &stat)) continue;

    std::string name(ZIP_STAT_FILENAME(stat));
    if (name.empty() || name.back() == '/')
      continue; // is nothing or is a directory

    if (name == "version.json") {
      size_t versionSize = 0;
      void *versionData = ZIP_READER_EXTRACT_TO_HEAP(zip, i, &versionSize, 0);
      if (versionData) {
        try {
          const json version =
              json::parse(static_cast<const char *>(versionData),
                          static_cast<const char *>(versionData) + versionSize);
          if (const auto id = version.find("id");
              id != version.end() && id->is_string()) {
            pack_.gameVersion = id->get<std::string>();
          }
          // 1.21.x writes pack_version.resource; 26.x writes resource_major.
          if (const auto pv = version.find("pack_version");
              pv != version.end() && pv->is_object()) {
            if (const auto r = pv->find("resource");
                r != pv->end() && r->is_number_integer()) {
              pack_.gameResourceFormat = r->get<int>();
            } else if (const auto rm = pv->find("resource_major");
                       rm != pv->end() && rm->is_number_integer()) {
              pack_.gameResourceFormat = rm->get<int>();
            }
          }
        } catch (const std::exception &e) {
          debug::Logger::error() << "error reading version.json in " << zipPath
                                 << ": " << e.what();
        }
        ZIP_FREE(versionData);
      }
      continue;
    }

    if (name == "pack.mcmeta") {
      size_t metaSize = 0;
      void *metaData = ZIP_READER_EXTRACT_TO_HEAP(zip, i, &metaSize, 0);
      if (metaData) {
        try {
          const json meta =
              json::parse(static_cast<const char *>(metaData),
                          static_cast<const char *>(metaData) + metaSize);
          applyPackMeta(pack_, meta);
        } catch (const std::exception &e) {
          debug::Logger::error()
              << "error reading pack.mcmeta in " << zipPath << ": " << e.what();
        }
        ZIP_FREE(metaData);
      }
      continue;
    }

    if (!name.starts_with("assets/")) continue; // bandaid

    std::vector<std::string> parts;
    std::string part;
    for (char c : name) {
      if (c == '/') {
        parts.push_back(part);
        part.clear();
      } else {
        part += c;
      }
    }
    if (!part.empty()) parts.push_back(part);

    if (parts.size() < 4) continue;

    std::string ns = parts[1];
    std::string type = parts[2];

    if (type == "items" && name.size() > 5 &&
        name.substr(name.size() - 5) == ".json") {
      size_t extractedSize = 0;
      void *data = ZIP_READER_EXTRACT_TO_HEAP(zip, i, &extractedSize, 0);
      if (data) {
        try {
          json j = json::parse(static_cast<const char *>(data),
                               static_cast<const char *>(data) + extractedSize);
          ZIP_FREE(data);

          std::string itemName = parts[3];
          if (itemName.size() > 5 &&
              itemName.substr(itemName.size() - 5) == ".json") {
            itemName.resize(itemName.size() - 5);
          }
          for (size_t k = 4; k < parts.size(); ++k) {
            std::string p = parts[k];
            if (p.size() > 5 && p.substr(p.size() - 5) == ".json") {
              p.resize(p.size() - 5);
            }
            itemName += "/" + p;
          }
          def_.push_back({.path = fs::path(name),
                          .nmsp = ns,
                          .name = itemName,
                          .rawJson = std::move(j)});
        } catch (...) {
          ZIP_FREE(data);
        }
      }
    } else if (type == "models" && name.size() > 5 &&
               name.substr(name.size() - 5) == ".json") {
      continue;
    } else if (type == "textures" && name.size() > 4 &&
               name.substr(name.size() - 4) == ".png") {
      size_t size = 0;
      void *data = ZIP_READER_EXTRACT_TO_HEAP(zip, i, &size, 0);
      if (data) {
        try {
          int w = 0, h = 0, ch = 0;
          unsigned char *pixels =
              stbi_load_from_memory(static_cast<const unsigned char *>(data),
                                    static_cast<int>(size), &w, &h, &ch, 4);
          ZIP_FREE(data);

          using std::ptrdiff_t;

          if (pixels) {
            std::vector<uint8_t> pixelVec(pixels,
                                          pixels + (ptrdiff_t)w * h * 4);
            stbi_image_free(pixels);

            std::string texName = parts[3];
            if (texName.size() > 4 &&
                texName.substr(texName.size() - 4) == ".png") {
              texName = texName.substr(0, texName.size() - 4);
            }
            for (size_t k = 4; k < parts.size(); ++k) {
              std::string p = parts[k];
              if (p.size() > 4 && p.substr(p.size() - 4) == ".png") {
                p = p.substr(0, p.size() - 4);
              }
              texName += "/" + p;
            }
            textures_.push_back({.path = fs::path(name),
                                 .nmsp_ = ns,
                                 .name = texName,
                                 .pixels = std::move(pixelVec),
                                 .width = w,
                                 .height = h});
          }
        } catch (const std::exception &e) {
          debug::Logger::error() << "error reading resources: " << zipPath;
          debug::Logger::error() << "error info: " << e.what();
          throw e;
        }
      }
    }
  }

  ZIP_READER_END(zip);

  // Crawl item definitions for referenced model IDs, then load only those.
  auto modelIds = collectReferencedModelIds();
  if (!modelIds.empty()) {
    // Re-open the zip for a second pass to load only referenced models.
    std::ifstream file2(zipPath, std::ios::binary | std::ios::ate);
    if (file2) {
      std::streamsize size2 = file2.tellg();
      file2.seekg(0, std::ios::beg);
      std::vector<uint8_t> buffer2(size2);
      if (file2.read(reinterpret_cast<char *>(buffer2.data()), size2)) {
        mz_zip_archive zip2;
        memset(&zip2, 0, sizeof(zip2));
        if (mz_zip_reader_init_mem(&zip2, buffer2.data(),
                                   static_cast<size_t>(size2), 0)) {
          ZIP_UINT numEntries2 = mz_zip_reader_get_num_files(&zip2);
          std::unordered_set<std::string> idSet(modelIds.begin(),
                                                modelIds.end());
          for (ZIP_UINT i = 0; i < numEntries2; ++i) {
            ZipStat stat;
            if (!mz_zip_reader_file_stat(&zip2, i, &stat)) continue;
            std::string name(ZIP_STAT_FILENAME(stat));
            if (name.empty() || name.back() == '/') continue;
            if (!name.starts_with("assets/")) continue;
            std::vector<std::string> parts;
            std::string part;
            for (char c : name) {
              if (c == '/') {
                parts.push_back(part);
                part.clear();
              } else {
                part += c;
              }
            }
            if (!part.empty()) parts.push_back(part);
            if (parts.size() < 4) continue;
            std::string type = parts[2];
            if (type != "models") continue;
            if (name.size() <= 5 || name.substr(name.size() - 5) != ".json")
              continue;
            std::string modelName = parts[3];
            if (modelName.size() > 5 &&
                modelName.substr(modelName.size() - 5) == ".json") {
              modelName = modelName.substr(0, modelName.size() - 5);
            }
            for (size_t k = 4; k < parts.size(); ++k) {
              std::string p = parts[k];
              if (p.size() > 5 && p.substr(p.size() - 5) == ".json") {
                p = p.substr(0, p.size() - 5);
              }
              modelName += "/" + p;
            }
            std::string fullId = parts[1] + ":" + modelName;
            if (!idSet.count(fullId)) continue;
            size_t extractedSize = 0;
            void *data = ZIP_READER_EXTRACT_TO_HEAP(zip2, i, &extractedSize, 0);
            if (data) {
              try {
                json j = json::parse(static_cast<const char *>(data),
                                     static_cast<const char *>(data) +
                                         extractedSize);
                ZIP_FREE(data);
                models_.push_back({.path = fs::path(name),
                                   .nmsp_ = parts[1],
                                   .name = modelName,
                                   .rawJson = std::move(j)});
              } catch (...) {
                ZIP_FREE(data);
              }
            }
          }
          ZIP_READER_END(zip2);
        }
      }
    }
  }

  for (size_t i = 0; i < models_.size(); ++i) {
    modelIndex_[models_[i].nmsp_ + ":" + models_[i].name] = i;
  }
  for (size_t i = 0; i < textures_.size(); ++i) {
    textureIndex_[textures_[i].nmsp_ + ":" + textures_[i].name] = i;
  }

  return true;
}

void ResourcePackLoader::scanModelDef(const fs::path &dir,
                                      const std::string &nmsp) {

  if (!fs::exists(dir)) return;

  for (const auto &entry : fs::recursive_directory_iterator(dir)) {
    if (!entry.is_regular_file() || entry.path().extension() != ".json")
      continue;

    try {
      std::ifstream f(entry.path());
      json j = json::parse(f);

      std::string name = entry.path()
                             .lexically_relative(dir)
                             .replace_extension("")
                             .generic_string();

      def_.push_back({.path = entry.path(),
                      .nmsp = nmsp,
                      .name = name,
                      .rawJson = std::move(j)});
    } catch (const std::exception &e) {
      debug::Logger::error() << "exception scanning item model definition"
                             << dir << " namespace " << nmsp;
      debug::Logger::error() << "error info: " << e.what();
      throw e;
    }
  }
}

void ResourcePackLoader::scanTextures(const fs::path &texturesDir,
                                      const std::string &nmsp) {
  if (!fs::exists(texturesDir)) return;

  for (const auto &entry : fs::recursive_directory_iterator(texturesDir)) {
    if (!entry.is_regular_file() || entry.path().extension() != ".png")
      continue;

    try {
      image::Image img = image::Image::load(entry.path().string());
      if (!img.empty()) {
        std::string name = entry.path()
                               .lexically_relative(texturesDir)
                               .replace_extension("")
                               .generic_string();
        textures_.push_back({.path = entry.path(),
                             .nmsp_ = nmsp,
                             .name = name,
                             .pixels = img.pixels(),
                             .width = img.width(),
                             .height = img.height()});
      }
    } catch (const std::exception &error) {
      debug::Logger::warning()
          << "skipped an unreadable texture: " << error.what();
    }
  }
}

std::vector<std::string> ResourcePackLoader::collectReferencedModelIds() const {
  std::unordered_set<std::string> refs;
  for (const auto &entry : def_) {
    collectModelRefsFromDef(entry.rawJson, refs);
  }
  return {refs.begin(), refs.end()};
}

void ResourcePackLoader::collectModelRefsFromDef(
    const json &defJson, std::unordered_set<std::string> &refs) const {
  if (!defJson.is_object()) return;
  if (!defJson.contains("model") || !defJson["model"].is_object()) return;
  collectModelRefsFromItemModel(defJson.at("model"), refs);
}

static std::string normalizeModelId(std::string id) {
  if (id.find(':') == std::string::npos) {
    id = "minecraft:" + id;
  }
  return id;
}

void ResourcePackLoader::collectModelRefsFromItemModel(
    const json &modelJson, std::unordered_set<std::string> &refs) {
  if (!modelJson.is_object() || !modelJson.contains("type") ||
      !modelJson["type"].is_string())
    return;

  std::string type = modelJson["type"].get<std::string>();

  if (type == "minecraft:model") {
    if (modelJson.contains("model") && modelJson["model"].is_string()) {
      refs.insert(normalizeModelId(modelJson["model"].get<std::string>()));
    }
  } else if (type == "minecraft:special") {
    if (modelJson.contains("base") && modelJson["base"].is_string()) {
      refs.insert(normalizeModelId(modelJson["base"].get<std::string>()));
    }
    if (modelJson.contains("model") && modelJson["model"].is_object()) {
      collectModelRefsFromItemModel(modelJson["model"], refs);
    }
  } else if (type == "minecraft:condition") {
    if (modelJson.contains("on_true") && modelJson["on_true"].is_object())
      collectModelRefsFromItemModel(modelJson["on_true"], refs);
    if (modelJson.contains("on_false") && modelJson["on_false"].is_object())
      collectModelRefsFromItemModel(modelJson["on_false"], refs);
  } else if (type == "minecraft:select") {
    if (modelJson.contains("cases") && modelJson["cases"].is_array()) {
      for (const auto &c : modelJson["cases"]) {
        if (c.contains("model") && c["model"].is_object())
          collectModelRefsFromItemModel(c["model"], refs);
      }
    }
    if (modelJson.contains("fallback") && modelJson["fallback"].is_object())
      collectModelRefsFromItemModel(modelJson["fallback"], refs);
  } else if (type == "minecraft:range_dispatch") {
    if (modelJson.contains("entries") && modelJson["entries"].is_array()) {
      for (const auto &e : modelJson["entries"]) {
        if (e.contains("model") && e["model"].is_object())
          collectModelRefsFromItemModel(e["model"], refs);
      }
    }
    if (modelJson.contains("fallback") && modelJson["fallback"].is_object())
      collectModelRefsFromItemModel(modelJson["fallback"], refs);
  } else if (type == "minecraft:composite") {
    if (modelJson.contains("models") && modelJson["models"].is_array()) {
      for (const auto &m : modelJson["models"]) {
        if (m.is_object()) collectModelRefsFromItemModel(m, refs);
      }
    }
  }
  // minecraft:empty, minecraft:bundle/selected_item: no refs
}

void ResourcePackLoader::loadModels(const std::vector<std::string> &modelIds) {
  if (modelIds.empty()) return;
  std::unordered_set<std::string> idSet(modelIds.begin(), modelIds.end());

  if (pack_.isDirectory) {
    for (const auto &nsEntry :
         fs::directory_iterator(pack_.rootPath / "assets")) {
      if (!nsEntry.is_directory()) continue;
      std::string ns = nsEntry.path().filename().string();
      fs::path modelsDir = nsEntry.path() / "models";
      if (!fs::exists(modelsDir)) continue;
      for (const auto &entry : fs::recursive_directory_iterator(modelsDir)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".json")
          continue;
        std::string name = entry.path()
                               .lexically_relative(modelsDir)
                               .replace_extension("")
                               .generic_string();
        std::string fullId = ns;
        fullId += ':';
        fullId += name;
        if (!idSet.count(fullId)) continue;
        // Avoid duplicates
        if (modelIndex_.count(fullId)) continue;
        try {
          std::ifstream f(entry.path());
          json j = json::parse(f);
          models_.push_back({.path = entry.path(),
                             .nmsp_ = ns,
                             .name = std::move(name),
                             .rawJson = std::move(j)});
        } catch (const std::exception &error) {
          debug::Logger::warning()
              << "skipped an unreadable model: " << error.what();
        }
      }
    }
  } else {
    // ZIP: re-open and scan for matching models
    std::ifstream file(pack_.rootPath, std::ios::binary | std::ios::ate);
    if (!file) return;
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<uint8_t> buffer(size);
    if (!file.read(reinterpret_cast<char *>(buffer.data()), size)) return;

    mz_zip_archive zip;
    memset(&zip, 0, sizeof(zip));
    if (!mz_zip_reader_init_mem(&zip, buffer.data(), static_cast<size_t>(size),
                                0))
      return;

    ZIP_UINT numEntries = mz_zip_reader_get_num_files(&zip);
    for (ZIP_UINT i = 0; i < numEntries; ++i) {
      ZipStat stat;
      if (!mz_zip_reader_file_stat(&zip, i, &stat)) continue;
      std::string name(ZIP_STAT_FILENAME(stat));
      if (name.empty() || name.back() == '/') continue;
      if (!name.starts_with("assets/")) continue;
      std::vector<std::string> parts;
      std::string part;
      for (char c : name) {
        if (c == '/') {
          parts.push_back(part);
          part.clear();
        } else {
          part += c;
        }
      }
      if (!part.empty()) parts.push_back(part);
      if (parts.size() < 4) continue;
      std::string type = parts[2];
      if (type != "models") continue;
      if (name.size() <= 5 || name.substr(name.size() - 5) != ".json") continue;
      std::string modelName = parts[3];
      if (modelName.size() > 5 &&
          modelName.substr(modelName.size() - 5) == ".json") {
        modelName = modelName.substr(0, modelName.size() - 5);
      }
      for (size_t k = 4; k < parts.size(); ++k) {
        std::string p = parts[k];
        if (p.size() > 5 && p.substr(p.size() - 5) == ".json") {
          p = p.substr(0, p.size() - 5);
        }
        modelName += "/" + p;
      }
      std::string fullId = parts[1] + ":" + modelName;
      if (!idSet.count(fullId)) continue;
      if (modelIndex_.count(fullId)) continue;
      size_t extractedSize = 0;
      void *data = ZIP_READER_EXTRACT_TO_HEAP(zip, i, &extractedSize, 0);
      if (data) {
        try {
          json j = json::parse(static_cast<const char *>(data),
                               static_cast<const char *>(data) + extractedSize);
          ZIP_FREE(data);
          models_.push_back({.path = fs::path(name),
                             .nmsp_ = parts[1],
                             .name = modelName,
                             .rawJson = std::move(j)});
        } catch (...) {
          ZIP_FREE(data);
        }
      }
    }
    ZIP_READER_END(zip);
  }

  // Fall back to base pack for any models not found in this pack.
  if (basePack_) {
    for (const auto &id : modelIds) {
      if (modelIndex_.count(id)) continue;
      if (auto j = basePack_->getModelJson(id)) {
        models_.push_back({fs::path{}, id.substr(0, id.find(':')),
                           id.substr(id.find(':') + 1), *j});
        modelIndex_[id] = models_.size() - 1;
      }
    }
  }
}

std::optional<json>
ResourcePackLoader::getModelJson(const std::string &nmsp,
                                 const std::string &name) const {
  auto it = modelIndex_.find(nmsp + ":" + name);
  if (it != modelIndex_.end()) {
    return models_[it->second].rawJson;
  }
  if (basePack_) {
    return basePack_->getModelJson(nmsp, name);
  }
  return std::nullopt;
}

std::optional<json>
ResourcePackLoader::getModelJson(const std::string &fullId) const {
  auto it = modelIndex_.find(fullId);
  if (it != modelIndex_.end()) {
    return models_[it->second].rawJson;
  }
  if (basePack_) {
    return basePack_->getModelJson(fullId);
  }
  return std::nullopt;
}

std::optional<json>
ResourcePackLoader::getItemDefinitionJson(const std::string &itemId) const {
  std::string namespacedId = itemId;
  if (namespacedId.find(':') == std::string::npos) {
    namespacedId = "minecraft:" + namespacedId;
  }

  for (const auto &entry : def_) {
    if (entry.nmsp + ":" + entry.name == namespacedId) {
      return entry.rawJson;
    }
  }
  // a pack that only changes textures has no definitions of its own uses the
  // referenced jar''s one
  if (basePack_) {
    return basePack_->getItemDefinitionJson(itemId);
  }
  return std::nullopt;
}

std::optional<std::vector<uint8_t>>
ResourcePackLoader::getTexturePixels(const std::string &nmsp_,
                                     const std::string &name) const {
  auto it = textureIndex_.find(nmsp_ + ":" + name);
  if (it != textureIndex_.end()) {
    return textures_[it->second].pixels;
  }
  if (basePack_) {
    return basePack_->getTexturePixels(nmsp_, name);
  }
  return std::nullopt;
}

ModelResolver::ModelResolver(const ResourcePackLoader &loader)
    : loader_(loader) {}

mesh::Model ModelResolver::resolve(const std::string &modelId) const {
  std::optional<json> modelJson;
  if (modelId.find(':') != std::string::npos) {
    modelJson = loader_.getModelJson(modelId);
  } else {
    modelJson = loader_.getModelJson("minecraft", modelId);
  }

  if (!modelJson) {
    throw std::runtime_error("Model not found: " + modelId);
  }

  std::unordered_set<std::string> visited;
  return resolveModelRecursive(*modelJson, visited);
}

mesh::Model ModelResolver::resolve(const json &modelJson) const {
  std::unordered_set<std::string> visited;
  return resolveModelRecursive(modelJson, visited);
}

std::unique_ptr<item::ItemModelDefinition>
ModelResolver::resolveItemDefinition(const std::string &itemId) const {
  std::optional<json> itemJson = loader_.getItemDefinitionJson(itemId);
  if (!itemJson) {
    throw std::runtime_error("Item definition not found: " + itemId);
  }
  return resolveItemDefinition(*itemJson);
}

std::unique_ptr<item::ItemModelDefinition>
ModelResolver::resolveItemDefinition(const json &itemJson) const {
  auto def = std::make_unique<item::ItemModelDefinition>();
  def->deserialize(itemJson);
  return def;
}

void ModelResolver::setTextureResolver(
    std::function<
        std::optional<enchantment_tweaks::image::Image>(const std::string &)>
        resolver) {
  textureResolver_ = std::move(resolver);
}

mesh::Model ModelResolver::resolveModelRecursive(
    const json &modelJson, std::unordered_set<std::string> &visited) const {
  json resolved = resolveParent(modelJson, visited);
  resolved = resolveTextures(resolved);
  resolved = resolveModel(resolved);

  mesh::Model model;
  model.deserialize(resolved);
  return model;
}

json ModelResolver::resolveParent(
    const json &modelJson, std::unordered_set<std::string> &visited) const {
  json result = modelJson;

  if (!result.contains("parent")) return result;

  std::string parentId = result["parent"].get<std::string>();
  if (visited.contains(parentId)) {
    throw std::runtime_error("Circular parent reference: " + parentId);
  }
  visited.insert(parentId);

  if (isItemTypeBuiltin(parentId)) {
    return result; // this is now the artificial root, the direct
                   // children of a (physically non-existent) root
  }

  std::optional<json> parentJson;
  if (parentId.find(':') != std::string::npos) {
    parentJson = loader_.getModelJson(parentId);
  } else {
    parentJson = loader_.getModelJson("minecraft", parentId);
  }

  if (!parentJson) {
    throw std::runtime_error("Parent model not found: " + parentId);
  }

  json parentResolved = resolveParent(*parentJson, visited);
  parentResolved = resolveTextures(parentResolved);
  parentResolved = resolveModel(parentResolved);

  for (auto &[key, value] : result.items()) {
    if (key != "parent") {
      parentResolved[key] = value;
    }
  }

  return parentResolved;
}

json ModelResolver::resolveTextures(const json &modelJson) const {
  json result = modelJson;

  if (!result.contains("textures") || !result["textures"].is_object())
    return result;

  json textures = result["textures"];
  for (auto &[key, value] : textures.items()) {
    if (value.is_string()) {
      std::string texId = value.get<std::string>();
      if (texId.starts_with('#')) {
        continue;
      }
      if (texId.find(':') == std::string::npos) {
        textures[key] = "minecraft:" + texId;
      }
    }
  }
  result["textures"] = std::move(textures);
  return result;
}

json ModelResolver::resolveModel(const json &modelJson) const {
  return modelJson;
}

ModelTree::ModelTree(const ResourcePackLoader &loader)
    : loader_(loader), resolver_(loader) {}

void ModelTree::build() {
  discoverModels();
  discoverItemDefinitions();
  resolveDependencies();
}

void ModelTree::discoverModels() {
  for (const auto &entry : loader_.getModels()) {
    std::string id = entry.nmsp_ + ":" + entry.name;
    try {
      mesh::Model model = resolver_.resolve(entry.rawJson);
      models_[id] = std::move(model);
      debug::Logger::debug() << "found model " << id;
    } catch (const std::exception &e) {
      debug::Logger::error()
          << "Failed to resolve model " << id << ": " << e.what() << "\n";
    }
  }
}

void ModelTree::discoverItemDefinitions() {
  for (const auto &entry : loader_.getDef()) {
    if (entry.rawJson.contains("type") && entry.rawJson["type"].is_string()) {
      std::string type = entry.rawJson["type"].get<std::string>();
      const static std::array<std::string, 8> excluded = {
          "minecraft:model",
          "minecraft:composite",
          "minecraft:condition",
          "minecraft:select",
          "minecraft:range_dispatch",
          "minecraft:empty",
          "minecraft:bundle/selected_item",
          "minecraft:special"};

      if (std::ranges::all_of(
              excluded, [&](const std::string &s) { return type != s; })) {
        debug::Logger::debug() << "item def type filtered: " << type;
        continue;
      }
    }

    if (entry.rawJson.is_object() &&
        (entry.rawJson.contains("model") ||
         entry.rawJson.contains("hand_animation_on_swap") ||
         entry.rawJson.contains("oversized_in_gui") ||
         entry.rawJson.contains("swap_animation_scale"))) {
      std::string id = entry.nmsp + ":" + entry.name;
      try {
        auto def = resolver_.resolveItemDefinition(entry.rawJson);
        if (def) {
          itemDefs_[id] = std::move(*def);
        }
        debug::Logger::debug() << "found item def: " << id;
      } catch (const std::exception &e) {
        debug::Logger::error() << "failed to resolve item definition " << id
                               << ": " << e.what() << "\n";
      }
    }
  }
}

void ModelTree::resolveDependencies() {
  for (const auto &[id, model] : models_) {
    if (model.parent.has_value()) {
      dependencies_[id].push_back(*model.parent);
    }
    for (const auto &[texKey, texId] : model.textures) {
      if (texId.starts_with('#')) continue; // texture reference, not a location
      if (texId.empty()) continue;
      std::string dep = texId;
      if (dep.find("model") != std::string::npos) {
        dependencies_[id].push_back(dep);
      }
    }
  }
}

const mesh::Model *ModelTree::getModel(const std::string &id) const {
  auto it = models_.find(id);
  return it != models_.end() ? &it->second : nullptr;
}

const item::ItemModelDefinition *
ModelTree::getItemDefinition(const std::string &id) const {
  auto it = itemDefs_.find(id);
  return it != itemDefs_.end() ? &it->second : nullptr;
}

std::vector<std::string> ModelTree::getAllModelIds() const {
  std::vector<std::string> ids;
  ids.reserve(models_.size());
  for (const auto &[id, _] : models_) {
    ids.push_back(id);
  }
  return ids;
}

std::vector<std::string> ModelTree::getAllItemDefinitionIds() const {
  std::vector<std::string> ids;
  ids.reserve(itemDefs_.size());
  for (const auto &[id, _] : itemDefs_) {
    ids.push_back(id);
  }
  return ids;
}

} // namespace enchantment_tweaks::resource

namespace enchantment_tweaks::resource {

bool ResourcePackLoader::unpack(const fs::path &outDir) const {
  if (!fs::exists(pack_.rootPath)) {
    return false;
  }
  if (pack_.isDirectory) {
    try {
      fs::create_directories(outDir);
      for (const auto &entry :
           fs::recursive_directory_iterator(pack_.rootPath)) {
        if (entry.is_regular_file()) {
          fs::path rel = fs::relative(entry.path(), pack_.rootPath);
          fs::path dest = outDir / rel;
          fs::create_directories(dest.parent_path());
          fs::copy_file(entry.path(), dest,
                        fs::copy_options::overwrite_existing);
        }
      }
      return true;
    } catch (...) {
      return false;
    }
  }

  // Zip/JAR
  std::ifstream file(pack_.rootPath, std::ios::binary | std::ios::ate);
  if (!file) {
    return false;
  }
  std::streamsize size = file.tellg();
  file.seekg(0, std::ios::beg);
  std::vector<uint8_t> buffer(static_cast<size_t>(size));
  if (!file.read(reinterpret_cast<char *>(buffer.data()), size)) {
    return false;
  }

  mz_zip_archive zip;
  memset(&zip, 0, sizeof(zip));
  if (!mz_zip_reader_init_mem(&zip, buffer.data(), static_cast<size_t>(size),
                              0)) {
    return false;
  }

  ZIP_UINT numEntries = mz_zip_reader_get_num_files(&zip);
  try {
    fs::create_directories(outDir);
    for (ZIP_UINT i = 0; i < numEntries; ++i) {
      ZipStat stat;
      if (!mz_zip_reader_file_stat(&zip, i, &stat)) {
        continue;
      }
      std::string name(ZIP_STAT_FILENAME(stat));
      if (name.empty()) {
        continue;
      }
      if (name.back() == '/') {
        fs::create_directories(outDir / name);
        continue;
      }
      size_t extractedSize = 0;
      void *data = ZIP_READER_EXTRACT_TO_HEAP(zip, i, &extractedSize, 0);
      if (!data) {
        continue;
      }
      fs::path dest = outDir / name;
      fs::create_directories(dest.parent_path());
      std::ofstream out(dest, std::ios::binary);
      if (out) {
        out.write(static_cast<const char *>(data),
                  static_cast<std::streamsize>(extractedSize));
        out.close();
      }
      ZIP_FREE(data);
    }
    ZIP_READER_END(zip);
    return true;
  } catch (...) {
    ZIP_READER_END(zip);
    return false;
  }
}
} // namespace enchantment_tweaks::resource
