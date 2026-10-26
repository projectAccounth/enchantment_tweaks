#pragma once
#include "types.h"
#include <optional>
#include <string>
#include <vector>

namespace et {

/// "Diamond Sword!" -> "diamond_sword". Used to derive an item ID from its
/// name.
std::string slugify(std::string_view s);

/// Return an error message, or nullopt when valid. Messages match the web app.
std::optional<std::string> validateItemDraft(const std::string &ns,
                                             const std::string &id,
                                             const std::string &name,
                                             const std::vector<Item> &existing);
std::optional<std::string>
validateNamespaceDraft(const std::string &label, const std::string &id,
                       const std::vector<Namespace> &existing);

std::string trim(std::string_view s);

} // namespace et
