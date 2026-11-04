#pragma once

#include "app/model/types.h"
#include "core/api/item_catalog.hpp"

#include <map>
#include <string>

namespace enchantment_tweaks::native {

/// Builds the selector catalog, using the placeholder for items without icons.
et::Catalog catalogFromItems(const catalog::ItemCatalog &source,
                             const std::map<std::string, std::string> &icons);

} // namespace enchantment_tweaks::native
