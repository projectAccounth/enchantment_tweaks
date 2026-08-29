#pragma once

#include "core/types/common.hpp"
#include "core/util/json/utils.hpp"

#include <array>
#include <optional>

using namespace util;

namespace enchantment_tweaks::item {

class RgbColor final : public JsonSerializable {
public:
  RgbColor() = default;
  explicit RgbColor(int32_t packed) : packed_(packed) {}
  explicit RgbColor(std::array<double, 3> rgb) : rgb_(rgb) {}

  bool isPacked() const { return packed_.has_value(); }
  int32_t packed() const { return packed_.value_or(0); }
  std::array<double, 3> rgb() const {
    return rgb_.value_or(std::array<double, 3>{0, 0, 0});
  }

  json serialize() const override {
    if (packed_.has_value()) {
      return {*packed_};
    }
    if (rgb_.has_value()) {
      json arr = json::array();
      for (double component : *rgb_) {
        arr.push_back(component);
      }
      return arr;
    }
    return {0};
  }

  void deserialize(const json &j) override {
    packed_.reset();
    rgb_.reset();
    if (j.is_array()) {
      std::array<double, 3> arr{0, 0, 0};
      for (size_t i = 0; i < arr.size() && i < j.size(); ++i) {
        arr[i] = j.at(i).get<double>();
      }
      rgb_ = arr;
    } else {
      packed_ = j.get<int32_t>();
    }
  }

private:
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

  json serialize() const override {
    if (matrix.has_value()) {
      json arr = json::array();
      for (double value : *matrix) {
        arr.push_back(value);
      }
      return arr;
    }
    json j = json::object();
    writeOptional(j, "translation", translation);
    if (scale.has_value()) {
      j["scale"] = scale->serialize();
    }
    if (rotation.has_value()) {
      j["rotation"] = rotation->serialize();
    }
    if (rightRotation.has_value()) {
      j["right_rotation"] = rightRotation->serialize();
    }
    return j;
  }

  void deserialize(const json &j) override {
    matrix.reset();
    translation.reset();
    scale.reset();
    rotation.reset();
    rightRotation.reset();

    if (j.is_array()) {
      std::array<double, 16> m{};
      for (size_t i = 0; i < m.size() && i < j.size(); ++i) {
        m[i] = j.at(i).get<double>();
      }
      matrix = m;
      return;
    }
    if (j.contains("translation")) {
      Vec3 v;
      v.deserialize(j.at("translation"));
      translation = v;
    }
    if (j.contains("scale")) {
      Vec3 v;
      v.deserialize(j.at("scale"));
      scale = v;
    }
    if (j.contains("rotation")) {
      Vec3 v;
      v.deserialize(j.at("rotation"));
      rotation = v;
    }
    if (j.contains("right_rotation")) {
      Vec3 v;
      v.deserialize(j.at("right_rotation"));
      rightRotation = v;
    }
  }
};

} // namespace enchantment_tweaks::item
