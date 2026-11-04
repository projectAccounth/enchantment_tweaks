#include "core/types/common.hpp"
#include "core/types/mesh/types.hpp"
#include "core/util/debug.hpp"
#include "libraries/nlohmann/json.hpp"
#include "test_utils.hpp"

#include <cstdlib>
#include <string>

using namespace enchantment_tweaks;
using namespace enchantment_tweaks::debug;
using namespace enchantment_tweaks::mesh;
using nlohmann::json;

int main() {
  test_util::Suite suite;

  suite.run("uv serialize/deserialize", [] {
    Uv uv(0, 0, 16, 16);
    json j = uv.serialize();
    test_util::check(j.is_array() && j.size() == 4, "uv should be array of 4");
    Uv uv2;
    uv2.deserialize(j);
    test_util::check(uv2.uFrom == 0 && uv2.uTo == 16, "uv values mismatch");
  });

  suite.run("face serialize/deserialize with cullface and rotation", [] {
    Uv uv(0, 0, 16, 16);
    Face face(uv, "#layer0");
    face.cullface = FaceDirection::North;
    face.rotation = 90;
    json j = face.serialize();
    test_util::check(j["texture"] == "#layer0", "texture mismatch");
    test_util::check(j["rotation"] == 90, "rotation mismatch");
    test_util::check(j.contains("cullface"), "cullface missing");
    Face face2;
    face2.deserialize(j);
    test_util::check(face2.rotation == 90, "rotation roundtrip mismatch");
    test_util::check(face2.cullface.has_value(), "cullface should be set");
  });

  suite.run("element serialize/deserialize", [] {
    Element elem;
    elem.from = Vec3{0, 0, 0};
    elem.to = Vec3{16, 16, 16};
    Uv uv(0, 0, 16, 16);
    Face face(uv, "#all");
    elem.faces[FaceDirection::North] = face;
    json j = elem.serialize();
    test_util::check(j.contains("from") && j.contains("to") &&
                         j.contains("faces"),
                     "missing fields");
    Element elem2;
    elem2.deserialize(j);
    test_util::check(elem2.faces.size() == 1, "faces count mismatch");
  });

  suite.run("display transform serialize/deserialize", [] {
    DisplayTransform dt;
    dt.rotation = Vec3{0, 90, 0};
    dt.scale = Vec3{0.5, 0.5, 0.5};
    json j = dt.serialize();
    test_util::check(j.contains("rotation") && j.contains("scale"),
                     "missing transforms");
    DisplayTransform dt2;
    dt2.deserialize(j);
    test_util::check(dt2.rotation.has_value() && dt2.scale.has_value(),
                     "roundtrip missing transforms");
  });

  suite.run("model serialize/deserialize with textures and display", [] {
    Model model;
    model.parent = "item/generated";
    model.textures["layer0"] = "minecraft:item/stone";
    DisplayTransform dt;
    dt.translation = Vec3{0, 3, 1};
    model.display[DisplayTransformType::Gui] = dt;
    json j = model.serialize();
    test_util::check(j["textures"]["layer0"] == "minecraft:item/stone",
                     "texture mismatch");
    Model model2;
    model2.deserialize(j);
    test_util::check(model2.textures["layer0"] == "minecraft:item/stone",
                     "texture roundtrip mismatch");
    test_util::check(model2.display.count(DisplayTransformType::Gui) == 1,
                     "display roundtrip missing");
  });

  suite.run("on_shelf display context round-trips", [] {
    const json j =
        json::parse(R"({"display":{"on_shelf":{"rotation":[0,180,0]}}})");
    Model model;
    model.deserialize(j);
    test_util::check(model.display.count(DisplayTransformType::OnShelf) == 1,
                     "on_shelf not loaded");
    test_util::check(model.serialize()["display"].contains("on_shelf"),
                     "on_shelf not written back");
  });

  suite.run("unknown display context is skipped, not fatal", [] {
    const json j = json::parse(
        R"({"display":{"future_context":{"rotation":[0,0,0]},"gui":{"rotation":[0,0,0]}}})");
    Model model;
    model.deserialize(j);
    test_util::check(model.display.count(DisplayTransformType::Gui) == 1,
                     "valid context lost alongside unknown one");
    test_util::check(model.display.size() == 1, "unknown context was kept");
  });

  suite.run("object-form texture keeps sprite and force_translucent", [] {
    const json j = json::parse(
        R"({"textures":{"all":{"sprite":"minecraft:block/blue_stained_glass","force_translucent":true},"particle":"minecraft:block/x"}})");
    Model model;
    model.deserialize(j);
    test_util::check(model.textures["all"] ==
                         "minecraft:block/blue_stained_glass",
                     "object texture sprite not read");
    test_util::check(model.forceTranslucent.count("all") == 1,
                     "force_translucent not read");
    test_util::check(model.textures["particle"] == "minecraft:block/x",
                     "plain string texture broken");
    const json out = model.serialize();
    test_util::check(out["textures"]["all"]["sprite"] ==
                             "minecraft:block/blue_stained_glass" &&
                         out["textures"]["all"]["force_translucent"] == true,
                     "object texture not written back in object form");
    test_util::check(out["textures"]["particle"] == "minecraft:block/x",
                     "plain string texture written in wrong form");
  });

  return suite.finish();
}
