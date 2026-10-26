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
  size_t passed = 0;
  size_t total = 0;
  auto run = [&](const std::string &name, auto &&fn) {
    ++total;
    if (test_util::runCase(name, std::forward<decltype(fn)>(fn))) {
      ++passed;
    }
  };

  test_util::begin();

  run("uv serialize/deserialize", [] {
    Uv uv(0, 0, 16, 16);
    json j = uv.serialize();
    test_util::check(j.is_array() && j.size() == 4, "uv should be array of 4");
    Uv uv2;
    uv2.deserialize(j);
    test_util::check(uv2.uFrom == 0 && uv2.uTo == 16, "uv values mismatch");
  });

  run("face serialize/deserialize with cullface and rotation", [] {
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

  run("element serialize/deserialize", [] {
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

  run("display transform serialize/deserialize", [] {
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

  run("model serialize/deserialize with textures and display", [] {
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

  debug::Logger::info() << "[summary] " << passed << "/" << total
                        << " tests passed\n";
  test_util::end();
  return passed == total ? 0 : 1;
}
