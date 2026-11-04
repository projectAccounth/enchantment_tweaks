#pragma once

#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace enchantment_tweaks::project {
namespace fs = std::filesystem;

enum class ProjectKind : uint8_t { Game, Pack };

struct ItemOptions {
  double thicknessPx = 1.0;
  double outlineSizePx = 0.0;
  std::string outlineColor = "ffffff";
  bool unenchantedOutline = false;
  bool unenchantedUnshaded = false;
  bool mergeColumns = false;
};

struct AppSettings {
  bool darkMode = false;
  std::string displayMode = "rotating";
  std::string groupMode = "none";
};

struct ItemOverride {
  std::optional<double> thicknessPx;
  std::optional<double> outlineSizePx;
  std::optional<std::string> outlineColor; // "rrggbb"
  std::optional<bool> unenchantedOutline;
  std::optional<bool> unenchantedUnshaded;
  std::optional<bool> mergeColumns;

  [[nodiscard]] bool empty() const {
    return !thicknessPx && !outlineSizePx && !outlineColor &&
           !unenchantedOutline && !unenchantedUnshaded && !mergeColumns;
  }
};

/// Returns options with per-item values applied.
ItemOptions applyOverride(const ItemOptions &base,
                          const ItemOverride &override);

struct Project {
  static constexpr int kFormatVersion = 1;

  ProjectKind kind = ProjectKind::Game;
  std::string gameVersionId; // e.g. "1.21.4"
  fs::path gameJar;          // client JAR or directory
  std::optional<fs::path> packPath;
  std::vector<std::string> selected;
  std::vector<std::string> supportedVersions;
  std::string sort = "name-asc";
  ItemOptions options;
  /// Per-item overrides keyed by "namespace:id".
  std::map<std::string, ItemOverride> overrides;
};

struct ProjectLoadResult {
  std::optional<Project> project;
  std::string error; // empty when the project loaded
};

std::string projectToJson(const Project &project);
ProjectLoadResult projectFromJson(const std::string &text);
ProjectLoadResult readProjectFile(const fs::path &path);
/// Writes atomically; returns an empty string on success or an error message.
std::string writeProjectFile(const fs::path &path, const Project &project);

std::string settingsToJson(const AppSettings &settings);
std::optional<AppSettings> settingsFromJson(const std::string &text);

} // namespace enchantment_tweaks::project
