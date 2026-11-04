#pragma once
#include "types.h"
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace et {

std::string slugify(std::string_view s);

std::optional<std::string> validateItemDraft(const std::string &ns,
                                             const std::string &id,
                                             const std::string &name,
                                             const std::vector<Item> &existing);
std::optional<std::string>
validateNamespaceDraft(const std::string &label, const std::string &id,
                       const std::vector<Namespace> &existing);

std::string trim(std::string_view s);

} // namespace et
