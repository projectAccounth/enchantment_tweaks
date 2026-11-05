#pragma once

#include "core/types/common.hpp"
#include "core/util/json/utils.hpp"
#include "special_model.hpp"
#include "tint_source.hpp"
#include "types.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace enchantment_tweaks::item {

using namespace util;

enum class ItemModelType : uint8_t {
  Model,
  Composite,
  Condition,
  Select,
  RangeDispatch,
  Empty,
  BundleSelectedItem,
  Special,
  Bed,
};

const EnumTable<ItemModelType> &itemModelTypeTable();
std::string toString(ItemModelType t);
ItemModelType itemModelTypeFromString(const std::string &s);

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
  OnShelf,
  Selected,
  UsingItem,
  ViewEntity,
  CustomModelData,
};

const EnumTable<BooleanPropertyType> &booleanPropertyTypeTable();
std::string toString(BooleanPropertyType t);
BooleanPropertyType booleanPropertyTypeFromString(const std::string &s);

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

const EnumTable<SelectPropertyType> &selectPropertyTypeTable();
std::string toString(SelectPropertyType t);
SelectPropertyType selectPropertyTypeFromString(const std::string &s);

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

const EnumTable<NumericPropertyType> &numericPropertyTypeTable();
std::string toString(NumericPropertyType t);
NumericPropertyType numericPropertyTypeFromString(const std::string &s);

/// Sources for the time range-dispatch property.
enum class TimeSource : uint8_t { Daytime, MoonPhase, Random };
const EnumTable<TimeSource> &timeSourceTable();
std::string toString(TimeSource t);
TimeSource timeSourceFromString(const std::string &s);

/// Targets for the compass range-dispatch property.
enum class CompassTarget : uint8_t { Spawn, Lodestone, Recovery, None };
const EnumTable<CompassTarget> &compassTargetTable();
std::string toString(CompassTarget t);
CompassTarget compassTargetFromString(const std::string &s);

// Select properties remain strings because they only appear as case values.
class ItemModel : public JsonSerializable {
public:
  explicit ItemModel(ItemModelType type) : type_(type) {}
  ~ItemModel() override = default;

  ItemModelType type() const { return type_; }

  std::optional<Transformation> transformation;

  json serialize() const override;
  void deserialize(const json &j) override;

  static std::unique_ptr<ItemModel> create(const json &j);

protected:
  virtual void serializeBody(json &j) const = 0;
  virtual void deserializeBody(const json &j) = 0;

  static std::unique_ptr<ItemModel> child(const json &j);

private:
  ItemModelType type_;
};

class ModelItemModel final : public ItemModel {
public:
  ModelItemModel() : ItemModel(ItemModelType::Model) {}

  std::string model; // namespaced id of the model file
  std::vector<std::unique_ptr<TintSource>> tints;

protected:
  void serializeBody(json &j) const override;
  void deserializeBody(const json &j) override;
};

class CompositeItemModel final : public ItemModel {
public:
  CompositeItemModel() : ItemModel(ItemModelType::Composite) {}

  std::vector<std::unique_ptr<ItemModel>> models;

protected:
  void serializeBody(json &j) const override;
  void deserializeBody(const json &j) override;
};

/// Additional fields used by boolean properties; only fields relevant to
/// `property` are populated and serialized.
class ConditionItemModel final : public ItemModel {
public:
  ConditionItemModel() : ItemModel(ItemModelType::Condition) {}

  BooleanPropertyType property = BooleanPropertyType::Selected;

  /// minecraft:component
  std::optional<std::string> predicate;
  std::optional<json> value;
  /// minecraft:has_component
  std::optional<std::string> component;
  std::optional<bool> ignoreDefault; // default false
  /// minecraft:keybind_down
  std::optional<std::string> keybind;
  /// minecraft:custom_model_data
  std::optional<int> index; // default 0

  std::unique_ptr<ItemModel> onTrue;
  std::unique_ptr<ItemModel> onFalse;

protected:
  void serializeBody(json &j) const override;
  void deserializeBody(const json &j) override;
};

struct SelectCase final : public JsonSerializable {
  std::vector<std::string> when; // one entry serializes as a bare string
  std::unique_ptr<ItemModel> model;

  json serialize() const override;
  void deserialize(const json &j) override;
};

class SelectItemModel final : public ItemModel {
public:
  SelectItemModel() : ItemModel(ItemModelType::Select) {}

  SelectPropertyType property = SelectPropertyType::MainHand;

  /// minecraft:block_state
  std::optional<std::string> blockStateProperty;
  /// minecraft:component
  std::optional<std::string> component;
  /// minecraft:local_time
  std::optional<std::string> locale;   // default ""
  std::optional<std::string> timeZone; // optional
  std::optional<std::string> pattern;  // required for local_time
  /// minecraft:custom_model_data
  std::optional<int> index; // default 0

  std::vector<SelectCase> cases;
  std::unique_ptr<ItemModel> fallback; // optional

protected:
  void serializeBody(json &j) const override;
  void deserializeBody(const json &j) override;
};

struct RangeDispatchEntry final : public JsonSerializable {
  double threshold = 0.0;
  std::unique_ptr<ItemModel> model;

  json serialize() const override;
  void deserialize(const json &j) override;
};

class RangeDispatchItemModel final : public ItemModel {
public:
  RangeDispatchItemModel() : ItemModel(ItemModelType::RangeDispatch) {}

  NumericPropertyType property = NumericPropertyType::Damage;
  double scale = 1.0; // optional, default 1.0

  /// minecraft:count / minecraft:damage
  std::optional<bool> normalize; // default true
  /// minecraft:compass
  std::optional<CompassTarget> target;
  std::optional<bool> wobble; // default true (compass & time)
  /// minecraft:time
  std::optional<TimeSource> source;
  /// minecraft:use_cycle
  std::optional<double> period; // default 1.0
  /// minecraft:use_duration
  std::optional<bool> remaining; // default false
  /// minecraft:custom_model_data
  std::optional<int> index; // default 0

  std::vector<RangeDispatchEntry> entries;
  std::unique_ptr<ItemModel> fallback; // optional

protected:
  void serializeBody(json &j) const override;
  void deserializeBody(const json &j) override;
};

class EmptyItemModel final : public ItemModel {
public:
  EmptyItemModel() : ItemModel(ItemModelType::Empty) {}

protected:
  void serializeBody(json &j) const override;
  void deserializeBody(const json &j) override;
};

class BundleSelectedItemItemModel final : public ItemModel {
public:
  BundleSelectedItemItemModel()
      : ItemModel(ItemModelType::BundleSelectedItem) {}

protected:
  void serializeBody(json &j) const override;
  void deserializeBody(const json &j) override;
};

class SpecialItemModel final : public ItemModel {
public:
  SpecialItemModel() : ItemModel(ItemModelType::Special) {}

  std::unique_ptr<SpecialModel> model;
  std::string base; // namespaced id of a plain model providing
                    // transforms/particle/GUI light

protected:
  void serializeBody(json &j) const override;
  void deserializeBody(const json &j) override;
};

} // namespace enchantment_tweaks::item
