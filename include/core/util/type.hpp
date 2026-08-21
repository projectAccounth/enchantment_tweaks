#pragma once

#include "libraries/nlohmann/json.hpp"
namespace enchantment_tweaks {

using nlohmann::json;

class JsonSerializable {
private:
public:
  JsonSerializable() = default;
  virtual ~JsonSerializable() = default;

  virtual json serialize() const = 0;
  virtual void deserialize(const json &json) = 0;
};

} // namespace enchantment_tweaks