#include "core/types/items/tint_source.hpp"
#include "core/util/json/utils.hpp"

#include <memory>
#include <string>

namespace enchantment_tweaks::item {

const EnumTable<TintSourceType> &tintSourceTypeTable() {
  static const EnumTable<TintSourceType> table = {
      {TintSourceType::Constant, "minecraft:constant"},
      {TintSourceType::Dye, "minecraft:dye"},
      {TintSourceType::Firework, "minecraft:firework"},
      {TintSourceType::Grass, "minecraft:grass"},
      {TintSourceType::MapColor, "minecraft:map_color"},
      {TintSourceType::Potion, "minecraft:potion"},
      {TintSourceType::Team, "minecraft:team"},
      {TintSourceType::CustomModelData, "minecraft:custom_model_data"},
  };
  return table;
}

std::string toString(TintSourceType t) {
  return enumToString(t, tintSourceTypeTable());
}

TintSourceType tintSourceTypeFromString(const std::string &s) {
  return enumFromString(s, tintSourceTypeTable());
}

json TintSource::serialize() const {
  json j = json::object();
  j["type"] = toString(type_);
  serializeBody(j);
  return j;
}

void TintSource::deserialize(const json &j) { deserializeBody(j); }

void ConstantTintSource::serializeBody(json &j) const {
  j["value"] = value.serialize();
}

void ConstantTintSource::deserializeBody(const json &j) {
  value.deserialize(j.at("value"));
}

void DefaultColorTintSource::serializeBody(json &j) const {
  auto color = defaultColor.serialize();
  j["default"] = color;
}

void DefaultColorTintSource::deserializeBody(const json &j) {
  defaultColor.deserialize(j.at("default"));
}

void GrassTintSource::serializeBody(json &j) const {
  j["temperature"] = temperature;
  j["downfall"] = downfall;
}

void GrassTintSource::deserializeBody(const json &j) {
  temperature = j.at("temperature").get<double>();
  downfall = j.at("downfall").get<double>();
}

void CustomModelDataTintSource::serializeBody(json &j) const {
  if (index != 0) {
    j["index"] = index;
  }
  j["default"] = defaultColor.serialize();
}

void CustomModelDataTintSource::deserializeBody(const json &j) {
  index = readOptional<int>(j, "index").value_or(0);
  defaultColor.deserialize(j.at("default"));
}

std::unique_ptr<TintSource> TintSource::create(const json &j) {
  const TintSourceType type =
      tintSourceTypeFromString(j.at("type").get<std::string>());
  std::unique_ptr<TintSource> result;
  switch (type) {
  case TintSourceType::Constant:
    result = std::make_unique<ConstantTintSource>();
    break;
  case TintSourceType::Dye:
    result = std::make_unique<DyeTintSource>();
    break;
  case TintSourceType::Firework:
    result = std::make_unique<FireworkTintSource>();
    break;
  case TintSourceType::Grass:
    result = std::make_unique<GrassTintSource>();
    break;
  case TintSourceType::MapColor:
    result = std::make_unique<MapColorTintSource>();
    break;
  case TintSourceType::Potion:
    result = std::make_unique<PotionTintSource>();
    break;
  case TintSourceType::Team:
    result = std::make_unique<TeamTintSource>();
    break;
  case TintSourceType::CustomModelData:
    result = std::make_unique<CustomModelDataTintSource>();
    break;
  }
  result->deserialize(j);
  return result;
}

} // namespace enchantment_tweaks::item
