#pragma once

#include "core/types/common.hpp"
#include "core/util/json/utils.hpp"
#include "types.hpp"

#include <memory>
#include <string>

using namespace util;

namespace enchantment_tweaks::item {

enum class TintSourceType : uint8_t {
  Constant,
  Dye,
  Firework,
  Grass,
  MapColor,
  Potion,
  Team,
  CustomModelData,
};

inline const EnumTable<TintSourceType> &tintSourceTypeTable() {
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
inline std::string toString(TintSourceType t) {
  return enumToString(t, tintSourceTypeTable());
}
inline TintSourceType tintSourceTypeFromString(const std::string &s) {
  return enumFromString(s, tintSourceTypeTable());
}

class TintSource : public JsonSerializable {
public:
  explicit TintSource(TintSourceType type) : type_(type) {}
  ~TintSource() override = default;

  TintSourceType type() const { return type_; }

  json serialize() const override {
    json j = json::object();
    j["type"] = toString(type_);
    serializeBody(j);
    return j;
  }

  void deserialize(const json &j) override { deserializeBody(j); }

  static std::unique_ptr<TintSource> create(const json &j);

protected:
  virtual void serializeBody(json &j) const = 0;
  virtual void deserializeBody(const json &j) = 0;

private:
  TintSourceType type_;
};

class ConstantTintSource final : public TintSource {
public:
  ConstantTintSource() : TintSource(TintSourceType::Constant) {}
  RgbColor value;

protected:
  void serializeBody(json &j) const override { j["value"] = value.serialize(); }
  void deserializeBody(const json &j) override {
    value.deserialize(j.at("value"));
  }
};

// Shared for dye / firework / map_color / potion / team
class DefaultColorTintSource : public TintSource {
public:
  explicit DefaultColorTintSource(TintSourceType type) : TintSource(type) {}
  RgbColor defaultColor;

protected:
  void serializeBody(json &j) const override {
    j["default"] = defaultColor.serialize();
  }
  void deserializeBody(const json &j) override {
    defaultColor.deserialize(j.at("default"));
  }
};

class DyeTintSource final : public DefaultColorTintSource {
public:
  DyeTintSource() : DefaultColorTintSource(TintSourceType::Dye) {}
};

class FireworkTintSource final : public DefaultColorTintSource {
public:
  FireworkTintSource() : DefaultColorTintSource(TintSourceType::Firework) {}
};

class MapColorTintSource final : public DefaultColorTintSource {
public:
  MapColorTintSource() : DefaultColorTintSource(TintSourceType::MapColor) {}
};

class PotionTintSource final : public DefaultColorTintSource {
public:
  PotionTintSource() : DefaultColorTintSource(TintSourceType::Potion) {}
};

class TeamTintSource final : public DefaultColorTintSource {
public:
  TeamTintSource() : DefaultColorTintSource(TintSourceType::Team) {}
};

class GrassTintSource final : public TintSource {
public:
  GrassTintSource() : TintSource(TintSourceType::Grass) {}
  double temperature = 0.0;
  double downfall = 0.0;

protected:
  void serializeBody(json &j) const override {
    j["temperature"] = temperature;
    j["downfall"] = downfall;
  }
  void deserializeBody(const json &j) override {
    temperature = j.at("temperature").get<double>();
    downfall = j.at("downfall").get<double>();
  }
};

class CustomModelDataTintSource final : public TintSource {
public:
  CustomModelDataTintSource() : TintSource(TintSourceType::CustomModelData) {}
  int index = 0; // optional, default 0
  RgbColor defaultColor;

protected:
  void serializeBody(json &j) const override {
    if (index != 0) {
      j["index"] = index;
    }
    j["default"] = defaultColor.serialize();
  }
  void deserializeBody(const json &j) override {
    index = readOptional<int>(j, "index").value_or(0);
    defaultColor.deserialize(j.at("default"));
  }
};

inline std::unique_ptr<TintSource> TintSource::create(const json &j) {
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
