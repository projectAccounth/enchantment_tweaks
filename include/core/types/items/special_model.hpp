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

// what the fuck did I just do
// a near-complete mirror of the specified schema
inline const EnumTable<SpecialModelType> &specialModelTypeTable() {
  static const EnumTable<SpecialModelType> table = {
      {SpecialModelType::Banner, "minecraft:banner"},
      {SpecialModelType::Bell, "minecraft:bell"},
      {SpecialModelType::Book, "minecraft:book"},
      {SpecialModelType::Chest, "minecraft:chest"},
      {SpecialModelType::Conduit, "minecraft:conduit"},
      {SpecialModelType::CopperGolemStatue, "minecraft:copper_golem_statue"},
      {SpecialModelType::DecoratedPot, "minecraft:decorated_pot"},
      {SpecialModelType::EndCube, "minecraft:end_cube"},
      {SpecialModelType::Head, "minecraft:head"},
      {SpecialModelType::PlayerHead, "minecraft:player_head"},
      {SpecialModelType::Shield, "minecraft:shield"},
      {SpecialModelType::ShulkerBox, "minecraft:shulker_box"},
      {SpecialModelType::Trident, "minecraft:trident"},
  };
  return table;
}
inline std::string toString(SpecialModelType t) {
  return enumToString(t, specialModelTypeTable());
}
inline SpecialModelType specialModelTypeFromString(const std::string &s) {
  return enumFromString(s, specialModelTypeTable());
}

enum class Attachment : uint8_t { Ground, Wall };
inline const EnumTable<Attachment> &attachmentTable() {
  static const EnumTable<Attachment> table = {
      {Attachment::Ground, "ground"},
      {Attachment::Wall, "wall"},
  };
  return table;
}
inline std::string toString(Attachment t) {
  return enumToString(t, attachmentTable());
}
inline Attachment attachmentFromString(const std::string &s) {
  return enumFromString(s, attachmentTable());
}

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
inline const EnumTable<DyeColor> &dyeColorTable() {
  static const EnumTable<DyeColor> table = {
      {DyeColor::White, "white"},
      {DyeColor::Orange, "orange"},
      {DyeColor::Magenta, "magenta"},
      {DyeColor::LightBlue, "light_blue"},
      {DyeColor::Yellow, "yellow"},
      {DyeColor::Lime, "lime"},
      {DyeColor::Pink, "pink"},
      {DyeColor::Gray, "gray"},
      {DyeColor::LightGray, "light_gray"},
      {DyeColor::Cyan, "cyan"},
      {DyeColor::Purple, "purple"},
      {DyeColor::Blue, "blue"},
      {DyeColor::Brown, "brown"},
      {DyeColor::Green, "green"},
      {DyeColor::Red, "red"},
      {DyeColor::Black, "black"},
  };
  return table;
}
inline std::string toString(DyeColor t) {
  return enumToString(t, dyeColorTable());
}
inline DyeColor dyeColorFromString(const std::string &s) {
  return enumFromString(s, dyeColorTable());
}

enum class HeadKind : uint8_t {
  Skeleton,
  WitherSkeleton,
  Player,
  Zombie,
  Creeper,
  Piglin,
  Dragon
};
inline const EnumTable<HeadKind> &headKindTable() {
  static const EnumTable<HeadKind> table = {
      {HeadKind::Skeleton, "skeleton"},
      {HeadKind::WitherSkeleton, "wither_skeleton"},
      {HeadKind::Player, "player"},
      {HeadKind::Zombie, "zombie"},
      {HeadKind::Creeper, "creeper"},
      {HeadKind::Piglin, "piglin"},
      {HeadKind::Dragon, "dragon"},
  };
  return table;
}
inline std::string toString(HeadKind t) {
  return enumToString(t, headKindTable());
}
inline HeadKind headKindFromString(const std::string &s) {
  return enumFromString(s, headKindTable());
}

enum class ChestType : uint8_t { Single, Left, Right };
inline const EnumTable<ChestType> &chestTypeTable() {
  static const EnumTable<ChestType> table = {
      {ChestType::Single, "single"},
      {ChestType::Left, "left"},
      {ChestType::Right, "right"},
  };
  return table;
}
inline std::string toString(ChestType t) {
  return enumToString(t, chestTypeTable());
}
inline ChestType chestTypeFromString(const std::string &s) {
  return enumFromString(s, chestTypeTable());
}

