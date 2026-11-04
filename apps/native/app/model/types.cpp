#include "types.h"
#include <optional>
#include <string_view>

namespace et {

const char *toString(SortKey key) {
  switch (key) {
  case SortKey::NameAsc:
    return "name-asc";
  case SortKey::NameDesc:
    return "name-desc";
  case SortKey::Id:
    return "id";
  case SortKey::Created:
    return "created";
  }
  return "name-asc";
}

std::optional<SortKey> sortKeyFromString(std::string_view s) {
  if (s == "name-asc") return SortKey::NameAsc;
  if (s == "name-desc") return SortKey::NameDesc;
  if (s == "id") return SortKey::Id;
  if (s == "created") return SortKey::Created;
  return std::nullopt;
}

} // namespace et
