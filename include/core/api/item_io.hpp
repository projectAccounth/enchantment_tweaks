#pragma once

#include "core/types/mesh/types.hpp"
#include "libraries/nlohmann/json.hpp"

#include <string>

namespace enchantment_tweaks::item {

using nlohmann::json;

mesh::Model fromJson(const json &json);

std::string serialize(const mesh::Model &model, int indent = 2);

void writeFile(const std::string &path, const mesh::Model &model,
               int indent = 2);

} // namespace enchantment_tweaks::item
