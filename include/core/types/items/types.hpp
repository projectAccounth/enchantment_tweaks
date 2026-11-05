#pragma once

#include "core/types/common.hpp"
#include "core/util/json/utils.hpp"

#include <array>
#include <cstdint>
#include <optional>

namespace enchantment_tweaks::item {

class RgbColor final : public JsonSerializable {
public:
  RgbColor() = default;
  explicit RgbColor(int32_t packed);
  explicit RgbColor(std::array<double, 3> rgb);

  bool isPacked() const;
  int32_t packed() const;
  std::array<double, 3> rgb() const;

  json serialize() const override;
  void deserialize(const json &j) override;

  std::optional<int32_t> packed_;
  std::optional<std::array<double, 3>> rgb_;
};

class Transformation final : public JsonSerializable {
public:
  std::optional<std::array<double, 16>> matrix;
  std::optional<Vec3> translation;
  std::optional<Vec3> scale;
  std::optional<Vec3> rotation;
  std::optional<Vec3> rightRotation;

  json serialize() const override;
  void deserialize(const json &j) override;
};

} // namespace enchantment_tweaks::item
