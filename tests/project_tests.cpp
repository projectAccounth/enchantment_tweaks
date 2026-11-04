#include "core/project/game_versions.hpp"
#include "core/project/project_file.hpp"
#include "core/util/debug.hpp"
#include "test_utils.hpp"

#include <filesystem>
#include <fstream>
#include <string>

using namespace enchantment_tweaks;
using namespace enchantment_tweaks::project;
using namespace enchantment_tweaks::debug;
namespace fs = std::filesystem;

namespace {

Project sampleProject() {
  Project p;
  p.kind = ProjectKind::Pack;
  p.options.thicknessPx = 2.0;
  p.options.outlineSizePx = 1.5;
  p.options.outlineColor = "00ff7f";
  p.options.unenchantedOutline = true;
  p.gameVersionId = "1.21.4";
  p.gameJar = "/games/1.21.4/1.21.4.jar";
  p.packPath = fs::path("/packs/mine.zip");
  p.selected = {"minecraft:diamond_sword", "minecraft:bow"};
  p.supportedVersions = {"1.21.4", "1.21.10"};
  p.sort = "id";
  return p;
}

void touch(const fs::path &file) {
  fs::create_directories(file.parent_path());
  std::ofstream(file) << "x";
}

} // namespace

int main() {
  test_util::Suite suite;

  suite.run("project round-trips through JSON", [] {
    const Project original = sampleProject();
    const auto loaded = projectFromJson(projectToJson(original));
    test_util::check(loaded.project.has_value(),
                     "project did not load: " + loaded.error);
    const Project &p = *loaded.project;
    test_util::check(p.gameVersionId == "1.21.4", "version lost");
    test_util::check(p.gameJar == original.gameJar, "game path lost");
    test_util::check(p.packPath && *p.packPath == *original.packPath,
                     "pack lost");
    test_util::check(p.selected == original.selected, "selection lost");
    test_util::check(p.sort == "id", "sort lost");
    test_util::check(p.kind == ProjectKind::Pack, "kind lost");
    test_util::check(p.supportedVersions ==
                         std::vector<std::string>{"1.21.4", "1.21.10"},
                     "supported versions lost");
    test_util::check(
        p.options.thicknessPx == 2.0 && p.options.outlineSizePx == 1.5 &&
            p.options.outlineColor == "00ff7f" &&
            p.options.unenchantedOutline && !p.options.unenchantedUnshaded,
        "item options lost");
  });

  suite.run("project without a pack loads with no pack", [] {
    Project p = sampleProject();
    p.kind = ProjectKind::Game;
    p.packPath.reset();
    const auto loaded = projectFromJson(projectToJson(p));
    test_util::check(loaded.project && !loaded.project->packPath,
                     "pack should be absent");
  });

  suite.run("rejects files that are not projects", [] {
    test_util::check(
        !projectFromJson(R"({"format":"something-else","version":1})")
             .project,
        "wrong format accepted");
    test_util::check(!projectFromJson("not json at all").project,
                     "garbage accepted");
    test_util::check(!projectFromJson("[]").project, "array accepted");
  });

  suite.run("rejects a project from a newer format version", [] {
    const auto r =
        projectFromJson("{\"format\":\"enchantment-tweaks-project\","
                        "\"version\":99,\"game\":{\"path\":\"/a.jar\"}}");
    test_util::check(!r.project, "future version accepted");
    test_util::check(r.error.find("99") != std::string::npos,
                     "error should name the version");
  });

  suite.run("rejects a project with no game", [] {
    test_util::check(
        !projectFromJson(
             R"({"format":"enchantment-tweaks-project","version":1})")
             .project,
        "project without game accepted");
    test_util::check(
        !projectFromJson(
             "{\"format\":\"enchantment-tweaks-project\",\"version\":1,"
             "\"game\":{\"path\":42}}")
             .project,
        "non-string game path accepted");
  });

  suite.run("rejects invalid selection keys", [] {
    test_util::check(
        !projectFromJson(
             "{\"format\":\"enchantment-tweaks-project\",\"version\":1,"
             "\"game\":{\"path\":\"/a.jar\"},\"selection\":[\"nocolon\"]}")
             .project,
        "bad key accepted");
    test_util::check(
        !projectFromJson(
             "{\"format\":\"enchantment-tweaks-project\",\"version\":1,"
             "\"game\":{\"path\":\"/a.jar\"},\"selection\":\"x\"}")
             .project,
        "non-list selection accepted");
  });

  suite.run("file write is atomic and reads back", [] {
    const fs::path file =
        fs::temp_directory_path() / "et_project_test" / "p.etproj";
    fs::remove_all(file.parent_path());
    test_util::check(writeProjectFile(file, sampleProject()).empty(),
                     "write failed");
    test_util::check(!fs::exists(fs::path(file.string() + ".tmp")),
                     "temp file left behind");
    const auto r = readProjectFile(file);
    test_util::check(r.project.has_value(), "read back failed: " + r.error);
    fs::remove_all(file.parent_path());
  });

  suite.run("missing project file reports an error", [] {
    const auto r = readProjectFile("/definitely/not/here.etproj");
    test_util::check(!r.project && !r.error.empty(), "missing file accepted");
  });

  suite.run("app settings round-trip and fall back on bad values", [] {
    AppSettings s;
    s.darkMode = true;
    s.displayMode = "baked";
    const auto back = settingsFromJson(settingsToJson(s));
    test_util::check(back.has_value() && back->darkMode &&
                         back->displayMode == "baked",
                     "app settings values lost");
    const auto clamped =
        settingsFromJson("{\"format\":\"enchantment-tweaks-settings\","
                         "\"displayMode\":\"spin\"}");
    test_util::check(clamped && clamped->displayMode == "rotating" &&
                         !clamped->darkMode,
                     "bad app settings not defaulted");
  });

  suite.run("project options are clamped when the file is edited by hand", [] {
    const auto r =
        projectFromJson("{\"format\":\"enchantment-tweaks-project\","
                        "\"version\":1,\"game\":{\"path\":\"/a.jar\"},"
                        "\"options\":{\"thicknessPx\":-3,\"outlineSizePx\":-1,"
                        "\"outlineColor\":\"gg\"}}");
    test_util::check(r.project.has_value(), "project did not load: " + r.error);
    if (r.project) {
      test_util::check(r.project->options.thicknessPx == 1.0 &&
                           r.project->options.outlineSizePx == 0.0 &&
                           r.project->options.outlineColor == "ffffff",
                       "options not clamped");
    }
  });

  suite.run("a pack project must name its pack", [] {
    test_util::check(
        !projectFromJson(
             "{\"format\":\"enchantment-tweaks-project\",\"version\":1,"
             "\"kind\":\"pack\",\"game\":{\"path\":\"/a.jar\"}}")
             .project,
        "pack project without a pack accepted");
  });

  suite.run(
      "installed versions are found, newest first, incomplete ones skipped",
      [] {
        const fs::path root = fs::temp_directory_path() / "et_mc_test";
        fs::remove_all(root);
        touch(root / "versions" / "1.21.4" / "1.21.4.jar");
        touch(root / "versions" / "1.21.10" / "1.21.10.jar");
        touch(root / "versions" / "1.20.4" / "1.20.4.jar");
        fs::create_directories(root / "versions" / "1.19.4"); // no jar: skipped
        touch(root / "versions" / "fabric-loader" /
              "fabric-loader.json"); // no jar: skipped
        const auto games = findInstalledGames(root);
        test_util::check(games.size() == 3, "expected 3 games, got " +
                                                std::to_string(games.size()));
        if (games.size() == 3) {
          test_util::check(games[0].id == "1.21.10" &&
                               games[1].id == "1.21.4" &&
                               games[2].id == "1.20.4",
                           "wrong order: " + games[0].id + ", " + games[1].id +
                               ", " + games[2].id);
          test_util::check(games[0].jar ==
                               root / "versions" / "1.21.10" / "1.21.10.jar",
                           "jar path wrong");
        }
        fs::remove_all(root);
      });

  suite.run("a missing launcher folder gives no versions", [] {
    test_util::check(findInstalledGames("/no/such/minecraft").empty(),
                     "expected empty list");
  });

  return suite.finish();
}
