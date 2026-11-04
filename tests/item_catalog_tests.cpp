#include "core/api/item_catalog.hpp"
#include "core/api/item_conversion.hpp"
#include "core/resource/resource_pack.hpp"
#include "libraries/nlohmann/json.hpp"
#include "test_utils.hpp"

#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

using namespace enchantment_tweaks;
using namespace enchantment_tweaks::catalog;
using namespace enchantment_tweaks::debug;
using namespace enchantment_tweaks::resource;
using nlohmann::json;
namespace fs = std::filesystem;

int main() {
  test_util::Suite suite;

  const fs::path jar =
      fs::path(TEST_FIXTURE_DIR) / "assets" / "1.21.4" / "1.21.4.jar";

  suite.run("display names from ids", [&jar] {
    test_util::check(displayNameFromId("diamond_sword") == "Diamond Sword",
                     "name not title-cased");
    test_util::check(displayNameFromId("a_b") == "A B",
                     "underscore not replaced");
  });

  if (fs::exists(jar)) {
    suite.run("catalog discovers generated vanilla items", [&jar] {
      const ItemCatalog cat = ItemCatalog::load(jar);
      test_util::check(cat.gameVersion() == "1.21.4",
                       "game version not read: " + cat.gameVersion());
      test_util::check(cat.gameResourceFormat() == 46,
                       "resource format not read");
      test_util::check(cat.items().size() > 300,
                       "too few convertible items: " +
                           std::to_string(cat.items().size()));
      const CatalogItem *sword = cat.find("minecraft:diamond_sword");
      test_util::check(sword != nullptr, "diamond_sword missing from catalog");
      if (!sword) return;
      test_util::check(sword->name == "Diamond Sword", "display name wrong");
      test_util::check(sword->sprite.width() == 16 &&
                           sword->sprite.height() == 16,
                       "sprite size wrong");
      test_util::check(sword->definition.at("model").at("type") ==
                           "minecraft:model",
                       "definition not kept");
      test_util::check(
          sword->modelIds ==
              std::vector<std::string>{"minecraft:item/diamond_sword"},
          "model list wrong: " + std::to_string(sword->modelIds.size()));
    });

    suite.run("buildPack rejects unknown keys", [&jar] {
      const ItemCatalog cat = ItemCatalog::load(jar);
      bool threw = false;
      try {
        (void)cat.buildPack({"minecraft:not_a_real_item"}, {}, "x");
      } catch (const std::invalid_argument &) {
        threw = true;
      }
      test_util::check(threw, "unknown key accepted");
    });

    suite.run(
        "selected items save as a pack that reloads over the base", [&jar] {
          const ItemCatalog cat = ItemCatalog::load(jar);
          conversion::ItemVariantOptions opts;
          opts.outlineSizePx = 1.0;
          opts.unenchantedOutline = true;
          const auto pack =
              cat.buildPack({"minecraft:diamond_sword", "minecraft:iron_sword"},
                            opts, "ET test");
          const fs::path zipPath =
              fs::temp_directory_path() / "et_catalog_save.zip";
          pack.writeZip(zipPath);

          ResourcePackLoader base(jar);
          test_util::check(base.load(), "base did not load");
          ResourcePackLoader overlay(zipPath);
          test_util::check(overlay.load(), "saved pack did not load");
          overlay.setBasePack(base);
          const auto def =
              overlay.getItemDefinitionJson("minecraft:iron_sword");
          test_util::check(def.has_value(),
                           "iron_sword definition not in saved pack");
          fs::remove(zipPath);
        });

    suite.run("writeIcons writes one PNG per item", [&jar] {
      const ItemCatalog cat = ItemCatalog::load(jar);
      const fs::path dir = fs::temp_directory_path() / "et_icon_test";
      fs::remove_all(dir);
      const auto paths = cat.writeIcons(dir);
      test_util::check(paths.size() == cat.items().size(),
                       "icon count differs from items");
      const auto it = paths.find("minecraft:diamond_sword");
      test_util::check(it != paths.end() && fs::exists(it->second),
                       "diamond_sword icon not written");
      fs::remove_all(dir);
    });

    suite.run("natural order: compass_2 before compass_10", [] {
      test_util::check(naturalLess("item/compass_2", "item/compass_10"),
                       "numeric runs not compared as numbers");
      test_util::check(!naturalLess("item/compass_10", "item/compass_2"),
                       "reverse order wrong");
      test_util::check(!naturalLess("item/a_2", "item/a_2"),
                       "equal strings compare as less");
      test_util::check(naturalLess("item/abc", "item/abd"),
                       "plain text order wrong");
    });

    suite.run("multi-model item: every frame is listed and exported", [&jar] {
      const ItemCatalog cat = ItemCatalog::load(jar);
      const CatalogItem *compass = cat.find("minecraft:recovery_compass");
      test_util::check(compass != nullptr, "recovery_compass missing");
      if (!compass) return;
      test_util::check(compass->modelIds.size() == 32,
                       "expected 32 frames, got " +
                           std::to_string(compass->modelIds.size()));
      test_util::check(
          compass->modelIds.front() == "minecraft:item/recovery_compass_00" ||
              compass->modelIds.front().find("recovery_compass_0") !=
                  std::string::npos,
          "frames not in natural order: " + compass->modelIds.front());
      test_util::check(compass->sprites.size() == compass->modelIds.size(),
                       "not every frame is convertible");

      auto countGlowing = [](const std::vector<conversion::OutputFile> &files) {
        int n = 0;
        for (const auto &f : files) {
          if (f.path.find("models/item/recovery_compass_") !=
                  std::string::npos &&
              f.path.find("_enchanted.json") != std::string::npos)
            ++n;
        }
        return n;
      };
      const auto all = cat.buildPack({"minecraft:recovery_compass"},
                                     conversion::ItemVariantOptions{}, "x",
                                     ExportScope::AllModels);
      test_util::check(countGlowing(all.files()) == 32,
                       "AllModels: expected 32 glowing frames");

      const auto rep = cat.buildPack({"minecraft:recovery_compass"},
                                     conversion::ItemVariantOptions{}, "x");
      test_util::check(countGlowing(rep.files()) == 1,
                       "Representative: expected exactly 1 glowing frame");
      const auto definitionText = [&] {
        for (const auto &f : rep.files()) {
          if (f.path == "assets/minecraft/items/recovery_compass.json")
            return std::string(f.bytes.begin(), f.bytes.end());
        }
        return std::string();
      }();
      test_util::check(!definitionText.empty(),
                       "definition missing from representative export");
      const json exported = json::parse(definitionText);
      test_util::check(exported.at("model").at("type") == "minecraft:select" ||
                           exported.at("model").at("type") ==
                               "minecraft:range_dispatch",
                       "definition structure not kept");
    });

    suite.run("custom 3D items are kept, flagged, and converted with unshaded "
              "textures",
              [&jar] {
                const ItemCatalog cat = ItemCatalog::load(jar);
                const CatalogItem *egg = cat.find("minecraft:dragon_egg");
                test_util::check(egg != nullptr,
                                 "dragon_egg (custom 3D) missing from catalog");
                if (!egg) return;
                test_util::check(!egg->generated,
                                 "dragon_egg should not be flagged generated");
                const CatalogItem *sword = cat.find("minecraft:diamond_sword");
                test_util::check(sword && sword->generated,
                                 "diamond_sword should stay generated");
                const auto pack =
                    cat.buildPack({"minecraft:dragon_egg"},
                                  conversion::ItemVariantOptions{}, "x");
                bool glow = false, texture = false;
                for (const auto &f : pack.files()) {
                  if (f.path ==
                      "assets/minecraft/models/block/dragon_egg_enchanted.json")
                    glow = true;
                  if (f.path ==
                      "assets/minecraft/textures/block/dragon_egg_unshaded.png")
                    texture = true;
                }
                test_util::check(
                    glow && texture,
                    "glowing custom model or its unshaded texture missing");
              });
  } else {
    Logger::warning() << "[skip] catalog integration tests need " << jar
                      << '\n';
  }

  return suite.finish();
}

// Natural ordering is checked in the catalog tests through the public catalog.
