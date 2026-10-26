#include "core/types/items/item_model.hpp"
#include "core/types/items/item_model_definition.hpp"
#include "core/util/debug.hpp"
#include "libraries/nlohmann/json.hpp"
#include "test_utils.hpp"

#include <cstdlib>
#include <string>

using namespace enchantment_tweaks;
using namespace enchantment_tweaks::debug;
using namespace enchantment_tweaks::item;
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

  run("item model type enums", [] {
    test_util::check(itemModelTypeFromString("minecraft:model") ==
                         ItemModelType::Model,
                     "model type mismatch");
    test_util::check(itemModelTypeFromString("minecraft:special") ==
                         ItemModelType::Special,
                     "special type mismatch");
    test_util::check(itemModelTypeFromString("minecraft:bed") ==
                         ItemModelType::Bed,
                     "bed type mismatch");
    test_util::check(toString(ItemModelType::Condition) ==
                         "minecraft:condition",
                     "toString mismatch");
  });

  run("boolean property enums including on_shelf", [] {
    auto p1 = booleanPropertyTypeFromString("minecraft:on_shelf");
    test_util::check(p1 == BooleanPropertyType::OnShelf,
                     "on_shelf namespaced mismatch");
    auto p2 = booleanPropertyTypeFromString("on_shelf");
    test_util::check(p2 == BooleanPropertyType::OnShelf,
                     "on_shelf bare mismatch");
    test_util::check(toString(BooleanPropertyType::Broken) ==
                         "minecraft:broken",
                     "toString broken mismatch");
  });

  run("select and numeric property enums", [] {
    test_util::check(
        selectPropertyTypeFromString("minecraft:display_context") ==
            SelectPropertyType::DisplayContext,
        "display_context mismatch");
    test_util::check(numericPropertyTypeFromString("minecraft:damage") ==
                         NumericPropertyType::Damage,
                     "damage mismatch");
    test_util::check(numericPropertyTypeFromString("minecraft:use_duration") ==
                         NumericPropertyType::UseDuration,
                     "use_duration mismatch");
  });

  run("model item model create/serialize", [] {
    json j = {{"type", "minecraft:model"}, {"model", "minecraft:item/stone"}};
    auto m = ItemModel::create(j);
    test_util::check(m != nullptr, "model should be created");
    json s = m->serialize();
    test_util::check(s["type"] == "minecraft:model", "type mismatch");
    test_util::check(s["model"] == "minecraft:item/stone", "model mismatch");
  });

  run("empty item model", [] {
    json j = {{"type", "minecraft:empty"}};
    auto m = ItemModel::create(j);
    test_util::check(m != nullptr, "empty model should be created");
    json s = m->serialize();
    test_util::check(s["type"] == "minecraft:empty", "type mismatch");
  });

  run("item model definition roundtrip", [] {
    json j = {
        {"model",
         {{"type", "minecraft:model"}, {"model", "minecraft:item/diamond"}}}};
    ItemModelDefinition def;
    def.deserialize(j);
    json s = def.serialize();
    test_util::check(s.contains("model"), "definition should have model");
    test_util::check(s["model"]["type"] == "minecraft:model",
                     "nested type mismatch");
  });

  debug::Logger::info() << "[summary] " << passed << "/" << total
                        << " tests passed\n";
  test_util::end();
  return passed == total ? 0 : 1;
}
