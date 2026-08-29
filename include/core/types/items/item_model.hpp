#pragma once

#include "core/types/common.hpp"
#include "core/util/json/utils.hpp"
#include "special_model.hpp"
#include "tint_source.hpp"
#include "types.hpp"

#include <memory>
#include <optional>
#include <string>
#include <vector>

using namespace util;

namespace enchantment_tweaks::item {

enum class ItemModelType : uint8_t {
  Model,
  Composite,
  Condition,
  Select,
  RangeDispatch,
  Empty,
  BundleSelectedItem,
  Special,
};

inline const EnumTable<ItemModelType> &itemModelTypeTable() {
  static const EnumTable<ItemModelType> table = {
      {ItemModelType::Model, "minecraft:model"},
      {ItemModelType::Composite, "minecraft:composite"},
      {ItemModelType::Condition, "minecraft:condition"},
      {ItemModelType::Select, "minecraft:select"},
      {ItemModelType::RangeDispatch, "minecraft:range_dispatch"},
      {ItemModelType::Empty, "minecraft:empty"},
      {ItemModelType::BundleSelectedItem, "minecraft:bundle/selected_item"},
      {ItemModelType::Special, "minecraft:special"},
  };
  return table;
}
inline std::string toString(ItemModelType t) {
  return enumToString(t, itemModelTypeTable());
}
inline ItemModelType itemModelTypeFromString(const std::string &s) {
  return enumFromString(s, itemModelTypeTable());
}

enum class BooleanPropertyType : uint8_t {
  Broken,
  BundleHasSelectedItem,
  Carried,
  Component,
  Damaged,
  ExtendedView,
  FishingRodCast,
  HasComponent,
  KeybindDown,
  Selected,
  UsingItem,
  ViewEntity,
  CustomModelData,
};

inline const EnumTable<BooleanPropertyType> &booleanPropertyTypeTable() {
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
      {BooleanPropertyType::Selected, "minecraft:selected"},
      {BooleanPropertyType::UsingItem, "minecraft:using_item"},
      {BooleanPropertyType::ViewEntity, "minecraft:view_entity"},
      {BooleanPropertyType::CustomModelData, "minecraft:custom_model_data"},
  };
  return table;
}
inline std::string toString(BooleanPropertyType t) {
  return enumToString(t, booleanPropertyTypeTable());
}
inline BooleanPropertyType booleanPropertyTypeFromString(const std::string &s) {
  return enumFromString(s, booleanPropertyTypeTable());
}

enum class SelectPropertyType : uint8_t {
  BlockState,
  ChargeType,
  Component,
  ContextDimension,
  ContextEntityType,
  DisplayContext,
  LocalTime,
  MainHand,
  TrimMaterial,
  CustomModelData,
};

