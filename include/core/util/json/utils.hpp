#pragma once

#include <libraries/nlohmann/json.hpp>

#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using nlohmann::json;

namespace util {

template <typename EnumT>
using EnumTable = std::vector<std::pair<EnumT, std::string>>;

template <typename EnumT>
std::string enumToString(EnumT value, const EnumTable<EnumT> &table) {
  for (const auto &entry : table) {
    if (entry.first == value) {
      return entry.second;
    }
  }
  throw std::invalid_argument("mc: no wire string registered for enum value");
}

template <typename EnumT>
EnumT enumFromString(const std::string &value, const EnumTable<EnumT> &table) {
  for (const auto &entry : table) {
    if (entry.second == value) {
      return entry.first;
    }
  }
  throw std::invalid_argument("mc: unrecognized enum string '" + value + "'");
}

template <typename T>
std::optional<T> readOptional(const json &j, const std::string &key) {
  const auto it = j.find(key);
  if (it == j.end() || it->is_null()) {
    return std::nullopt;
  }
  return it->get<T>();
}

template <typename T>
void writeOptional(json &j, const std::string &key,
                   const std::optional<T> &value) {
  if (value.has_value()) {
    j[key] = *value;
  }
}

} // namespace util
