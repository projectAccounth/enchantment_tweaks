#include "core/types/items/item_model_definition.hpp"
#include "core/types/items/item_model.hpp"
#include "core/util/json/utils.hpp"

namespace enchantment_tweaks::item {

json ItemModelDefinition::serialize() const {
  json j = json::object();
  if (!handAnimationOnSwap) {
    j["hand_animation_on_swap"] = handAnimationOnSwap;
  }
  if (oversizedInGui) {
    j["oversized_in_gui"] = oversizedInGui;
  }
  if (swapAnimationScale != 1.0) {
    j["swap_animation_scale"] = swapAnimationScale;
  }
  if (model) {
    j["model"] = model->serialize();
  }
  return j;
}

void ItemModelDefinition::deserialize(const json &j) {
  handAnimationOnSwap =
      readOptional<bool>(j, "hand_animation_on_swap").value_or(true);
  oversizedInGui = readOptional<bool>(j, "oversized_in_gui").value_or(false);
  swapAnimationScale =
      readOptional<double>(j, "swap_animation_scale").value_or(1.0);
  model = j.contains("model") ? ItemModel::create(j.at("model")) : nullptr;
}

} // namespace enchantment_tweaks::item
