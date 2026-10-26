#include "core/types/items/special_model.hpp"
#include "core/util/json/utils.hpp"

#include <memory>
#include <string>

namespace enchantment_tweaks::item {

const EnumTable<SpecialModelType> &specialModelTypeTable() {
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

std::string toString(SpecialModelType t) {
  return enumToString(t, specialModelTypeTable());
}

SpecialModelType specialModelTypeFromString(const std::string &s) {
  return enumFromString(s, specialModelTypeTable());
}

const EnumTable<Attachment> &attachmentTable() {
  static const EnumTable<Attachment> table = {
      {Attachment::Ground, "ground"},
      {Attachment::Wall, "wall"},
  };
  return table;
}

std::string toString(Attachment t) {
  return enumToString(t, attachmentTable());
}
Attachment attachmentFromString(const std::string &s) {
  return enumFromString(s, attachmentTable());
}

const EnumTable<DyeColor> &dyeColorTable() {
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

std::string toString(DyeColor t) { return enumToString(t, dyeColorTable()); }
DyeColor dyeColorFromString(const std::string &s) {
  return enumFromString(s, dyeColorTable());
}

const EnumTable<HeadKind> &headKindTable() {
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

std::string toString(HeadKind t) { return enumToString(t, headKindTable()); }
HeadKind headKindFromString(const std::string &s) {
  return enumFromString(s, headKindTable());
}

const EnumTable<ChestType> &chestTypeTable() {
  static const EnumTable<ChestType> table = {
      {ChestType::Single, "single"},
      {ChestType::Left, "left"},
      {ChestType::Right, "right"},
  };
  return table;
}

std::string toString(ChestType t) { return enumToString(t, chestTypeTable()); }
ChestType chestTypeFromString(const std::string &s) {
  return enumFromString(s, chestTypeTable());
}

const EnumTable<CopperGolemPose> &copperGolemPoseTable() {
  static const EnumTable<CopperGolemPose> table = {
      {CopperGolemPose::Sitting, "sitting"},
      {CopperGolemPose::Running, "running"},
      {CopperGolemPose::Star, "star"},
      {CopperGolemPose::Standing, "standing"},
  };
  return table;
}

std::string toString(CopperGolemPose t) {
  return enumToString(t, copperGolemPoseTable());
}
CopperGolemPose copperGolemPoseFromString(const std::string &s) {
  return enumFromString(s, copperGolemPoseTable());
}

const EnumTable<EndCubeEffect> &endCubeEffectTable() {
  static const EnumTable<EndCubeEffect> table = {
      {EndCubeEffect::Gateway, "gateway"},
      {EndCubeEffect::Portal, "portal"},
  };
  return table;
}

std::string toString(EndCubeEffect t) {
  return enumToString(t, endCubeEffectTable());
}
EndCubeEffect endCubeEffectFromString(const std::string &s) {
  return enumFromString(s, endCubeEffectTable());
}

json SpecialModel::serialize() const {
  json j = json::object();
  j["type"] = toString(type_);
  serializeBody(j);
  return j;
}

void SpecialModel::deserialize(const json &j) { deserializeBody(j); }

void BannerSpecialModel::serializeBody(json &j) const {
  if (attachment != Attachment::Ground) {
    j["attachment"] = toString(attachment);
  }
  j["color"] = toString(color);
}

void BannerSpecialModel::deserializeBody(const json &j) {
  if (auto s = readOptional<std::string>(j, "attachment")) {
    attachment = attachmentFromString(*s);
  } else {
    attachment = Attachment::Ground;
  }
  color = dyeColorFromString(j.at("color").get<std::string>());
}

void BellSpecialModel::serializeBody(json &) const {}
void BellSpecialModel::deserializeBody(const json &) {}

void BookSpecialModel::serializeBody(json &j) const {
  j["open_angle"] = openAngle;
  j["page1"] = page1;
  j["page2"] = page2;
}

void BookSpecialModel::deserializeBody(const json &j) {
  openAngle = j.at("open_angle").get<double>();
  page1 = j.at("page1").get<double>();
  page2 = j.at("page2").get<double>();
}

void ChestSpecialModel::serializeBody(json &j) const {
  j["texture"] = texture;
  if (chestType != ChestType::Single) {
    j["chest_type"] = toString(chestType);
  }
  if (openness != 0.0) {
    j["openness"] = openness;
  }
}

void ChestSpecialModel::deserializeBody(const json &j) {
  texture = j.at("texture").get<std::string>();
  if (auto s = readOptional<std::string>(j, "chest_type")) {
    chestType = chestTypeFromString(*s);
  } else {
    chestType = ChestType::Single;
  }
  openness = readOptional<double>(j, "openness").value_or(0.0);
}

void ConduitSpecialModel::serializeBody(json &) const {}
void ConduitSpecialModel::deserializeBody(const json &) {}

void CopperGolemStatueSpecialModel::serializeBody(json &j) const {
  j["pose"] = toString(pose);
  j["texture"] = texture;
}

void CopperGolemStatueSpecialModel::deserializeBody(const json &j) {
  pose = copperGolemPoseFromString(j.at("pose").get<std::string>());
  texture = j.at("texture").get<std::string>();
}

void DecoratedPotSpecialModel::serializeBody(json &) const {}
void DecoratedPotSpecialModel::deserializeBody(const json &) {}

void EndCubeSpecialModel::serializeBody(json &j) const {
  j["effect"] = toString(effect);
}

void EndCubeSpecialModel::deserializeBody(const json &j) {
  effect = endCubeEffectFromString(j.at("effect").get<std::string>());
}

void HeadSpecialModel::serializeBody(json &j) const {
  j["kind"] = toString(kind);
  writeOptional(j, "texture", texture);
  if (animation != 0.0) {
    j["animation"] = animation;
  }
}

void HeadSpecialModel::deserializeBody(const json &j) {
  kind = headKindFromString(j.at("kind").get<std::string>());
  texture = readOptional<std::string>(j, "texture");
  animation = readOptional<double>(j, "animation").value_or(0.0);
}

void PlayerHeadSpecialModel::serializeBody(json &) const {}
void PlayerHeadSpecialModel::deserializeBody(const json &) {}
void ShieldSpecialModel::serializeBody(json &) const {}
void ShieldSpecialModel::deserializeBody(const json &) {}

void ShulkerBoxSpecialModel::serializeBody(json &j) const {
  j["texture"] = texture;
  if (openness != 0.0) {
    j["openness"] = openness;
  }
}

void ShulkerBoxSpecialModel::deserializeBody(const json &j) {
  texture = j.at("texture").get<std::string>();
  openness = readOptional<double>(j, "openness").value_or(0.0);
}

void TridentSpecialModel::serializeBody(json &) const {}
void TridentSpecialModel::deserializeBody(const json &) {}

std::unique_ptr<SpecialModel> SpecialModel::create(const json &j) {
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
