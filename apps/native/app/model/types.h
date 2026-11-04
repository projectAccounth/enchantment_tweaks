#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

// TODO: Move these UI types into the main project namespace.
namespace et {

struct Namespace {
  std::string id;
  std::string label;
  bool custom = false;
};

struct Item {
  std::string ns;
  std::string id;
  std::string name;
  std::string icon;
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

struct PersistedState {
  std::optional<std::vector<std::string>> versions;
  std::optional<std::vector<std::string>> created;
  std::optional<SortKey> sort;
  std::vector<Namespace> custom_namespaces;
  std::vector<Item> custom_items;
};

struct PackFile {
  std::string path;
  std::string text;
};

} // namespace et
