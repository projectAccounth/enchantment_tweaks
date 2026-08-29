#include "core/api/item_io.hpp"
#include "core/types/mesh/types.hpp"
#include "libraries/nlohmann/json.hpp"

#include <fstream>
#include <stdexcept>

namespace enchantment_tweaks::item {

using nlohmann::json;

namespace {} // namespace

std::string serialize(const mesh::Model &model, int indent) {
  const json j = model.serialize();
  return indent < 0 ? j.dump() : j.dump(indent);
}

void writeFile(const std::string &path, const mesh::Model &model, int indent) {
  std::ofstream out(path, std::ios::binary);
  if (!out) {
    throw std::runtime_error(
        "enchantment_tweaks::item_io::writeFile: cannot open '" + path +
        "' for writing");
  }
  out << serialize(model, indent);
  if (!out) {
    throw std::runtime_error(
        "enchantment_tweaks::item_io::writeFile: write failed for '" + path +
        "'");
  }
}

} // namespace enchantment_tweaks::item
