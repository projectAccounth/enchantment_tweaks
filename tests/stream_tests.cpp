#include "core/api/pack_stream.hpp"
#include "core/api/shader_overlays.hpp"
#include "core/util/debug.hpp"
#include "core/util/pack_files.hpp"
#include "test_utils.hpp"

#include <algorithm>
#include <filesystem>
#include <string>

using namespace enchantment_tweaks;
using namespace enchantment_tweaks::conversion;
namespace fs = std::filesystem;

namespace {

std::string textOf(const std::vector<OutputFile> &files,
                   const std::string &path) {
  for (const auto &f : files) {
    if (f.path == path) return {f.bytes.begin(), f.bytes.end()};
  }
  return {};
}

bool has(const std::vector<OutputFile> &files, const std::string &path) {
  return std::any_of(files.begin(), files.end(),
                     [&](const OutputFile &f) { return f.path == path; });
}

ConvertedItem item(const std::string &id,
                   const std::vector<OutputFile> &extra) {
  ConvertedItem c;
  c.itemId = "minecraft:" + id;
  c.definition = {
      {"model",
       {{"type", "minecraft:model"}, {"model", "minecraft:item/" + id}}}};
  c.files = extra;
  return c;
}

} // namespace

int main() {
  test_util::Suite suite;

  suite.run("a streamed zip reads back with every entry", [] {
    const fs::path zipPath = fs::temp_directory_path() / "et_stream_basic.zip";
    {
      zip::StreamingZipWriter w(zipPath);
      w.add("a.txt", {'h', 'i'});
      w.add("dir/b.txt", {'x', 'y', 'z'});
      w.close();
    }
    const auto files = pack_files::readPackFiles(zipPath);
    test_util::check(files.size() == 2, "expected 2 entries");
    test_util::check(textOf(files, "dir/b.txt") == "xyz",
                     "bytes not read back");
    fs::remove(zipPath);
  });

  suite.run("the streamed writer refuses duplicate and empty names", [] {
    const fs::path zipPath = fs::temp_directory_path() / "et_stream_dup.zip";
    zip::StreamingZipWriter w(zipPath);
    w.add("a.txt", {'1'});
    bool dup = false, empty = false;
    try {
      w.add("a.txt", {'2'});
    } catch (const std::invalid_argument &) {
      dup = true;
    }
    try {
      w.add("", {'3'});
    } catch (const std::invalid_argument &) {
      empty = true;
    }
    w.close();
    fs::remove(zipPath);
    test_util::check(dup, "duplicate accepted");
    test_util::check(empty, "empty name accepted");
  });

  suite.run(
      "items stream out with their files, and identical shared files are "
      "written once",
      [] {
        const fs::path zipPath =
            fs::temp_directory_path() / "et_stream_items.zip";
        {
          PackStreamer p(zipPath, "test", 46);
          const OutputFile shared{"assets/minecraft/textures/item/shared.png",
                                  {1, 2, 3}};
          p.addItem(item("bow", {shared}));
          p.addItem(item("crossbow", {shared}));
          p.setSupportedFormats(46, 46);
          p.close();
        }
        const auto files = pack_files::readPackFiles(zipPath);
        test_util::check(has(files, "assets/minecraft/items/bow.json") &&
                             has(files, "assets/minecraft/items/crossbow.json"),
                         "item definitions missing");
        int shares = 0;
        for (const auto &f : files) {
          if (f.path == "assets/minecraft/textures/item/shared.png") ++shares;
        }
        test_util::check(shares == 1, "shared file written " +
                                          std::to_string(shares) + " times");
        test_util::check(
            textOf(files, "pack.mcmeta").find("\"min_inclusive\": 46") !=
                std::string::npos,
            "supported formats missing");
        fs::remove(zipPath);
      });

  suite.run("a different file under a used name is an error", [] {
    const fs::path zipPath =
        fs::temp_directory_path() / "et_stream_conflict.zip";
    PackStreamer p(zipPath, "test", 46);
    p.addItem(item("bow", {{"assets/minecraft/textures/item/x.png", {1}}}));
    bool threw = false;
    try {
      p.addItem(
          item("crossbow", {{"assets/minecraft/textures/item/x.png", {2}}}));
    } catch (const std::invalid_argument &) {
      threw = true;
    }
    p.close();
    fs::remove(zipPath);
    test_util::check(threw, "conflict accepted");
  });

  suite.run(
      "a base pack's file is skipped where a generated file has the same name",
      [] {
        const fs::path zipPath =
            fs::temp_directory_path() / "et_stream_base.zip";
        {
          PackStreamer p(zipPath, "test", 46);
          p.addItem(item("bow", {}));
          p.addIfAbsent(
              {"assets/minecraft/items/bow.json", {'b', 'a', 's', 'e'}});
          p.addIfAbsent({"assets/minecraft/items/other.json", {'o'}});
          p.addIfAbsent({"pack.mcmeta", {'x'}});
          p.close();
        }
        const auto files = pack_files::readPackFiles(zipPath);
        test_util::check(textOf(files, "assets/minecraft/items/bow.json") !=
                             "base",
                         "base file overwrote the generated one");
        test_util::check(has(files, "assets/minecraft/items/other.json"),
                         "base file missing");
        fs::remove(zipPath);
      });

  suite.run("the stored shader overlays are written in their original order",
            [] {
              const fs::path root = fs::path(TEST_FIXTURE_DIR) / ".." / "apps" /
                                    "native" / "app" / "assets" / "shaders" /
                                    "overlays";
              const auto overlays = loadShaderOverlays(root);
              test_util::check(overlays.size() == 5,
                               "expected 5 overlays, got " +
                                   std::to_string(overlays.size()));
              if (overlays.size() == 5) {
                test_util::check(overlays[0].directory == "1.21.5" &&
                                     overlays[4].directory == "26.3",
                                 "order changed");
                test_util::check(overlays[3].minFormat == 84 &&
                                     overlays[3].maxFormat == 96,
                                 "format range changed");
              }
              const fs::path zipPath =
                  fs::temp_directory_path() / "et_stream_shaders.zip";
              {
                PackStreamer p(zipPath, "test", 46);
                for (const auto &o : overlays)
                  p.addOverlay(o, root);
                p.close();
              }
              const auto files = pack_files::readPackFiles(zipPath);
              test_util::check(
                  has(files, "1.21.5/assets/minecraft/shaders/core/"
                             "rendertype_item_entity_translucent_cull.fsh"),
                  "1.21.5 shader missing");
              test_util::check(
                  has(files, "26.3/assets/minecraft/shaders/core/item.fsh"),
                  "26.3 shader missing");
              const std::string meta = textOf(files, "pack.mcmeta");
              test_util::check(meta.find("\"overlays\"") != std::string::npos &&
                                   meta.find("\"26.3\"") != std::string::npos,
                               "overlays not listed in pack.mcmeta");
              fs::remove(zipPath);
            });

  return suite.finish();
}
