#pragma once

#include "catalog_source.hpp"
#include "core/api/item_catalog.hpp"
#include "core/project/game_versions.hpp"
#include "core/project/project_file.hpp"
#include "model/types.h"

#include <filesystem>
#include <map>
#include <utility>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace enchantment_tweaks::native {
namespace fs = std::filesystem;


// Application state and actions used by the UI.
class AppSession {
public:
  // `assetsDirectory` points to shader overlays; empty disables them.
  explicit AppSession(fs::path appDirectory,
                      fs::path assetsDirectory = {});

  [[nodiscard]] const project::AppSettings &appSettings() const noexcept {
    return appSettings_;
  }
  std::string setAppSettings(const project::AppSettings &settings);

  [[nodiscard]] bool hasProject() const noexcept { return catalog_ != nullptr; }
  [[nodiscard]] bool hasGame() const noexcept {
    return catalog_ != nullptr;
  } // kept for readability
  [[nodiscard]] bool isPackProject() const noexcept {
    return project_.kind == project::ProjectKind::Pack;
  }
  [[nodiscard]] bool hasPack() const noexcept {
    return project_.packPath.has_value();
  }
  [[nodiscard]] bool hasProjectFile() const noexcept {
    return projectPath_.has_value();
  }
  [[nodiscard]] bool dirty() const noexcept { return dirty_; }
  [[nodiscard]] const project::Project &project() const noexcept {
    return project_;
  }
  [[nodiscard]] const std::optional<fs::path> &
  projectPath() const noexcept {
    return projectPath_;
  }
  [[nodiscard]] std::string gameVersion() const {
    return catalog_ ? catalog_->gameVersion() : std::string();
  }
  [[nodiscard]] std::string statusLine() const;

  [[nodiscard]] const std::vector<std::string> &selection() const noexcept {
    return project_.selected;
  }
  void setSelection(const std::vector<std::string> &keys);

  [[nodiscard]] const project::ItemOptions &options() const noexcept {
    return project_.options;
  }
  std::string setOptions(const project::ItemOptions &options);

  [[nodiscard]] std::vector<project::InstalledGame> installedGames() const;

  [[nodiscard]] const std::vector<std::string> &
  supportedVersions() const noexcept {
    return project_.supportedVersions;
  }

  [[nodiscard]] const std::string &sort() const noexcept {
    return project_.sort;
  }
  void setSort(const std::string &sort);
  [[nodiscard]] fs::path downloadedVersionsRoot() const {
    return appDirectory_ / "minecraft" / "versions";
  }

  [[nodiscard]] et::Catalog uiCatalog();

  // Prepare on the UI thread, perform on a worker, and commit on the UI thread.
  struct LoadRequest {
    std::string label;       // shown while loading
    project::Project target; // the project as it will be
    std::optional<fs::path>
        projectFile;               // set when a project file was opened
    bool clearProjectFile = false; // a new project has no file yet
    bool dirtyAfter = false;
  };
  struct LoadResult {
    bool ok = false;
    std::string error;
    std::shared_ptr<catalog::ItemCatalog> catalog;
    et::Catalog ui;
  };
  std::string prepareNewGame(const fs::path &jar,
                             const std::string &versionId,
                             LoadRequest &out) const;
  std::string
  prepareNewPack(const fs::path &pack,
                 const std::vector<project::InstalledGame> &supported,
                 LoadRequest &out) const;
  std::string prepareOpenProject(const fs::path &file,
                                 LoadRequest &out) const;
  std::string prepareSupported(const std::vector<std::string> &ids,
                               LoadRequest &out) const;
  std::string prepareOpenPack(const fs::path &zip,
                              LoadRequest &out) const;
  std::string prepareClosePack(LoadRequest &out) const;
  static LoadResult performLoad(const project::Project &target,
                                const fs::path &appDirectory);
  std::string commitLoad(const LoadRequest &request, LoadResult result);

  std::string saveProject(); // Fails if no project file is set.
  std::string saveProjectAs(const fs::path &file);

  // An empty override removes the item's saved overrides.
  void setOverride(const std::vector<std::string> &keys,
                   const project::ItemOverride &override);
  void clearOverrides(const std::vector<std::string> &keys);
  [[nodiscard]] project::ItemOverride overrideFor(const std::string &key) const;

  // Format range for installed supported versions, or empty if unknown.
  [[nodiscard]] std::string supportedRangeText() const;

  // Lowest and highest known format among installed supported versions.
  [[nodiscard]] std::optional<std::pair<int, int>> installedFormatRange() const;

  // Closes the overlay pack, or the project if no pack is open.
  void closeCurrent();
  // Reads and caches the target format for a game JAR.
  [[nodiscard]] std::optional<int>
  formatOf(const fs::path &jar) const;

  // Writes selected items for a game project or all changes for a pack project.
  catalog::ItemCatalog::SaveResult
  exportPack(const fs::path &directory,
             const std::string &fileName);

private:
  conversion::ItemVariantOptions
  variantOptionsFor(const std::string &key) const;
  void markDirty() noexcept { dirty_ = true; }

  fs::path appDirectory_;
  fs::path assetsDirectory_;
  project::AppSettings appSettings_;
  project::Project project_;
  std::optional<fs::path> projectPath_;
  std::shared_ptr<catalog::ItemCatalog> catalog_;
  bool dirty_ = false;
  mutable std::map<std::string, std::optional<int>> formatCache_;
  et::Catalog
      uiCache_; // the grid's catalog, built by the worker that loaded the game
};

} // namespace enchantment_tweaks::native