enum class CopperGolemPose : uint8_t { Sitting, Running, Star, Standing };
inline const EnumTable<CopperGolemPose> &copperGolemPoseTable() {
  static const EnumTable<CopperGolemPose> table = {
      {CopperGolemPose::Sitting, "sitting"},
      {CopperGolemPose::Running, "running"},
      {CopperGolemPose::Star, "star"},
      {CopperGolemPose::Standing, "standing"},
  };
  return table;
}
inline std::string toString(CopperGolemPose t) {
  return enumToString(t, copperGolemPoseTable());
}
inline CopperGolemPose copperGolemPoseFromString(const std::string &s) {
  return enumFromString(s, copperGolemPoseTable());
}

enum class EndCubeEffect : uint8_t { Gateway, Portal };
inline const EnumTable<EndCubeEffect> &endCubeEffectTable() {
  static const EnumTable<EndCubeEffect> table = {
      {EndCubeEffect::Gateway, "gateway"},
      {EndCubeEffect::Portal, "portal"},
  };
  return table;
}
inline std::string toString(EndCubeEffect t) {
  return enumToString(t, endCubeEffectTable());
}
inline EndCubeEffect endCubeEffectFromString(const std::string &s) {
  return enumFromString(s, endCubeEffectTable());
}

// SpecialModel hierarchy

class SpecialModel : public JsonSerializable {
public:
  explicit SpecialModel(SpecialModelType type) : type_(type) {}
  ~SpecialModel() override = default;

  SpecialModelType type() const { return type_; }

  json serialize() const override {
    json j = json::object();
    j["type"] = toString(type_);
    serializeBody(j);
    return j;
  }
  void deserialize(const json &j) override { deserializeBody(j); }

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
  void serializeBody(json &j) const override {
    if (attachment != Attachment::Ground) {
      j["attachment"] = toString(attachment);
    }
    j["color"] = toString(color);
  }
  void deserializeBody(const json &j) override {
    if (auto s = readOptional<std::string>(j, "attachment")) {
      attachment = attachmentFromString(*s);
    } else {
      attachment = Attachment::Ground;
    }
    color = dyeColorFromString(j.at("color").get<std::string>());
  }
};

class BellSpecialModel final : public SpecialModel {
public:
  BellSpecialModel() : SpecialModel(SpecialModelType::Bell) {}

protected:
  void serializeBody(json & /*j*/) const override {}
  void deserializeBody(const json & /*j*/) override {}
};

class BookSpecialModel final : public SpecialModel {
public:
  BookSpecialModel() : SpecialModel(SpecialModelType::Book) {}
  int openAngle = 0;
  double page1 = 0.0;
  double page2 = 0.0;

protected:
  void serializeBody(json &j) const override {
    j["open_angle"] = openAngle;
    j["page1"] = page1;
    j["page2"] = page2;
  }
  void deserializeBody(const json &j) override {
    openAngle = j.at("open_angle").get<int>();
    page1 = j.at("page1").get<double>();
    page2 = j.at("page2").get<double>();
  }
};

class ChestSpecialModel final : public SpecialModel {
public:
  ChestSpecialModel() : SpecialModel(SpecialModelType::Chest) {}
  std::string texture;
  ChestType chestType = ChestType::Single; // optional, default single
  double openness = 0.0;                   // optional, default 0.0

protected:
  void serializeBody(json &j) const override {
    j["texture"] = texture;
    if (chestType != ChestType::Single) {
      j["chest_type"] = toString(chestType);
    }
    if (openness != 0.0) {
      j["openness"] = openness;
    }
  }
  void deserializeBody(const json &j) override {
    texture = j.at("texture").get<std::string>();
    if (auto s = readOptional<std::string>(j, "chest_type")) {
      chestType = chestTypeFromString(*s);
    } else {
      chestType = ChestType::Single;
    }
    openness = readOptional<double>(j, "openness").value_or(0.0);
  }
};

class ConduitSpecialModel final : public SpecialModel {
public:
  ConduitSpecialModel() : SpecialModel(SpecialModelType::Conduit) {}

protected:
  void serializeBody(json & /*j*/) const override {}
  void deserializeBody(const json & /*j*/) override {}
};

class CopperGolemStatueSpecialModel final : public SpecialModel {
public:
  CopperGolemStatueSpecialModel()
      : SpecialModel(SpecialModelType::CopperGolemStatue) {}
  CopperGolemPose pose = CopperGolemPose::Standing;
  std::string texture; // includes ".png" suffix

protected:
  void serializeBody(json &j) const override {
    j["pose"] = toString(pose);
    j["texture"] = texture;
  }
  void deserializeBody(const json &j) override {
    pose = copperGolemPoseFromString(j.at("pose").get<std::string>());
    texture = j.at("texture").get<std::string>();
  }
};

