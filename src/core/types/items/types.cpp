#include "core/types/items/types.hpp"
#include "libraries/nlohmann/json.hpp"

#include <array>
#include <cstdint>
#include <cstdlib>

namespace enchantment_tweaks::item {

using nlohmann::json;

RgbColor::RgbColor(int32_t packed) : packed_(packed) {}
RgbColor::RgbColor(std::array<double, 3> rgb) : rgb_(rgb) {}

bool RgbColor::isPacked() const { return packed_.has_value(); }
int32_t RgbColor::packed() const { return packed_.value_or(0); }
std::array<double, 3> RgbColor::rgb() const {
  return rgb_.value_or(std::array<double, 3>{0, 0, 0});
}

json RgbColor::serialize() const {
  json arr = json::array();
  if (packed_.has_value()) {
    return *packed_;
  }
  if (rgb_.has_value()) {
    for (double component : *rgb_) {
      arr.push_back(component);
    }
    return arr;
  }
  return {0};
}

void RgbColor::deserialize(const json &j) {
  packed_.reset();
  rgb_.reset();
  if (j.is_array() && j.size() == 3) {
    std::array<double, 3> arr{0, 0, 0};
    for (size_t i = 0; i < arr.size() && i < j.size(); ++i) {
      arr[i] = j.at(i).get<double>();
    }
    rgb_ = arr;
  } else {
    packed_ = j.get<int32_t>();
  }
}

json Transformation::serialize() const {
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

void Transformation::deserialize(const json &j) {
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

} // namespace enchantment_tweaks::item
