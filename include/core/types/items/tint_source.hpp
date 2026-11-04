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

const EnumTable<TintSourceType> &tintSourceTypeTable();
std::string toString(TintSourceType t);
TintSourceType tintSourceTypeFromString(const std::string &s);

class TintSource : public JsonSerializable {
public:
  explicit TintSource(TintSourceType type) : type_(type) {}
  ~TintSource() override = default;

  TintSourceType type() const { return type_; }

  json serialize() const override;
  void deserialize(const json &j) override;

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
  void serializeBody(json &j) const override;
  void deserializeBody(const json &j) override;
};

/// Shared for dye / firework / map_color / potion / team
class DefaultColorTintSource : public TintSource {
public:
  explicit DefaultColorTintSource(TintSourceType type) : TintSource(type) {}
  RgbColor defaultColor;

protected:
  void serializeBody(json &j) const override;
  void deserializeBody(const json &j) override;
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
  void serializeBody(json &j) const override;
  void deserializeBody(const json &j) override;
};

class CustomModelDataTintSource final : public TintSource {
public:
  CustomModelDataTintSource() : TintSource(TintSourceType::CustomModelData) {}
  int index = 0; // optional, default 0
  RgbColor defaultColor;

protected:
  void serializeBody(json &j) const override;
  void deserializeBody(const json &j) override;
};

} // namespace enchantment_tweaks::item