class DecoratedPotSpecialModel final : public SpecialModel {
public:
  DecoratedPotSpecialModel() : SpecialModel(SpecialModelType::DecoratedPot) {}

protected:
  void serializeBody(json & /*j*/) const override {}
  void deserializeBody(const json & /*j*/) override {}
};

class EndCubeSpecialModel final : public SpecialModel {
public:
  EndCubeSpecialModel() : SpecialModel(SpecialModelType::EndCube) {}
  EndCubeEffect effect = EndCubeEffect::Gateway;

protected:
  void serializeBody(json &j) const override { j["effect"] = toString(effect); }
  void deserializeBody(const json &j) override {
    effect = endCubeEffectFromString(j.at("effect").get<std::string>());
  }
};

class HeadSpecialModel final : public SpecialModel {
public:
  HeadSpecialModel() : SpecialModel(SpecialModelType::Head) {}
  HeadKind kind = HeadKind::Skeleton;
  std::optional<std::string> texture; // optional; defaults based on kind
  double animation = 0.0;             // optional, default 0.0

protected:
  void serializeBody(json &j) const override {
    j["kind"] = toString(kind);
    writeOptional(j, "texture", texture);
    if (animation != 0.0) {
      j["animation"] = animation;
    }
  }
  void deserializeBody(const json &j) override {
    kind = headKindFromString(j.at("kind").get<std::string>());
    texture = readOptional<std::string>(j, "texture");
    animation = readOptional<double>(j, "animation").value_or(0.0);
  }
};

class PlayerHeadSpecialModel final : public SpecialModel {
public:
  PlayerHeadSpecialModel() : SpecialModel(SpecialModelType::PlayerHead) {}

protected:
  void serializeBody(json & /*j*/) const override {}
  void deserializeBody(const json & /*j*/) override {}
};

class ShieldSpecialModel final : public SpecialModel {
public:
  ShieldSpecialModel() : SpecialModel(SpecialModelType::Shield) {}

protected:
  void serializeBody(json & /*j*/) const override {}
  void deserializeBody(const json & /*j*/) override {}
};

class ShulkerBoxSpecialModel final : public SpecialModel {
public:
  ShulkerBoxSpecialModel() : SpecialModel(SpecialModelType::ShulkerBox) {}
  std::string texture;
  double openness = 0.0; // optional, default 0.0

protected:
  void serializeBody(json &j) const override {
    j["texture"] = texture;
    if (openness != 0.0) {
      j["openness"] = openness;
    }
  }
  void deserializeBody(const json &j) override {
    texture = j.at("texture").get<std::string>();
    openness = readOptional<double>(j, "openness").value_or(0.0);
  }
};

class TridentSpecialModel final : public SpecialModel {
public:
  TridentSpecialModel() : SpecialModel(SpecialModelType::Trident) {}

protected:
  void serializeBody(json & /*j*/) const override {}
  void deserializeBody(const json & /*j*/) override {}
};

inline std::unique_ptr<SpecialModel> SpecialModel::create(const json &j) {
  const SpecialModelType type =
      specialModelTypeFromString(j.at("type").get<std::string>());
  std::unique_ptr<SpecialModel> result;
  switch (type) {
  case SpecialModelType::Banner:
    result = std::make_unique<BannerSpecialModel>();
    break;
  case SpecialModelType::Bell:
    result = std::make_unique<BellSpecialModel>();
    break;
  case SpecialModelType::Book:
    result = std::make_unique<BookSpecialModel>();
    break;
  case SpecialModelType::Chest:
    result = std::make_unique<ChestSpecialModel>();
    break;
  case SpecialModelType::Conduit:
    result = std::make_unique<ConduitSpecialModel>();
    break;
  case SpecialModelType::CopperGolemStatue:
    result = std::make_unique<CopperGolemStatueSpecialModel>();
    break;
  case SpecialModelType::DecoratedPot:
    result = std::make_unique<DecoratedPotSpecialModel>();
    break;
  case SpecialModelType::EndCube:
    result = std::make_unique<EndCubeSpecialModel>();
    break;
  case SpecialModelType::Head:
    result = std::make_unique<HeadSpecialModel>();
    break;
  case SpecialModelType::PlayerHead:
    result = std::make_unique<PlayerHeadSpecialModel>();
    break;
  case SpecialModelType::Shield:
    result = std::make_unique<ShieldSpecialModel>();
    break;
  case SpecialModelType::ShulkerBox:
    result = std::make_unique<ShulkerBoxSpecialModel>();
    break;
  case SpecialModelType::Trident:
    result = std::make_unique<TridentSpecialModel>();
    break;
  }
  result->deserialize(j);
  return result;
}

} // namespace enchantment_tweaks::item
