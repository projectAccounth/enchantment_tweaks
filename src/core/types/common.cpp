#include "core/types/common.hpp"

namespace enchantment_tweaks {

Vec3::Vec3(double x, double y, double z) : x(x), y(y), z(z) {}
Vec3::Vec3() : x(0), y(0), z(0) {}

json Vec3::serialize() const {
  json arr = json::array();
  arr.push_back(x);
  arr.push_back(y);
  arr.push_back(z);
  return arr;
}

void Vec3::deserialize(const json &j) {
  x = j.at(0).get<double>();
  y = j.at(1).get<double>();
  z = j.at(2).get<double>();
}

} // namespace enchantment_tweaks
