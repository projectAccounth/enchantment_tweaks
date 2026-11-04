#include "core/api/item_conversion.hpp"
#include "core/api/pack_writer.hpp"
#include "core/resource/resource_pack.hpp"
#include "core/util/debug.hpp"
#include "core/util/zip_writer.hpp"

#include "test_utils.hpp"
#include <map>

#include <filesystem>
#include <fstream>
#include <string>

using namespace enchantment_tweaks;
using namespace enchantment_tweaks::conversion;
using namespace enchantment_tweaks::debug;
using namespace enchantment_tweaks::resource;
using nlohmann::json;
namespace fs = std::filesystem;

namespace {

// 2x2 sprite: opaque red, opaque green, transparent, semi-transparent blue.
image::Image tinySprite() {
  return image::Image(
      2, 2, {255, 0, 0, 255, 0, 255, 0, 255, 0, 0, 0, 0, 0, 0, 255, 128});
}

bool hasFile(const std::vector<OutputFile> &files, const std::string &path) {
  for (const auto &f : files) {
    if (f.path == path) return true;
  }
  return false;
}

std::string modelOf(const json &node) {
  return node.at("model").get<std::string>();
}

} // namespace

int main() {
  test_util::Suite suite;

  suite.run(
      "unshaded mask: opaque and partial pixels to 252, transparent stays 0",
      [] {
        const auto out = makeUnshadedSprite(tinySprite());
        const auto &p = out.pixels();
        test_util::check(p[3] == kUnshadedAlpha, "opaque red not masked");
        test_util::check(p[7] == kUnshadedAlpha, "opaque green not masked");
        test_util::check(p[11] == 0, "transparent pixel changed");
        test_util::check(p[15] == kUnshadedAlpha, "partial pixel not masked");
        test_util::check(p[0] == 255 && p[1] == 0 && p[2] == 0,
                         "color changed by mask");
      });

  suite.run("outline sprite: color and mask", [] {
    const auto lit = makeOutlineSprite(3, 2, OutlineColor{10, 20, 30}, false);
    test_util::check(lit.width() == 3 && lit.height() == 2,
                     "outline size wrong");
    test_util::check(lit.at(2, 1).a == 255 && lit.at(2, 1).r == 10,
                     "outline pixel wrong");
    const auto masked = makeOutlineSprite(1, 1, OutlineColor{}, true);
    test_util::check(masked.at(0, 0).a == kUnshadedAlpha,
                     "masked outline alpha wrong");
  });

  suite.run(
      "enchanted-only: definition switches on glint and enchantments", [] {
        ItemVariantOptions opts;
        const auto item = convertSingleSpriteItem("minecraft", "diamond_sword",
                                                  tinySprite(), opts);
        test_util::check(item.itemId == "minecraft:diamond_sword",
                         "item id wrong");
        const json &model = item.definition.at("model");
        test_util::check(model.at("component") ==
                             "minecraft:enchantment_glint_override",
                         "outer condition is not the glint override");
        test_util::check(model.at("on_true").at("model") ==
                             "minecraft:item/diamond_sword_enchanted",
                         "glint branch does not use the glowing model");
        const json &inner = model.at("on_false");
        test_util::check(inner.at("component") == "minecraft:enchantments",
                         "inner condition is not enchantments");
        test_util::check(modelOf(inner.at("on_false")) ==
                             "minecraft:item/diamond_sword",
                         "unenchanted branch should keep the original model");
        test_util::check(
            hasFile(
                item.files,
                "assets/minecraft/models/item/diamond_sword_enchanted.json"),
            "glowing model file missing");
        test_util::check(
            hasFile(
                item.files,
                "assets/minecraft/textures/item/diamond_sword_unshaded.png"),
            "masked texture missing");
        test_util::check(
            !hasFile(
                item.files,
                "assets/minecraft/models/item/diamond_sword_unenchanted.json"),
            "unenchanted model generated without the option");
      });

  suite.run("unenchanted outline: separate model, shared masked texture", [] {
    ItemVariantOptions opts;
    opts.outlineSizePx = 1.0;
    opts.unenchantedOutline = true;
    opts.unenchantedUnshaded = true;
    const auto item = convertSingleSpriteItem("minecraft", "diamond_sword",
                                              tinySprite(), opts);
    const json &inner = item.definition.at("model").at("on_false");
    test_util::check(modelOf(inner.at("on_false")) ==
                         "minecraft:item/diamond_sword_unenchanted",
                     "unenchanted branch does not use the outlined model");
    test_util::check(
        hasFile(item.files,
                "assets/minecraft/models/item/diamond_sword_unenchanted.json"),
        "unenchanted model missing");
    test_util::check(hasFile(item.files, "assets/minecraft/textures/item/"
                                         "diamond_sword_outline_unshaded.png"),
                     "masked outline texture missing");
    test_util::check(
        !hasFile(item.files,
                 "assets/minecraft/textures/item/diamond_sword_outline.png"),
        "unmasked outline generated though only masked is used");
  });

  suite.run("outline option: glowing model references its outline texture", [] {
    ItemVariantOptions opts;
    opts.outlineSizePx = 1.0;
    const auto item =
        convertSingleSpriteItem("minecraft", "bow", tinySprite(), opts);
    std::string modelText;
    for (const auto &f : item.files) {
      if (f.path == "assets/minecraft/models/item/bow_enchanted.json") {
        modelText.assign(f.bytes.begin(), f.bytes.end());
      }
    }
    const json model = json::parse(modelText);
    test_util::check(model.at("textures").contains("outline"),
                     "outline texture variable missing");
    test_util::check(model.at("textures").at("outline") ==
                         "minecraft:item/bow_outline_unshaded",
                     "outline points at the wrong texture");
    test_util::check(model.at("textures").at("texture") ==
                         "minecraft:item/bow_unshaded",
                     "glowing model does not use the masked sprite");
  });

  suite.run("PNG output is a valid PNG", [] {
    const auto bytes = tinySprite().encodePng();
    test_util::check(bytes.size() > 8 && bytes[0] == 0x89 && bytes[1] == 'P' &&
                         bytes[2] == 'N' && bytes[3] == 'G',
                     "encodePng did not produce PNG signature");
  });

  suite.run("zip writer: stored archive reads back through the loader", [] {
    const fs::path tmp = fs::temp_directory_path() / "et_zip_roundtrip.zip";
    zip::ZipWriter zw;
    zw.addFile("pack.mcmeta",
               std::string(R"({"pack":{"pack_format":46,"description":"x"}})"));
    zw.addFile("assets/minecraft/items/a.json", std::string("{}"));
    zw.write(tmp);
    ResourcePackLoader loader(tmp);
    test_util::check(loader.load(), "zip written by ZipWriter did not load");
    test_util::check(loader.getPack().packFormat == 46,
                     "pack format not read back");
    fs::remove(tmp);
  });

  suite.run("zip writer rejects duplicate entries", [] {
    zip::ZipWriter zw;
    zw.addFile("a.txt", std::string("1"));
    bool threw = false;
    try {
      zw.addFile("a.txt", std::string("2"));
    } catch (const std::invalid_argument &) {
      threw = true;
    }
    test_util::check(threw, "duplicate entry accepted");
  });

  const fs::path vanillaJar =
      fs::path(TEST_FIXTURE_DIR) / "assets" / "1.21.4" / "1.21.4.jar";
  if (fs::exists(vanillaJar)) {
    suite.run(
        "end to end: convert a real vanilla item and reload it over the base "
        "pack",
        [] {
          const fs::path jar =
              fs::path(TEST_FIXTURE_DIR) / "assets" / "1.21.4" / "1.21.4.jar";
          ResourcePackLoader base(jar);
          test_util::check(base.load(), "base JAR did not load");
          test_util::check(base.getPack().gameResourceFormat > 0,
                           "game resource format not read from version.json");

          const TextureEntry *sword = nullptr;
          for (const auto &t : base.getTextures()) {
            if (t.nmsp_ == "minecraft" && t.name == "item/diamond_sword")
              sword = &t;
          }
          test_util::check(sword != nullptr,
                           "diamond_sword sprite not found in base pack");
          if (!sword) return;

          image::Image sprite(sword->width, sword->height, sword->pixels);
          ItemVariantOptions opts;
          opts.outlineSizePx = 1.0;
          opts.unenchantedOutline = true;
          const auto item = convertSingleSpriteItem(
              "minecraft", "diamond_sword", sprite, opts);

          PackBuilder pack("ET test overlay",
                           base.getPack().gameResourceFormat);
          pack.addItem(item);
          const fs::path zipPath =
              fs::temp_directory_path() / "et_overlay_test.zip";
          pack.writeZip(zipPath);

          ResourcePackLoader overlay(zipPath);
          test_util::check(overlay.load(), "overlay zip did not load");
          overlay.setBasePack(base);
          const auto def =
              overlay.getItemDefinitionJson("minecraft:diamond_sword");
          test_util::check(def.has_value(),
                           "converted definition not found after reload");
          if (def) {
            test_util::check(def->at("model").at("component") ==
                                 "minecraft:enchantment_glint_override",
                             "reloaded definition lost its condition");
          }
          ModelResolver resolver(overlay);
          const auto glow = resolver.resolve(
              std::string("minecraft:item/diamond_sword_enchanted"));
          test_util::check(glow.textures.count("texture") == 1 &&
                               !glow.elements.empty(),
                           "glowing model did not resolve with elements");
          fs::remove(zipPath);
        });
  } else {
    debug::Logger::warning()
        << "[skip] end-to-end conversion test needs " << vanillaJar << '\n';
  }

  suite.run(
      "select definition: every model rewritten, structure kept, files per "
      "model",
      [] {
        const json def = json::parse(
            R"({"model":{"type":"minecraft:select","property":"minecraft:display_context",
      "cases":[{"when":"gui","model":{"type":"minecraft:model","model":"minecraft:item/spy_a"}}],
      "fallback":{"type":"minecraft:model","model":"minecraft:item/spy_b"}}})");
        std::map<std::string, ModelSprite> sprites{
            {"minecraft:item/spy_a", {"item/spy_a", tinySprite()}},
            {"minecraft:item/spy_b", {"item/spy_b", tinySprite()}}};
        ItemVariantOptions opts;
        const auto item =
            convertItemDefinition("minecraft", "spyglass", def, sprites, opts);
        const json &sel = item.definition.at("model");
        test_util::check(sel.at("type") == "minecraft:select",
                         "select wrapper lost");
        test_util::check(sel.at("property") == "minecraft:display_context",
                         "select property lost");
        const json &caseModel = sel.at("cases").at(0).at("model");
        test_util::check(caseModel.at("type") == "minecraft:condition",
                         "case leaf not wrapped");
        test_util::check(modelOf(caseModel.at("on_true")) ==
                             "minecraft:item/spy_a_enchanted",
                         "case glow model wrong");
        test_util::check(modelOf(sel.at("fallback").at("on_true")) ==
                             "minecraft:item/spy_b_enchanted",
                         "fallback glow model wrong");
        test_util::check(
            hasFile(item.files,
                    "assets/minecraft/models/item/spy_a_enchanted.json") &&
                hasFile(item.files,
                        "assets/minecraft/models/item/spy_b_enchanted.json"),
            "a model was not generated");
      });

  suite.run(
      "select definition: a model shared by two leaves is generated once", [] {
        const json def =
            json::parse(R"({"model":{"type":"minecraft:select","property":"x",
      "cases":[{"when":"a","model":{"type":"minecraft:model","model":"minecraft:item/same"}}],
      "fallback":{"type":"minecraft:model","model":"minecraft:item/same"}}})");
        std::map<std::string, ModelSprite> sprites{
            {"minecraft:item/same", {"item/same", tinySprite()}}};
        const auto item = convertItemDefinition("minecraft", "x", def, sprites,
                                                ItemVariantOptions{});
        int count = 0;
        for (const auto &f : item.files) {
          if (f.path == "assets/minecraft/models/item/same_enchanted.json")
            ++count;
        }
        test_util::check(count == 1, "shared model generated more than once");
      });

  suite.run("leaves without a sprite are left exactly as they were", [] {
    const json def = json::parse(
        R"({"model":{"type":"minecraft:model","model":"minecraft:item/unknown","tints":[{"type":"minecraft:constant","value":-1}]}})");
    const auto item =
        convertItemDefinition("minecraft", "x", def, {}, ItemVariantOptions{});
    test_util::check(item.definition == def, "unconverted definition changed");
    test_util::check(item.files.empty(),
                     "files generated for an unconverted model");
  });

  return suite.finish();
}
