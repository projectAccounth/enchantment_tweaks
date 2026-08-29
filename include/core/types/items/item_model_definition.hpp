#pragma once

#include "core/types/common.hpp"
#include "core/types/items/item_model.hpp"
#include "libraries/nlohmann/json.hpp"
#include <memory>

using nlohmann::json;
using namespace util;

namespace enchantment_tweaks::item {

class ItemModelDefinition final : public JsonSerializable {
public:
  bool handAnimationOnSwap = true; // optional, default true
  bool oversizedInGui = false;     // optional, default false
  double swapAnimationScale = 1.0; // optional, default 1.0
  std::unique_ptr<ItemModel> model;

  json serialize() const override {
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

  void deserialize(const json &j) override {
    handAnimationOnSwap =
        readOptional<bool>(j, "hand_animation_on_swap").value_or(true);
    oversizedInGui = readOptional<bool>(j, "oversized_in_gui").value_or(false);
    swapAnimationScale =
        readOptional<double>(j, "swap_animation_scale").value_or(1.0);
    model = j.contains("model") ? ItemModel::create(j.at("model")) : nullptr;
  }
};

} // namespace enchantment_tweaks::item
