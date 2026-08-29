#pragma once

#include "libraries/nlohmann/json.hpp"

using nlohmann::json;

namespace enchantment_tweaks {
class JsonSerializable {
private:
public:
  JsonSerializable() = default;
  virtual ~JsonSerializable() = default;

  virtual json serialize() const = 0;
  virtual void deserialize(const json &json) = 0;
};

struct Vec3 final : public JsonSerializable {
  Vec3(double x, double y, double z) : x(x), y(y), z(z) {}
  Vec3() : x(0), y(0), z(0) {}
  double x, y, z;

  json serialize() const override {
    json arr = json::array();
    arr.push_back(x);
    arr.push_back(y);
    arr.push_back(z);
    return arr;
  }

  void deserialize(const json &j) override {
    x = j.at(0).get<double>();
    y = j.at(1).get<double>();
    z = j.at(2).get<double>();
  }
};
} // namespace enchantment_tweaks
