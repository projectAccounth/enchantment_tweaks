#pragma once

#include "core/api/mesh.hpp"

#include <string>

namespace enchantment_tweaks::item {

std::string serialize(const mesh::Model& model, int indent = 2);

void writeFile(const std::string& path, const mesh::Model& model, int indent = 2);

}  // namespace enchantment_tweaks::item