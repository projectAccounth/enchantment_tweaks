#include "core/types/items/item_model.hpp"
#include "core/types/items/special_model.hpp"
#include "core/types/items/tint_source.hpp"
#include "core/types/items/types.hpp"
#include "core/util/json/utils.hpp"
#include <memory>
#include <string>
#include <utility>

namespace enchantment_tweaks::item {

const EnumTable<ItemModelType> &itemModelTypeTable() {
  static const EnumTable<ItemModelType> table = {
      {ItemModelType::Model, "minecraft:model"},
      {ItemModelType::Composite, "minecraft:composite"},
      {ItemModelType::Condition, "minecraft:condition"},
      {ItemModelType::Select, "minecraft:select"},
      {ItemModelType::RangeDispatch, "minecraft:range_dispatch"},
      {ItemModelType::Empty, "minecraft:empty"},
      {ItemModelType::BundleSelectedItem, "minecraft:bundle/selected_item"},
      {ItemModelType::Special, "minecraft:special"},
      {ItemModelType::Bed, "minecraft:bed"},
  };
  return table;
}
std::string toString(ItemModelType t) {
  return enumToString(t, itemModelTypeTable());
}
ItemModelType itemModelTypeFromString(const std::string &s) {
  return enumFromString(s, itemModelTypeTable());
}

const EnumTable<BooleanPropertyType> &booleanPropertyTypeTable() {
  static const EnumTable<BooleanPropertyType> table = {
      {BooleanPropertyType::Broken, "minecraft:broken"},
      {BooleanPropertyType::BundleHasSelectedItem,
       "minecraft:bundle/has_selected_item"},
      {BooleanPropertyType::Carried, "minecraft:carried"},
      {BooleanPropertyType::Component, "minecraft:component"},
      {BooleanPropertyType::Damaged, "minecraft:damaged"},
      {BooleanPropertyType::ExtendedView, "minecraft:extended_view"},
      {BooleanPropertyType::FishingRodCast, "minecraft:fishing_rod/cast"},
      {BooleanPropertyType::HasComponent, "minecraft:has_component"},
      {BooleanPropertyType::KeybindDown, "minecraft:keybind_down"},
      {BooleanPropertyType::OnShelf, "minecraft:on_shelf"},
      {BooleanPropertyType::OnShelf, "on_shelf"},
      {BooleanPropertyType::Selected, "minecraft:selected"},
      {BooleanPropertyType::UsingItem, "minecraft:using_item"},
      {BooleanPropertyType::ViewEntity, "minecraft:view_entity"},
      {BooleanPropertyType::CustomModelData, "minecraft:custom_model_data"},
  };
  return table;
}
std::string toString(BooleanPropertyType t) {
  return enumToString(t, booleanPropertyTypeTable());
}
BooleanPropertyType booleanPropertyTypeFromString(const std::string &s) {
  return enumFromString(s, booleanPropertyTypeTable());
}

const EnumTable<SelectPropertyType> &selectPropertyTypeTable() {
  static const EnumTable<SelectPropertyType> table = {
      {SelectPropertyType::BlockState, "minecraft:block_state"},
      {SelectPropertyType::ChargeType, "minecraft:charge_type"},
      {SelectPropertyType::Component, "minecraft:component"},
      {SelectPropertyType::ContextDimension, "minecraft:context_dimension"},
      {SelectPropertyType::ContextEntityType, "minecraft:context_entity_type"},
      {SelectPropertyType::DisplayContext, "minecraft:display_context"},
      {SelectPropertyType::LocalTime, "minecraft:local_time"},
      {SelectPropertyType::MainHand, "minecraft:main_hand"},
      {SelectPropertyType::TrimMaterial, "minecraft:trim_material"},
      {SelectPropertyType::CustomModelData, "minecraft:custom_model_data"},
  };
  return table;
}
std::string toString(SelectPropertyType t) {
  return enumToString(t, selectPropertyTypeTable());
}
SelectPropertyType selectPropertyTypeFromString(const std::string &s) {
  return enumFromString(s, selectPropertyTypeTable());
}

const EnumTable<NumericPropertyType> &numericPropertyTypeTable() {
  static const EnumTable<NumericPropertyType> table = {
      {NumericPropertyType::BundleFullness, "minecraft:bundle/fullness"},
      {NumericPropertyType::Compass, "minecraft:compass"},
      {NumericPropertyType::Cooldown, "minecraft:cooldown"},
      {NumericPropertyType::Count, "minecraft:count"},
      {NumericPropertyType::CrossbowPull, "minecraft:crossbow/pull"},
      {NumericPropertyType::Damage, "minecraft:damage"},
      {NumericPropertyType::Time, "minecraft:time"},
      {NumericPropertyType::UseCycle, "minecraft:use_cycle"},
      {NumericPropertyType::UseDuration, "minecraft:use_duration"},
      {NumericPropertyType::CustomModelData, "minecraft:custom_model_data"},
  };
  return table;
}
std::string toString(NumericPropertyType t) {
  return enumToString(t, numericPropertyTypeTable());
}
NumericPropertyType numericPropertyTypeFromString(const std::string &s) {
  return enumFromString(s, numericPropertyTypeTable());
}

const EnumTable<TimeSource> &timeSourceTable() {
  static const EnumTable<TimeSource> table = {
      {TimeSource::Daytime, "daytime"},
      {TimeSource::MoonPhase, "moon_phase"},
      {TimeSource::Random, "random"},
  };
  return table;
}
std::string toString(TimeSource t) {
  return enumToString(t, timeSourceTable());
}
TimeSource timeSourceFromString(const std::string &s) {
  return enumFromString(s, timeSourceTable());
}

const EnumTable<CompassTarget> &compassTargetTable() {
  static const EnumTable<CompassTarget> table = {
      {CompassTarget::Spawn, "spawn"},
      {CompassTarget::Lodestone, "lodestone"},
      {CompassTarget::Recovery, "recovery"},
      {CompassTarget::None, "none"},
  };
  return table;
}
std::string toString(CompassTarget t) {
  return enumToString(t, compassTargetTable());
}
CompassTarget compassTargetFromString(const std::string &s) {
  return enumFromString(s, compassTargetTable());
}

json ItemModel::serialize() const {
  json j = json::object();
  j["type"] = toString(type_);
  if (transformation.has_value()) {
    j["transformation"] = transformation->serialize();
  }
  serializeBody(j);
  return j;
}

void ItemModel::deserialize(const json &j) {
  if (j.contains("transformation")) {
    Transformation t;
    t.deserialize(j.at("transformation"));
    transformation = t;
  } else {
    transformation.reset();
  }
  deserializeBody(j);
}

std::unique_ptr<ItemModel> ItemModel::child(const json &j) {
  return ItemModel::create(j);
}

void ModelItemModel::serializeBody(json &j) const {
  j["model"] = model;
  if (!tints.empty()) {
    json arr = json::array();
    for (const auto &tint : tints) {
      arr.push_back(tint->serialize());
    }
    j["tints"] = std::move(arr);
  }
}
void ModelItemModel::deserializeBody(const json &j) {
  model = j.at("model").get<std::string>();
  tints.clear();
  if (const auto it = j.find("tints"); it != j.end()) {
    for (const auto &tj : *it) {
      tints.push_back(TintSource::create(tj));
    }
  }
}

void CompositeItemModel::serializeBody(json &j) const {
  json arr = json::array();
  for (const auto &m : models) {
    arr.push_back(m->serialize());
  }
  j["models"] = std::move(arr);
}
void CompositeItemModel::deserializeBody(const json &j) {
  models.clear();
  for (const auto &mj : j.at("models")) {
    models.push_back(child(mj));
  }
}

void ConditionItemModel::serializeBody(json &j) const {
  j["property"] = toString(property);
  switch (property) {
  case BooleanPropertyType::Component:
    writeOptional(j, "predicate", predicate);
    if (value.has_value()) j["value"] = *value;
    break;
  case BooleanPropertyType::HasComponent:
    writeOptional(j, "component", component);
    if (ignoreDefault && *ignoreDefault) j["ignore_default"] = true;
    break;
  case BooleanPropertyType::KeybindDown:
    writeOptional(j, "keybind", keybind);
    break;
  case BooleanPropertyType::CustomModelData:
    if (index && *index != 0) j["index"] = *index;
    break;
  default:
    break;
  }
  if (onTrue) j["on_true"] = onTrue->serialize();
  if (onFalse) j["on_false"] = onFalse->serialize();
}
void ConditionItemModel::deserializeBody(const json &j) {
  property = booleanPropertyTypeFromString(j.at("property").get<std::string>());
  predicate.reset();
  value.reset();
  component.reset();
  ignoreDefault.reset();
  keybind.reset();
  index.reset();
  switch (property) {
  case BooleanPropertyType::Component:
    predicate = readOptional<std::string>(j, "predicate");
    if (j.contains("value")) value = j.at("value");
    break;
  case BooleanPropertyType::HasComponent:
    component = readOptional<std::string>(j, "component");
    ignoreDefault = readOptional<bool>(j, "ignore_default").value_or(false);
    break;
  case BooleanPropertyType::KeybindDown:
    keybind = readOptional<std::string>(j, "keybind");
    break;
  case BooleanPropertyType::CustomModelData:
    index = readOptional<int>(j, "index").value_or(0);
    break;
  default:
    break;
  }
  onTrue = j.contains("on_true") ? child(j.at("on_true")) : nullptr;
  onFalse = j.contains("on_false") ? child(j.at("on_false")) : nullptr;
}

json SelectCase::serialize() const {
  json j = json::object();
  j["when"] = (when.size() == 1) ? json(when.front()) : json(when);
  if (model) j["model"] = model->serialize();
  return j;
}
void SelectCase::deserialize(const json &j) {
  when.clear();
  const auto &w = j.at("when");
  if (w.is_array()) {
    for (const auto &e : w)
      when.push_back(e.get<std::string>());
  } else {
    when.push_back(w.get<std::string>());
  }
  model = j.contains("model") ? ItemModel::create(j.at("model")) : nullptr;
}

void SelectItemModel::serializeBody(json &j) const {
  j["property"] = toString(property);
  switch (property) {
  case SelectPropertyType::BlockState:
    writeOptional(j, "block_state_property", blockStateProperty);
    break;
  case SelectPropertyType::Component:
    writeOptional(j, "component", component);
    break;
  case SelectPropertyType::LocalTime:
    if (locale.has_value() && !locale->empty()) j["locale"] = *locale;
    writeOptional(j, "time_zone", timeZone);
    writeOptional(j, "pattern", pattern);
    break;
  case SelectPropertyType::CustomModelData:
    if (index && *index != 0) j["index"] = *index;
    break;
  default:
    break;
  }
  json arr = json::array();
  for (const auto &c : cases)
    arr.push_back(c.serialize());
  j["cases"] = std::move(arr);
  if (fallback) j["fallback"] = fallback->serialize();
}
void SelectItemModel::deserializeBody(const json &j) {
  property = selectPropertyTypeFromString(j.at("property").get<std::string>());
  blockStateProperty.reset();
  component.reset();
  locale.reset();
  timeZone.reset();
  pattern.reset();
  index.reset();
  switch (property) {
  case SelectPropertyType::BlockState:
    blockStateProperty = readOptional<std::string>(j, "block_state_property");
    break;
  case SelectPropertyType::Component:
    component = readOptional<std::string>(j, "component");
    break;
  case SelectPropertyType::LocalTime:
    locale = readOptional<std::string>(j, "locale").value_or("");
    timeZone = readOptional<std::string>(j, "time_zone");
    pattern = readOptional<std::string>(j, "pattern");
    break;
  case SelectPropertyType::CustomModelData:
    index = readOptional<int>(j, "index").value_or(0);
    break;
  default:
    break;
  }
  cases.clear();
  for (const auto &cj : j.at("cases")) {
    SelectCase c;
    c.deserialize(cj);
    cases.push_back(std::move(c));
  }
  fallback = j.contains("fallback") ? child(j.at("fallback")) : nullptr;
}

json RangeDispatchEntry::serialize() const {
  json j = json::object();
  j["threshold"] = threshold;
  if (model) j["model"] = model->serialize();
  return j;
}
void RangeDispatchEntry::deserialize(const json &j) {
  threshold = j.at("threshold").get<double>();
  model = j.contains("model") ? ItemModel::create(j.at("model")) : nullptr;
}

void RangeDispatchItemModel::serializeBody(json &j) const {
  j["property"] = toString(property);
  if (scale != 1.0) j["scale"] = scale;
  switch (property) {
  case NumericPropertyType::Count:
  case NumericPropertyType::Damage:
    if (normalize && !*normalize) j["normalize"] = false;
    break;
  case NumericPropertyType::Compass:
    if (target.has_value()) j["target"] = toString(*target);
    if (wobble && !*wobble) j["wobble"] = false;
    break;
  case NumericPropertyType::Time:
    if (source.has_value()) j["source"] = toString(*source);
    if (wobble && !*wobble) j["wobble"] = false;
    break;
  case NumericPropertyType::UseCycle:
    if (period && *period != 1.0) j["period"] = *period;
    break;
  case NumericPropertyType::UseDuration:
    if (remaining && *remaining) j["remaining"] = true;
    break;
  case NumericPropertyType::CustomModelData:
    if (index && *index != 0) j["index"] = *index;
    break;
  default:
    break;
  }
  json arr = json::array();
  for (const auto &e : entries)
    arr.push_back(e.serialize());
  j["entries"] = std::move(arr);
  if (fallback) j["fallback"] = fallback->serialize();
}
void RangeDispatchItemModel::deserializeBody(const json &j) {
  property = numericPropertyTypeFromString(j.at("property").get<std::string>());
  scale = readOptional<double>(j, "scale").value_or(1.0);
  normalize.reset();
  target.reset();
  wobble.reset();
  source.reset();
  period.reset();
  remaining.reset();
  index.reset();
  switch (property) {
  case NumericPropertyType::Count:
  case NumericPropertyType::Damage:
    normalize = readOptional<bool>(j, "normalize").value_or(true);
    break;
  case NumericPropertyType::Compass:
    if (auto s = readOptional<std::string>(j, "target"))
      target = compassTargetFromString(*s);
    wobble = readOptional<bool>(j, "wobble").value_or(true);
    break;
  case NumericPropertyType::Time:
    if (auto s = readOptional<std::string>(j, "source"))
      source = timeSourceFromString(*s);
    wobble = readOptional<bool>(j, "wobble").value_or(true);
    break;
  case NumericPropertyType::UseCycle:
    period = readOptional<double>(j, "period").value_or(1.0);
    break;
  case NumericPropertyType::UseDuration:
    remaining = readOptional<bool>(j, "remaining").value_or(false);
    break;
  case NumericPropertyType::CustomModelData:
    index = readOptional<int>(j, "index").value_or(0);
    break;
  default:
    break;
  }
  entries.clear();
  for (const auto &ej : j.at("entries")) {
    RangeDispatchEntry e;
    e.deserialize(ej);
    entries.push_back(std::move(e));
  }
  fallback = j.contains("fallback") ? child(j.at("fallback")) : nullptr;
}

void EmptyItemModel::serializeBody(json &) const {}
void EmptyItemModel::deserializeBody(const json &) {}
void BundleSelectedItemItemModel::serializeBody(json &) const {}
void BundleSelectedItemItemModel::deserializeBody(const json &) {}

void SpecialItemModel::serializeBody(json &j) const {
  if (model) j["model"] = model->serialize();
  j["base"] = base;
}
void SpecialItemModel::deserializeBody(const json &j) {
  if (j.contains("model")) {
    try {
      model = SpecialModel::create(j.at("model"));
    } catch (...) {
      // Not a known special model type (e.g. minecraft:bed inside special)
      model.reset();
    }
  }
  base = j.at("base").get<std::string>();
}

std::unique_ptr<ItemModel> ItemModel::create(const json &j) {
  const ItemModelType type =
      itemModelTypeFromString(j.at("type").get<std::string>());
  std::unique_ptr<ItemModel> result;
  switch (type) {
  case ItemModelType::Model:
    result = std::make_unique<ModelItemModel>();
    break;
  case ItemModelType::Composite:
    result = std::make_unique<CompositeItemModel>();
    break;
  case ItemModelType::Condition:
    result = std::make_unique<ConditionItemModel>();
    break;
  case ItemModelType::Select:
    result = std::make_unique<SelectItemModel>();
    break;
  case ItemModelType::RangeDispatch:
    result = std::make_unique<RangeDispatchItemModel>();
    break;
  case ItemModelType::Empty:
    result = std::make_unique<EmptyItemModel>();
    break;
  case ItemModelType::BundleSelectedItem:
    result = std::make_unique<BundleSelectedItemItemModel>();
    break;
  case ItemModelType::Special:
    result = std::make_unique<SpecialItemModel>();
    break;
  case ItemModelType::Bed:
    result = std::make_unique<ModelItemModel>();
    break;
  }
  result->deserialize(j);
  return result;
}

} // namespace enchantment_tweaks::item
