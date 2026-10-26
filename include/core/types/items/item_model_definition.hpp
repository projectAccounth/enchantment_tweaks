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

  json serialize() const override;
  void deserialize(const json &j) override;
};

} // namespace enchantment_tweaks::item
