#pragma once

#include "core/types/common.hpp"
#include "core/util/json/utils.hpp"

#include <memory>
#include <optional>
#include <string>

using namespace util;

namespace enchantment_tweaks::item {

enum class SpecialModelType : uint8_t {
  Banner,
  Bell,
  Book,
  Chest,
  Conduit,
  CopperGolemStatue,
  DecoratedPot,
  EndCube,
  Head,
  PlayerHead,
  Shield,
  ShulkerBox,
  Trident,
};

/// Mirrors Minecraft's special model schema.
const EnumTable<SpecialModelType> &specialModelTypeTable();
std::string toString(SpecialModelType t);
SpecialModelType specialModelTypeFromString(const std::string &s);

enum class Attachment : uint8_t { Ground, Wall };
const EnumTable<Attachment> &attachmentTable();
std::string toString(Attachment t);
Attachment attachmentFromString(const std::string &s);

enum class DyeColor : uint8_t {
  White,
  Orange,
  Magenta,
  LightBlue,
  Yellow,
  Lime,
  Pink,
  Gray,
  LightGray,
  Cyan,
  Purple,
  Blue,
  Brown,
  Green,
  Red,
  Black,
};
const EnumTable<DyeColor> &dyeColorTable();
std::string toString(DyeColor t);
DyeColor dyeColorFromString(const std::string &s);

enum class HeadKind : uint8_t {
  Skeleton,
  WitherSkeleton,
  Player,
  Zombie,
  Creeper,
  Piglin,
  Dragon
};
const EnumTable<HeadKind> &headKindTable();
std::string toString(HeadKind t);
HeadKind headKindFromString(const std::string &s);

enum class ChestType : uint8_t { Single, Left, Right };
const EnumTable<ChestType> &chestTypeTable();
std::string toString(ChestType t);
ChestType chestTypeFromString(const std::string &s);

enum class CopperGolemPose : uint8_t { Sitting, Running, Star, Standing };
const EnumTable<CopperGolemPose> &copperGolemPoseTable();
std::string toString(CopperGolemPose t);
CopperGolemPose copperGolemPoseFromString(const std::string &s);

enum class EndCubeEffect : uint8_t { Gateway, Portal };
const EnumTable<EndCubeEffect> &endCubeEffectTable();
std::string toString(EndCubeEffect t);
EndCubeEffect endCubeEffectFromString(const std::string &s);

class SpecialModel : public JsonSerializable {
public:
  explicit SpecialModel(SpecialModelType type) : type_(type) {}
  ~SpecialModel() override = default;

  SpecialModelType type() const { return type_; }

  json serialize() const override;
  void deserialize(const json &j) override;

  static std::unique_ptr<SpecialModel> create(const json &j);

protected:
  virtual void serializeBody(json &j) const = 0;
  virtual void deserializeBody(const json &j) = 0;

private:
  SpecialModelType type_;
};

class BannerSpecialModel final : public SpecialModel {
public:
  BannerSpecialModel() : SpecialModel(SpecialModelType::Banner) {}
  Attachment attachment = Attachment::Ground; // optional, default ground
  DyeColor color = DyeColor::White;

protected:
  void serializeBody(json &j) const override;
  void deserializeBody(const json &j) override;
};

class BellSpecialModel final : public SpecialModel {
public:
  BellSpecialModel() : SpecialModel(SpecialModelType::Bell) {}

protected:
  void serializeBody(json &j) const override;
  void deserializeBody(const json &j) override;
};

class BookSpecialModel final : public SpecialModel {
public:
  BookSpecialModel() : SpecialModel(SpecialModelType::Book) {}
  double openAngle = 0;
  double page1 = 0.0;
  double page2 = 0.0;

protected:
  void serializeBody(json &j) const override;
  void deserializeBody(const json &j) override;
};

class ChestSpecialModel final : public SpecialModel {
public:
  ChestSpecialModel() : SpecialModel(SpecialModelType::Chest) {}
  std::string texture;
  ChestType chestType = ChestType::Single; // optional, default single
  double openness = 0.0;                   // optional, default 0.0

protected:
  void serializeBody(json &j) const override;
  void deserializeBody(const json &j) override;
};

class ConduitSpecialModel final : public SpecialModel {
public:
  ConduitSpecialModel() : SpecialModel(SpecialModelType::Conduit) {}

protected:
  void serializeBody(json &j) const override;
  void deserializeBody(const json &j) override;
};

class CopperGolemStatueSpecialModel final : public SpecialModel {
public:
  CopperGolemStatueSpecialModel()
      : SpecialModel(SpecialModelType::CopperGolemStatue) {}
  CopperGolemPose pose = CopperGolemPose::Standing;
  std::string texture; // includes ".png" suffix

protected:
  void serializeBody(json &j) const override;
  void deserializeBody(const json &j) override;
};

class DecoratedPotSpecialModel final : public SpecialModel {
public:
  DecoratedPotSpecialModel() : SpecialModel(SpecialModelType::DecoratedPot) {}

protected:
  void serializeBody(json &j) const override;
  void deserializeBody(const json &j) override;
};

class EndCubeSpecialModel final : public SpecialModel {
public:
  EndCubeSpecialModel() : SpecialModel(SpecialModelType::EndCube) {}
  EndCubeEffect effect = EndCubeEffect::Gateway;

protected:
  void serializeBody(json &j) const override;
  void deserializeBody(const json &j) override;
};

class HeadSpecialModel final : public SpecialModel {
public:
  HeadSpecialModel() : SpecialModel(SpecialModelType::Head) {}
  HeadKind kind = HeadKind::Skeleton;
  std::optional<std::string> texture; // optional; defaults based on kind
  double animation = 0.0;             // optional, default 0.0

protected:
  void serializeBody(json &j) const override;
  void deserializeBody(const json &j) override;
};

class PlayerHeadSpecialModel final : public SpecialModel {
public:
  PlayerHeadSpecialModel() : SpecialModel(SpecialModelType::PlayerHead) {}

protected:
  void serializeBody(json &j) const override;
  void deserializeBody(const json &j) override;
};

class ShieldSpecialModel final : public SpecialModel {
public:
  ShieldSpecialModel() : SpecialModel(SpecialModelType::Shield) {}

protected:
  void serializeBody(json &j) const override;
  void deserializeBody(const json &j) override;
};

class ShulkerBoxSpecialModel final : public SpecialModel {
public:
  ShulkerBoxSpecialModel() : SpecialModel(SpecialModelType::ShulkerBox) {}
  std::string texture;
  double openness = 0.0; // optional, default 0.0

protected:
  void serializeBody(json &j) const override;
  void deserializeBody(const json &j) override;
};

class TridentSpecialModel final : public SpecialModel {
public:
  TridentSpecialModel() : SpecialModel(SpecialModelType::Trident) {}

protected:
  void serializeBody(json &j) const override;
  void deserializeBody(const json &j) override;
};

} // namespace enchantment_tweaks::item
