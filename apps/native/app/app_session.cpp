#include "app_session.hpp"

#include "app/catalog_source.hpp"
#include "app/model/types.h"
#include "core/api/item_catalog.hpp"
#include "core/api/item_conversion.hpp"
#include "core/api/pack_stream.hpp"
#include "core/api/shader_overlays.hpp"
#include "core/project/game_versions.hpp"
#include "core/project/minecraft_download.hpp"
#include "core/project/project_file.hpp"
#include "core/util/file_io.hpp"
#include "core/util/pack_files.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <ios>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

namespace enchantment_tweaks::native {

namespace {

fs::path settingsFile(const fs::path &dir) { return dir / "settings.json"; }

} // namespace

AppSession::AppSession(fs::path appDirectory, fs::path assetsDirectory)
    : appDirectory_(std::move(appDirectory)),
      assetsDirectory_(std::move(assetsDirectory)) {
  if (const auto text = file_io::readText(settingsFile(appDirectory_))) {
    if (auto loaded = project::settingsFromJson(*text)) appSettings_ = *loaded;
  }
}

std::string AppSession::setAppSettings(const project::AppSettings &settings) {
  appSettings_ = settings;
  try {
    fs::create_directories(appDirectory_);
    std::ofstream out(settingsFile(appDirectory_),
                      std::ios::binary | std::ios::trunc);
    if (!out) return "cannot write settings";
    out << project::settingsToJson(appSettings_);
  } catch (const std::exception &e) {
    return e.what();
  }
  return {};
}

std::string AppSession::statusLine() const {
  std::string line;
  if (!hasProject()) {
    line = "No project open";
  } else {
    line = std::string(isPackProject() ? "Pack project" : "Game project") +
           "  ·  Minecraft " + gameVersion();
    if (project_.packPath)
      line += "  ·  " + project_.packPath->filename().string();
    line += "  ·  " + std::to_string(project_.selected.size()) + " selected";
  }
  if (projectPath_) {
    line += "  ·  " + projectPath_->filename().string() + (dirty_ ? " *" : "");
  } else if (hasProject()) {
    line += "  ·  Unsaved project";
  }
  return line;
}

void AppSession::setSelection(const std::vector<std::string> &keys) {
  if (keys == project_.selected) return;
  project_.selected = keys;
  markDirty();
}

std::string AppSession::setOptions(const project::ItemOptions &options) {
  if (options.thicknessPx <= 0.0) return "thickness must be above 0";
  if (options.outlineSizePx < 0.0) return "outline size cannot be negative";
  project_.options = options;
  markDirty();
  return {};
}

std::vector<project::InstalledGame> AppSession::installedGames() const {
  std::vector<project::InstalledGame> games;
  std::vector<std::string> seen;
  for (const auto &root :
       {project::defaultMinecraftDirectory(), appDirectory_ / "minecraft"}) {
    for (auto &game : project::findInstalledGames(root)) {
      if (std::ranges::find(seen, game.id) != seen.end()) continue;
      seen.push_back(game.id);
      games.push_back(std::move(game));
    }
  }
  std::ranges::sort(games, [](const auto &a, const auto &b) {
    return catalog::naturalLess(b.id, a.id);
  });
  return games;
}

void AppSession::setSort(const std::string &sort) {
  if (sort == project_.sort) return;
  project_.sort = sort;
  markDirty();
}

et::Catalog AppSession::uiCatalog() {
  if (!catalog_) return {};
  et::Catalog out = uiCache_;
  // Pack projects can target any installed version.
  out.versions.clear();
  if (isPackProject()) {
    for (const auto &game : installedGames())
      out.versions.push_back(game.id);
  } else {
    out.versions.push_back(gameVersion());
  }
  return out;
}

std::string AppSession::saveProject() {
  if (!projectPath_) return "this project has not been saved yet";
  return saveProjectAs(*projectPath_);
}

std::string AppSession::saveProjectAs(const fs::path &file) {
  if (!hasProject()) return "open a project before saving";
  if (const auto error = project::writeProjectFile(file, project_);
      !error.empty())
    return error;
  projectPath_ = file;
  dirty_ = false;
  return {};
}

std::string AppSession::prepareNewGame(const fs::path &jar,
                                       const std::string &versionId,
                                       LoadRequest &out) const {
  out = LoadRequest{};
  out.label = "Minecraft " + versionId;
  out.target = project::Project{};
  out.target.kind = project::ProjectKind::Game;
  out.target.gameVersionId = versionId;
  out.target.gameJar = jar;
  out.clearProjectFile = true;
  out.dirtyAfter = true;
  return {};
}

std::string
AppSession::prepareNewPack(const fs::path &pack,
                           const std::vector<project::InstalledGame> &supported,
                           LoadRequest &out) const {
  if (!fs::exists(pack)) return "pack not found: " + pack.string();
  if (supported.empty()) return "install a Minecraft version first";
  out = LoadRequest{};
  out.label = "pack " + pack.filename().string();
  out.target = project::Project{};
  out.target.kind = project::ProjectKind::Pack;
  out.target.packPath = pack;
  for (const auto &game : supported)
    out.target.supportedVersions.push_back(game.id);
  out.target.gameVersionId = supported.front().id;
  out.target.gameJar = supported.front().jar;
  out.clearProjectFile = true;
  out.dirtyAfter = true;
  return {};
}

std::string AppSession::prepareOpenProject(const fs::path &file,
                                           LoadRequest &out) const {
  const auto loaded = project::readProjectFile(file);
  if (!loaded.project) return loaded.error;
  out = LoadRequest{};
  out.label = file.filename().string();
  out.target = *loaded.project;
  out.projectFile = file;
  out.dirtyAfter = false;
  return {};
}

std::string AppSession::prepareSupported(const std::vector<std::string> &ids,
                                         LoadRequest &out) const {
  if (!isPackProject()) return "only pack projects have supported versions";
  if (ids.empty()) return "a pack needs at least one supported version";
  // Use the newest installed supported version as the reference.
  std::optional<project::InstalledGame> reference;
  for (const auto &game : installedGames()) {
    if (std::ranges::find(ids, game.id) != ids.end()) {
      reference = game;
      break;
    }
  }
  if (!reference) return "none of the supported versions is installed";
  out = LoadRequest{};
  out.label = "Minecraft " + reference->id;
  out.target = project_;
  out.target.supportedVersions = ids;
  out.target.gameVersionId = reference->id;
  out.target.gameJar = reference->jar;
  out.dirtyAfter = true;
  return {};
}

std::string AppSession::prepareOpenPack(const fs::path &zip,
                                        LoadRequest &out) const {
  if (!hasProject()) return "open a game first";
  if (isPackProject()) return "this project already is a pack";
  out = LoadRequest{};
  out.label = "pack " + zip.filename().string();
  out.target = project_;
  out.target.packPath = zip;
  out.dirtyAfter = true;
  return {};
}

std::string AppSession::prepareClosePack(LoadRequest &out) const {
  if (!hasPack() || isPackProject()) return "no pack is open";
  out = LoadRequest{};
  out.label = "game";
  out.target = project_;
  out.target.packPath.reset();
  out.dirtyAfter = true;
  return {};
}

AppSession::LoadResult AppSession::performLoad(const project::Project &target,
                                               const fs::path &appDirectory) {
  // Runs on a worker thread and writes only to the icon cache.
  LoadResult result;
  if (target.gameJar.empty()) {
    result.error = "project has no game";
    return result;
  }
  try {
    result.catalog = std::make_shared<catalog::ItemCatalog>(
        catalog::ItemCatalog::load(target.gameJar, target.packPath));
    const auto icons = result.catalog->writeIcons(
        appDirectory / "icon-cache-x96" / result.catalog->gameVersion());
    result.ui = native::catalogFromItems(*result.catalog, icons);
    result.ok = true;
  } catch (const std::exception &e) {
    result.error = e.what();
  }
  return result;
}

std::string AppSession::commitLoad(const LoadRequest &request,
                                   LoadResult result) {
  if (!result.ok) return result.error;
  project_ = request.target;
  // Drop selections that are absent from the newly loaded catalog.
  std::vector<std::string> kept;
  for (const auto &key : project_.selected) {
    if (result.catalog->find(key)) kept.push_back(key);
  }
  project_.selected = std::move(kept);
  catalog_ = std::move(result.catalog);
  uiCache_ = std::move(result.ui);
  if (request.clearProjectFile) projectPath_.reset();
  if (request.projectFile) projectPath_ = request.projectFile;
  dirty_ = request.dirtyAfter;
  return {};
}

void AppSession::setOverride(const std::vector<std::string> &keys,
                             const project::ItemOverride &override) {
  for (const auto &key : keys) {
    if (override.empty()) {
      project_.overrides.erase(key);
    } else {
      project_.overrides[key] = override;
    }
  }
  markDirty();
}

void AppSession::clearOverrides(const std::vector<std::string> &keys) {
  for (const auto &key : keys)
    project_.overrides.erase(key);
  markDirty();
}

project::ItemOverride AppSession::overrideFor(const std::string &key) const {
  const auto it = project_.overrides.find(key);
  return it == project_.overrides.end() ? project::ItemOverride{} : it->second;
}

std::optional<std::pair<int, int>> AppSession::installedFormatRange() const {
  if (!isPackProject()) return std::nullopt;
  std::optional<std::pair<int, int>> range;
  for (const auto &id : project_.supportedVersions) {
    for (const auto &game : installedGames()) {
      if (game.id != id) continue;
      const auto format = formatOf(game.jar);
      if (!format) continue;
      if (!range) {
        range = std::make_pair(*format, *format);
      } else {
        range->first = std::min(range->first, *format);
        range->second = std::max(range->second, *format);
      }
    }
  }
  return range;
}

std::string AppSession::supportedRangeText() const {
  if (!isPackProject()) return {};
  const auto range = installedFormatRange();
  if (!range)
    return "No supported version is installed, so the format range is unknown.";
  return "Pack formats " + std::to_string(range->first) + " to " +
         std::to_string(range->second) +
         " (from the installed supported versions).";
}

void AppSession::closeCurrent() {
  project_ = project::Project{};
  catalog_.reset();
  uiCache_ = et::Catalog{};
  projectPath_.reset();
  dirty_ = false;
}

std::optional<int> AppSession::formatOf(const fs::path &jar) const {
  const std::string key = jar.string();
  const auto cached = formatCache_.find(key);
  if (cached != formatCache_.end()) return cached->second;
  const auto format = project::resourceFormatOfJar(jar);
  formatCache_[key] = format;
  return format;
}

conversion::ItemVariantOptions
AppSession::variantOptionsFor(const std::string &key) const {
  const project::ItemOptions merged =
      project::applyOverride(project_.options, overrideFor(key));
  conversion::ItemVariantOptions o;

  o.thicknessPx = merged.thicknessPx;
  o.outlineSizePx = merged.outlineSizePx;
  o.unenchantedOutline = merged.unenchantedOutline;
  o.unenchantedUnshaded = merged.unenchantedUnshaded;

  // Merging columns applies only to generated items.
  const auto *item = catalog_ ? catalog_->find(key) : nullptr;
  o.mergeColumns = merged.mergeColumns && item && item->generated;
  const auto channel = [&](size_t i) {
    return static_cast<uint8_t>(
        std::stoi(merged.outlineColor.substr(i, 2), nullptr, 16));
  };
  o.outlineColor = {channel(0), channel(2), channel(4)};
  return o;
}

catalog::ItemCatalog::SaveResult
AppSession::exportPack(const fs::path &directory, const std::string &fileName) {
  catalog::ItemCatalog::SaveResult result;
  if (!catalog_) {
    result.error = "open a project first";
    return result;
  }
  if (project_.selected.empty() && !isPackProject()) {
    result.error = "select at least one item to export";
    return result;
  }
  try {
    const auto destination = directory / fileName;
    const std::string description = "Enchantment Tweaks pack";

    const auto range = installedFormatRange();
    const int lowest = range ? range->first : catalog_->gameResourceFormat();
    const int highest = range ? range->second : lowest;

    conversion::PackStreamer out(destination, description, lowest);
    const catalog::ExportScope scope = catalog::ExportScope::AllModels;

    for (const auto &key : project_.selected) {
      out.addItem(catalog_->convertItem(key, variantOptionsFor(key), scope));
    }

    if (isPackProject() && project_.packPath) {
      for (const auto &file : pack_files::readPackFiles(*project_.packPath)) {
        out.addIfAbsent(file);
      }
    }
    if (!assetsDirectory_.empty()) {
      const auto shaderRoot = assetsDirectory_ / "shaders" / "overlays";
      for (const auto &overlay : conversion::loadShaderOverlays(shaderRoot)) {
        out.addOverlay(overlay, shaderRoot);
      }
    }
    if (lowest > 0 && highest >= lowest)
      out.setSupportedFormats(lowest, highest);
    out.close();

    result.saved = true;
    result.path = destination;
  } catch (const std::exception &error) {
    result.error = error.what();
  }
  return result;
}

} // namespace enchantment_tweaks::native
