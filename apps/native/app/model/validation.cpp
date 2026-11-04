#include "validation.h"
#include "app/model/types.h"
#include <algorithm>
#include <cctype>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace et {

std::string trim(std::string_view s) {
  size_t a = 0, b = s.size();
  while (a < b && std::isspace(static_cast<unsigned char>(s[a])))
    ++a;
  while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1])))
    --b;
  return std::string(s.substr(a, b - a));
}

std::string slugify(std::string_view s) {
  std::string out;
  bool pending_sep = false;
  for (unsigned char c : trim(s)) {
    if (std::isalnum(c) || c == '_') {
      if (pending_sep && !out.empty()) out += '_';
      pending_sep = false;
      out += static_cast<char>(std::tolower(c));
    } else {
      pending_sep =
          true; // runs of other characters collapse into one underscore
    }
  }
  return out;
}

static bool AllOf(const std::string &s, bool (*ok)(unsigned char)) {
  return !s.empty() && std::all_of(s.begin(), s.end(), [&](char c) {
    return ok(static_cast<unsigned char>(c));
  });
}
static bool ItemIdChar(unsigned char c) {
  return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_';
}
static bool NamespaceIdChar(unsigned char c) {
  return ItemIdChar(c) || c == '.' || c == '-';
}

std::optional<std::string>
validateItemDraft(const std::string &ns, const std::string &id_in,
                  const std::string &name_in,
                  const std::vector<Item> &existing) {
  const std::string name = trim(name_in), id = trim(id_in);
  if (name.empty()) return "Enter an item name.";
  if (!AllOf(id, ItemIdChar))
    return "Item ID can only use lowercase letters, numbers and underscores.";
  for (const Item &i : existing)
    if (i.ns == ns && i.id == id) {
      std::string message = ns;
      message += ':';
      message += id;
      message += " already exists. Choose a different ID.";
      return message;
    }
  return std::nullopt;
}

std::optional<std::string>
validateNamespaceDraft(const std::string &label_in, const std::string &id_in,
                       const std::vector<Namespace> &existing) {
  const std::string label = trim(label_in), id = trim(id_in);
  if (label.empty()) return "Enter a display name.";
  // Not purely dots: the id becomes a folder name in the pack, so "." and ".."
  // are out.
  const bool only_dots =
      !id.empty() &&
      std::all_of(id.begin(), id.end(), [](char c) { return c == '.'; });
  if (!AllOf(id, NamespaceIdChar) || only_dots)
    return "Namespace ID can only use lowercase letters, numbers, underscores, "
           "dots and dashes.";
  for (const Namespace &n : existing)
    if (n.id == id)
      return "The namespace \xE2\x80\x9C" + id + "\xE2\x80\x9D already exists.";
  return std::nullopt;
}

} // namespace et
