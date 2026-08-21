#include "core/api/item_io.hpp"
#include "core/util/mesh/types.hpp"
#include "libraries/nlohmann/json.hpp"

#include <fstream>
#include <stdexcept>

namespace enchantment_tweaks::item {

using nlohmann::json;

namespace {

json ToJson(const mesh::Model &m) {
  json j;
  for (const auto &[name, id] : m.textures) {
    j["textures"][name] = id.getString();
  }

  json elements = json::array();
  for (const auto &e : m.elements)
    elements.push_back(e.serialize());
  j["elements"] = elements;

  json display = json::object();
  for (const auto &[name, transform] : m.display)
    display[mesh::stringFromTransformType(name)] = transform.serialize();
  j["display"] = display;

  return j;
}

} // namespace

std::string serialize(const mesh::Model &model, int indent) {
  const json j = ToJson(model);
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