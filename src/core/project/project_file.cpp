#include "core/project/project_file.hpp"

#include "core/types/common.hpp"
#include "core/util/file_io.hpp"
#include "core/util/text.hpp"

#include <cctype>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

namespace enchantment_tweaks::project {

namespace {

constexpr const char *kFormatName = "enchantment-tweaks-project";
constexpr const char *kSettingsName = "enchantment-tweaks-settings";

bool isKey(const std::string &key) {
  const auto colon = key.find(':');
  return colon != std::string::npos && colon > 0 && colon + 1 < key.size();
}

std::string optionalString(const json &j, const char *key,
                           std::string fallback) {
  const auto it = j.find(key);
  return it != j.end() && it->is_string() ? it->get<std::string>()
                                          : std::move(fallback);
}

} // namespace

ItemOptions applyOverride(const ItemOptions &base, const ItemOverride &o) {
  ItemOptions out = base;
  if (o.thicknessPx) out.thicknessPx = *o.thicknessPx;
  if (o.outlineSizePx) out.outlineSizePx = *o.outlineSizePx;
  if (o.outlineColor) out.outlineColor = *o.outlineColor;
  if (o.unenchantedOutline) out.unenchantedOutline = *o.unenchantedOutline;
  if (o.unenchantedUnshaded) out.unenchantedUnshaded = *o.unenchantedUnshaded;
  if (o.mergeColumns) out.mergeColumns = *o.mergeColumns;
  return out;
}

std::string projectToJson(const Project &project) {
  json j;
  j["format"] = kFormatName;
  j["version"] = Project::kFormatVersion;
  j["kind"] = project.kind == ProjectKind::Pack ? "pack" : "game";
  j["game"] = {{"versionId", project.gameVersionId},
               {"path", project.gameJar.string()}};
  j["pack"] =
      project.packPath ? json(project.packPath->string()) : json(nullptr);
  j["selection"] = project.selected;
  j["supported"] = project.supportedVersions;
  j["sort"] = project.sort;
  json overrides = json::object();
  for (const auto &[key, o] : project.overrides) {
    if (o.empty()) continue;
    json entry = json::object();
    if (o.thicknessPx) entry["thicknessPx"] = *o.thicknessPx;
    if (o.outlineSizePx) entry["outlineSizePx"] = *o.outlineSizePx;
    if (o.outlineColor) entry["outlineColor"] = *o.outlineColor;
    if (o.unenchantedOutline)
      entry["unenchantedOutline"] = *o.unenchantedOutline;
    if (o.unenchantedUnshaded)
      entry["unenchantedUnshaded"] = *o.unenchantedUnshaded;
    if (o.mergeColumns) entry["mergeColumns"] = *o.mergeColumns;
    overrides[key] = entry;
  }
  j["overrides"] = overrides;
  j["options"] = {{"thicknessPx", project.options.thicknessPx},
                  {"outlineSizePx", project.options.outlineSizePx},
                  {"outlineColor", project.options.outlineColor},
                  {"unenchantedOutline", project.options.unenchantedOutline},
                  {"unenchantedUnshaded", project.options.unenchantedUnshaded},
                  {"mergeColumns", project.options.mergeColumns}};
  return j.dump(2) + "\n";
}

ProjectLoadResult projectFromJson(const std::string &text) {
  ProjectLoadResult result;
  json j = json::parse(text, nullptr, /*allow_exceptions=*/false);
  if (!j.is_object()) {
    result.error = "not a project file";
    return result;
  }
  if (j.value("format", "") != kFormatName) {
    result.error = "not an Enchantment Tweaks project";
    return result;
  }
  const int version = j.value("version", 0);
  if (version < 1 || version > Project::kFormatVersion) {
    result.error = "unsupported project version " + std::to_string(version);
    return result;
  }
  const auto game = j.find("game");
  if (game == j.end() || !game->is_object() || !game->contains("path") ||
      !game->at("path").is_string() ||
      game->at("path").get<std::string>().empty()) {
    result.error = "project has no game set";
    return result;
  }

  Project p;
  const std::string kind = optionalString(j, "kind", "game");
  if (kind != "game" && kind != "pack") {
    result.error = "unknown project kind '" + kind + "'";
    return result;
  }
  p.kind = kind == "pack" ? ProjectKind::Pack : ProjectKind::Game;
  p.gameVersionId = optionalString(*game, "versionId", "");
  p.gameJar = game->at("path").get<std::string>();

  const auto pack = j.find("pack");
  if (pack != j.end() && pack->is_string())
    p.packPath = pack->get<std::string>();
  if (p.kind == ProjectKind::Pack && !p.packPath) {
    result.error = "a pack project needs its pack";
    return result;
  }

  const auto selection = j.find("selection");
  if (selection != j.end()) {
    if (!selection->is_array()) {
      result.error = "selection must be a list";
      return result;
    }
    for (const auto &entry : *selection) {
      if (!entry.is_string() || !isKey(entry.get<std::string>())) {
        result.error = "selection contains an invalid item key";
        return result;
      }
      p.selected.push_back(entry.get<std::string>());
    }
  }
  p.sort = optionalString(j, "sort", "name-asc");
  const auto supported = j.find("supported");
  if (supported != j.end()) {
    if (!supported->is_array()) {
      result.error = "supported versions must be a list";
      return result;
    }
    for (const auto &entry : *supported) {
      if (!entry.is_string()) {
        result.error = "a supported version is not a version id";
        return result;
      }
      p.supportedVersions.push_back(entry.get<std::string>());
    }
  }

  const auto options = j.find("options");
  if (options != j.end() && options->is_object()) {
    p.options.thicknessPx =
        options->value("thicknessPx", p.options.thicknessPx);
    p.options.outlineSizePx =
        options->value("outlineSizePx", p.options.outlineSizePx);
    p.options.outlineColor =
        optionalString(*options, "outlineColor", p.options.outlineColor);
    p.options.unenchantedOutline = options->value("unenchantedOutline", false);
    p.options.unenchantedUnshaded =
        options->value("unenchantedUnshaded", false);
    p.options.mergeColumns = options->value("mergeColumns", false);
  }
  if (!(p.options.thicknessPx > 0.0)) p.options.thicknessPx = 1.0;
  if (p.options.outlineSizePx < 0.0) p.options.outlineSizePx = 0.0;
  if (!text::isHexColor(p.options.outlineColor))
    p.options.outlineColor = "ffffff";

  const auto overrides = j.find("overrides");
  if (overrides != j.end() && overrides->is_object()) {
    for (const auto &[key, entry] : overrides->items()) {
      if (!isKey(key) || !entry.is_object()) continue;
      ItemOverride o;
      if (entry.contains("thicknessPx") && entry["thicknessPx"].is_number())
        o.thicknessPx = entry["thicknessPx"].get<double>();
      if (entry.contains("outlineSizePx") && entry["outlineSizePx"].is_number())
        o.outlineSizePx = entry["outlineSizePx"].get<double>();
      if (entry.contains("outlineColor") && entry["outlineColor"].is_string() &&
          text::isHexColor(entry["outlineColor"].get<std::string>()))
        o.outlineColor = entry["outlineColor"].get<std::string>();
      if (entry.contains("unenchantedOutline") &&
          entry["unenchantedOutline"].is_boolean())
        o.unenchantedOutline = entry["unenchantedOutline"].get<bool>();
      if (entry.contains("unenchantedUnshaded") &&
          entry["unenchantedUnshaded"].is_boolean())
        o.unenchantedUnshaded = entry["unenchantedUnshaded"].get<bool>();
      if (entry.contains("mergeColumns") && entry["mergeColumns"].is_boolean())
        o.mergeColumns = entry["mergeColumns"].get<bool>();
      if (!o.empty()) p.overrides[key] = o;
    }
  }

  result.project = std::move(p);
  return result;
}

ProjectLoadResult readProjectFile(const fs::path &path) {
  const auto text = file_io::readText(path);
  if (!text) {
    ProjectLoadResult missing;
    missing.error = "cannot open " + path.string();
    return missing;
  }
  return projectFromJson(*text);
}

std::string writeProjectFile(const fs::path &path,
                             const Project &project) {
  const std::string text = projectToJson(project);
  const auto temp = fs::path(path.string() + ".tmp");
  try {
    if (path.has_parent_path())
      fs::create_directories(path.parent_path());
    {
      std::ofstream out(temp, std::ios::binary | std::ios::trunc);
      if (!out) return "cannot write " + temp.string();
      out << text;
      if (!out) return "failed writing " + temp.string();
    }
    fs::rename(temp, path);
  } catch (const fs::filesystem_error &error) {
    return error.what();
  }
  return {};
}

std::string settingsToJson(const AppSettings &s) {
  json j;
  j["format"] = kSettingsName;
  j["version"] = 2;
  j["darkMode"] = s.darkMode;
  j["displayMode"] = s.displayMode;
  j["groupMode"] = s.groupMode;
  return j.dump(2) + "\n";
}

std::optional<AppSettings> settingsFromJson(const std::string &text) {
  json j = json::parse(text, nullptr, /*allow_exceptions=*/false);
  if (!j.is_object() || j.value("format", "") != kSettingsName)
    return std::nullopt;
  AppSettings s;
  s.darkMode = j.value("darkMode", s.darkMode);
  s.displayMode = optionalString(j, "displayMode", s.displayMode);
  if (s.displayMode != "baked" && s.displayMode != "rotating")
    s.displayMode = "rotating";
  s.groupMode = optionalString(j, "groupMode", s.groupMode);
  if (s.groupMode != "none" && s.groupMode != "prefix" &&
      s.groupMode != "suffix")
    s.groupMode = "none";
  return s;
}

} // namespace enchantment_tweaks::project