inline const EnumTable<SelectPropertyType> &selectPropertyTypeTable() {
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
inline std::string toString(SelectPropertyType t) {
  return enumToString(t, selectPropertyTypeTable());
}
inline SelectPropertyType selectPropertyTypeFromString(const std::string &s) {
  return enumFromString(s, selectPropertyTypeTable());
}

enum class NumericPropertyType : uint8_t {
  BundleFullness,
  Compass,
  Cooldown,
  Count,
  CrossbowPull,
  Damage,
  Time,
  UseCycle,
  UseDuration,
  CustomModelData,
};

inline const EnumTable<NumericPropertyType> &numericPropertyTypeTable() {
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
inline std::string toString(NumericPropertyType t) {
  return enumToString(t, numericPropertyTypeTable());
}
inline NumericPropertyType numericPropertyTypeFromString(const std::string &s) {
  return enumFromString(s, numericPropertyTypeTable());
}

// range_dispatch / time : "source" field
enum class TimeSource : uint8_t { Daytime, MoonPhase, Random };
inline const EnumTable<TimeSource> &timeSourceTable() {
  static const EnumTable<TimeSource> table = {
      {TimeSource::Daytime, "daytime"},
      {TimeSource::MoonPhase, "moon_phase"},
      {TimeSource::Random, "random"},
  };
  return table;
}
inline std::string toString(TimeSource t) {
  return enumToString(t, timeSourceTable());
}
inline TimeSource timeSourceFromString(const std::string &s) {
  return enumFromString(s, timeSourceTable());
}

// range_dispatch / compass : "target" field
enum class CompassTarget : uint8_t { Spawn, Lodestone, Recovery, None };
inline const EnumTable<CompassTarget> &compassTargetTable() {
  static const EnumTable<CompassTarget> table = {
      {CompassTarget::Spawn, "spawn"},
      {CompassTarget::Lodestone, "lodestone"},
      {CompassTarget::Recovery, "recovery"},
      {CompassTarget::None, "none"},
  };
  return table;
}
inline std::string toString(CompassTarget t) {
  return enumToString(t, compassTargetTable());
}
inline CompassTarget compassTargetFromString(const std::string &s) {
  return enumFromString(s, compassTargetTable());
}

// Documented, but represented as plain strings rather than enums because
// they only ever appear as `select` *case values* (i.e. free-form matched
// strings), never as a struct field of their own:
//   charge_type values:      none | rocket | arrow
//   display_context values:  none | thirdperson_lefthand |
//   thirdperson_righthand |
//                            firstperson_lefthand | firstperson_righthand |
//                            head | gui | ground | fixed | on_shelf
//   main_hand values:        left | right

// ===========================================================================
// ItemModel hierarchy (the recursive "Items model" object)
// ===========================================================================

class ItemModel : public JsonSerializable {
public:
  explicit ItemModel(ItemModelType type) : type_(type) {}
  ~ItemModel() override = default;

  ItemModelType type() const { return type_; }

  std::optional<Transformation> transformation;

  json serialize() const override {
    json j = json::object();
    j["type"] = toString(type_);
    if (transformation.has_value()) {
      j["transformation"] = transformation->serialize();
    }
    serializeBody(j);
    return j;
  }

  void deserialize(const json &j) override {
    if (j.contains("transformation")) {
      Transformation t;
      t.deserialize(j.at("transformation"));
      transformation = t;
    } else {
      transformation.reset();
    }
    deserializeBody(j);
  }

  static std::unique_ptr<ItemModel> create(const json &j);

protected:
  virtual void serializeBody(json &j) const = 0;
  virtual void deserializeBody(const json &j) = 0;

  static std::unique_ptr<ItemModel> child(const json &j) {
    return ItemModel::create(j);
  }

private:
  ItemModelType type_;
};

// --- model ---------------------------------------------------------------

class ModelItemModel final : public ItemModel {
public:
  ModelItemModel() : ItemModel(ItemModelType::Model) {}

  std::string model; // namespaced id of the model file
  std::vector<std::unique_ptr<TintSource>> tints;

protected:
  void serializeBody(json &j) const override {
    j["model"] = model;
    if (!tints.empty()) {
      json arr = json::array();
      for (const auto &tint : tints) {
        arr.push_back(tint->serialize());
      }
      j["tints"] = std::move(arr);
    }
  }
  void deserializeBody(const json &j) override {
    model = j.at("model").get<std::string>();
    tints.clear();
    if (const auto it = j.find("tints"); it != j.end()) {
      for (const auto &tj : *it) {
        tints.push_back(TintSource::create(tj));
      }
    }
  }
};

// --- composite -------------------------------------------------------------

class CompositeItemModel final : public ItemModel {
public:
  CompositeItemModel() : ItemModel(ItemModelType::Composite) {}

  std::vector<std::unique_ptr<ItemModel>> models;

protected:
  void serializeBody(json &j) const override {
    json arr = json::array();
    for (const auto &m : models) {
      arr.push_back(m->serialize());
    }
    j["models"] = std::move(arr);
  }
  void deserializeBody(const json &j) override {
    models.clear();
    for (const auto &mj : j.at("models")) {
      models.push_back(child(mj));
    }
  }
};

// --- condition ---------------------------------------------------------

// Holds the (small) union of "additional fields" used by the various
// boolean property types. Only the members relevant to `property` are
// populated / serialized; see the wiki table under "Boolean property types".
class ConditionItemModel final : public ItemModel {
public:
  ConditionItemModel() : ItemModel(ItemModelType::Condition) {}

  BooleanPropertyType property = BooleanPropertyType::Selected;

  // minecraft:component
  std::optional<std::string> predicate;
  std::optional<json> value;
  // minecraft:has_component
  std::optional<std::string> component;
  std::optional<bool> ignoreDefault; // default false
  // minecraft:keybind_down
  std::optional<std::string> keybind;
  // minecraft:custom_model_data
  std::optional<int> index; // default 0

  std::unique_ptr<ItemModel> onTrue;
  std::unique_ptr<ItemModel> onFalse;

protected:
  void serializeBody(json &j) const override {
    j["property"] = toString(property);
    switch (property) {
    case BooleanPropertyType::Component:
      writeOptional(j, "predicate", predicate);
      if (value.has_value()) {
        j["value"] = *value;
      }
      break;
    case BooleanPropertyType::HasComponent:
      writeOptional(j, "component", component);
      if (ignoreDefault.value_or(false)) {
        j["ignore_default"] = *ignoreDefault;
      }
      break;
    case BooleanPropertyType::KeybindDown:
      writeOptional(j, "keybind", keybind);
      break;
    case BooleanPropertyType::CustomModelData:
      if (index.value_or(0) != 0) {
        j["index"] = *index;
      }
      break;
    default:
      break; // no additional fields
    }
    if (onTrue) {
      j["on_true"] = onTrue->serialize();
    }
    if (onFalse) {
      j["on_false"] = onFalse->serialize();
    }
  }

  void deserializeBody(const json &j) override {
    property =
        booleanPropertyTypeFromString(j.at("property").get<std::string>());
    predicate.reset();
    value.reset();
    component.reset();
    ignoreDefault.reset();
    keybind.reset();
    index.reset();

    switch (property) {
    case BooleanPropertyType::Component:
      predicate = readOptional<std::string>(j, "predicate");
      if (j.contains("value")) {
        value = j.at("value");
      }
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
};

// --- select --------------------------------------------------------------

struct SelectCase final : public JsonSerializable {
  std::vector<std::string> when; // one entry serializes as a bare string
  std::unique_ptr<ItemModel> model;

  json serialize() const override {
    json j = json::object();
    j["when"] = (when.size() == 1) ? json(when.front()) : json(when);
    if (model) {
      j["model"] = model->serialize();
    }
    return j;
  }
  void deserialize(const json &j) override {
    when.clear();
    const auto &w = j.at("when");
    if (w.is_array()) {
      for (const auto &e : w) {
        when.push_back(e.get<std::string>());
      }
    } else {
      when.push_back(w.get<std::string>());
    }
    model = j.contains("model") ? ItemModel::create(j.at("model")) : nullptr;
  }
};

class SelectItemModel final : public ItemModel {
public:
  SelectItemModel() : ItemModel(ItemModelType::Select) {}

  SelectPropertyType property = SelectPropertyType::MainHand;

  // minecraft:block_state
  std::optional<std::string> blockStateProperty;
  // minecraft:component
  std::optional<std::string> component;
  // minecraft:local_time
  std::optional<std::string> locale;   // default ""
  std::optional<std::string> timeZone; // optional
  std::optional<std::string> pattern;  // required for local_time
  // minecraft:custom_model_data
  std::optional<int> index; // default 0

  std::vector<SelectCase> cases;
  std::unique_ptr<ItemModel> fallback; // optional

protected:
  void serializeBody(json &j) const override {
    j["property"] = toString(property);
    switch (property) {
    case SelectPropertyType::BlockState:
      writeOptional(j, "block_state_property", blockStateProperty);
      break;
    case SelectPropertyType::Component:
      writeOptional(j, "component", component);
      break;
    case SelectPropertyType::LocalTime:
      if (locale.has_value() && !locale->empty()) {
        j["locale"] = *locale;
      }
      writeOptional(j, "time_zone", timeZone);
      writeOptional(j, "pattern", pattern);
      break;
    case SelectPropertyType::CustomModelData:
      if (index.value_or(0) != 0) {
        j["index"] = *index;
      }
      break;
    default:
      break;
    }
    json arr = json::array();
    for (const auto &c : cases) {
      arr.push_back(c.serialize());
    }
    j["cases"] = std::move(arr);
    if (fallback) {
      j["fallback"] = fallback->serialize();
    }
  }

  void deserializeBody(const json &j) override {
    property =
        selectPropertyTypeFromString(j.at("property").get<std::string>());
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
};

// range_dispatch

struct RangeDispatchEntry final : public JsonSerializable {
  double threshold = 0.0;
  std::unique_ptr<ItemModel> model;

  json serialize() const override {
    json j = json::object();
    j["threshold"] = threshold;
    if (model) {
      j["model"] = model->serialize();
    }
    return j;
  }
  void deserialize(const json &j) override {
    threshold = j.at("threshold").get<double>();
    model = j.contains("model") ? ItemModel::create(j.at("model")) : nullptr;
  }
};

class RangeDispatchItemModel final : public ItemModel {
public:
  RangeDispatchItemModel() : ItemModel(ItemModelType::RangeDispatch) {}

  NumericPropertyType property = NumericPropertyType::Damage;
  double scale = 1.0; // optional, default 1.0

  // minecraft:count / minecraft:damage
  std::optional<bool> normalize; // default true
  // minecraft:compass
  std::optional<CompassTarget> target;
  std::optional<bool> wobble; // default true (compass & time)
  // minecraft:time
  std::optional<TimeSource> source;
  // minecraft:use_cycle
  std::optional<double> period; // default 1.0
  // minecraft:use_duration
  std::optional<bool> remaining; // default false
  // minecraft:custom_model_data
  std::optional<int> index; // default 0

  std::vector<RangeDispatchEntry> entries;
  std::unique_ptr<ItemModel> fallback; // optional

protected:
  void serializeBody(json &j) const override {
    j["property"] = toString(property);
    if (scale != 1.0) {
      j["scale"] = scale;
    }

    switch (property) {
    case NumericPropertyType::Count:
    case NumericPropertyType::Damage:
      if (!normalize.value_or(true)) {
        j["normalize"] = *normalize;
      }
      break;
    case NumericPropertyType::Compass:
      if (target.has_value()) {
        j["target"] = toString(*target);
      }
      if (!wobble.value_or(true)) {
        j["wobble"] = *wobble;
      }
      break;
    case NumericPropertyType::Time:
      if (source.has_value()) {
        j["source"] = toString(*source);
      }
      if (!wobble.value_or(true)) {
        j["wobble"] = *wobble;
      }
      break;
    case NumericPropertyType::UseCycle:
      if (period.value_or(1.0) != 1.0) {
        j["period"] = *period;
      }
      break;
    case NumericPropertyType::UseDuration:
      if (remaining.value_or(false)) {
        j["remaining"] = *remaining;
      }
      break;
    case NumericPropertyType::CustomModelData:
      if (index.value_or(0) != 0) {
        j["index"] = *index;
      }
      break;
    default:
      break;
    }

    json arr = json::array();
    for (const auto &e : entries) {
      arr.push_back(e.serialize());
    }
    j["entries"] = std::move(arr);
    if (fallback) {
      j["fallback"] = fallback->serialize();
    }
  }

  void deserializeBody(const json &j) override {
    property =
        numericPropertyTypeFromString(j.at("property").get<std::string>());
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
      if (auto s = readOptional<std::string>(j, "target")) {
        target = compassTargetFromString(*s);
      }
      wobble = readOptional<bool>(j, "wobble").value_or(true);
      break;
    case NumericPropertyType::Time:
      if (auto s = readOptional<std::string>(j, "source")) {
        source = timeSourceFromString(*s);
      }
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
};

// --- empty / bundle/selected_item ------------------------------------------

class EmptyItemModel final : public ItemModel {
public:
  EmptyItemModel() : ItemModel(ItemModelType::Empty) {}

protected:
  void serializeBody(json & /*j*/) const override {}
  void deserializeBody(const json & /*j*/) override {}
};

class BundleSelectedItemItemModel final : public ItemModel {
public:
  BundleSelectedItemItemModel()
      : ItemModel(ItemModelType::BundleSelectedItem) {}

protected:
  void serializeBody(json & /*j*/) const override {}
  void deserializeBody(const json & /*j*/) override {}
};

// --- special ---------------------------------------------------------------

class SpecialItemModel final : public ItemModel {
public:
  SpecialItemModel() : ItemModel(ItemModelType::Special) {}

  std::unique_ptr<SpecialModel> model;
  std::string base; // namespaced id of a plain model providing
                    // transforms/particle/GUI light

protected:
  void serializeBody(json &j) const override {
    if (model) {
      j["model"] = model->serialize();
    }
    j["base"] = base;
  }
  void deserializeBody(const json &j) override {
    model = j.contains("model") ? SpecialModel::create(j.at("model")) : nullptr;
    base = j.at("base").get<std::string>();
  }
};

inline std::unique_ptr<ItemModel> ItemModel::create(const json &j) {
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
  }
  result->deserialize(j);
  return result;
}

} // namespace enchantment_tweaks::item
