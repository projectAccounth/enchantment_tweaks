#pragma once
// Plain data types shared by the model and UI layers. No RmlUi, no JSON library
// here. The JSON shapes (see Json.cpp) are identical to the web app's, so one
// catalog.json serves both.

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace et {

struct Namespace {
  std::string id;
  std::string label;
  bool custom = false; // created by the user in the UI
};

struct Item {
  std::string ns;
  std::string id;
  std::string name;
  std::string
      icon; // sprite name ("sword"), a file path, or empty (placeholder)
  bool custom = false;

  std::string Key() const { return ns + ":" + id; }
};

enum class SortKey { NameAsc, NameDesc, Id, Created };

const char *toString(SortKey key);
std::optional<SortKey> sortKeyFromString(std::string_view s);

/// Read-only data the UI is built from.
struct Catalog {
  std::vector<std::string> versions;
  std::vector<Namespace> namespaces;
  std::vector<Item> items;
};

/// What is saved between sessions (same shape as the web app's localStorage
/// entry).
struct PersistedState {
  std::optional<std::vector<std::string>> versions;
  std::optional<std::vector<std::string>> created; // "namespace:id"
  std::optional<SortKey> sort;
  std::vector<Namespace> custom_namespaces;
  std::vector<Item> custom_items;
};

struct PackFile {
  std::string path;
  std::string text;
};

} // namespace et
