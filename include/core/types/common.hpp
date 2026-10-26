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
  Vec3(double x, double y, double z);
  Vec3();
  double x, y, z;

  json serialize() const override;
  void deserialize(const json &j) override;
};
} // namespace enchantment_tweaks
