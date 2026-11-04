#include "catalog_source.hpp"

#include "app/model/types.h"
#include "core/api/item_catalog.hpp"

#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <utility>

namespace enchantment_tweaks::native {

namespace {

std::string namespaceLabel(const std::string &ns) {
  static const std::unordered_map<std::string, std::string> labelMap = {
      {"minecraft", "Vanilla"}};

  return labelMap.count(ns) ? labelMap.at(ns) : ns;
}

} // namespace

et::Catalog catalogFromItems(const catalog::ItemCatalog &source,
                             const std::map<std::string, std::string> &icons) {
  et::Catalog out;
  out.versions.push_back(source.gameVersion().empty() ? "unknown"
                                                      : source.gameVersion());

  std::set<std::string> seenNamespaces;
  for (const auto &item : source.items()) {
    if (seenNamespaces.insert(item.ns).second) {
      out.namespaces.push_back({item.ns, namespaceLabel(item.ns), false});
    }
    et::Item ui;

    ui.ns = item.ns;
    ui.id = item.id;
    ui.name = item.name;

    if (const auto icon = icons.find(item.key()); icon != icons.end()) {
      ui.icon = icon->second;
    }
    out.items.push_back(std::move(ui));
  }
  return out;
}

} // namespace enchantment_tweaks::native
