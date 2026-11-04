#include <algorithm>
#include "json.h"
#include "app/model/types.h"
#include "core/util/file_io.hpp"
#include <filesystem>
#include <fstream>
#include <ios>
#include <libraries/nlohmann/json.hpp>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

namespace et {
using nlohmann::json;

namespace {
std::optional<std::string> Str(const json &j, const char *key) {
  auto it = j.find(key);
  if (it == j.end() || !it->is_string()) return std::nullopt;
  return it->get<std::string>();
}

std::vector<std::string> stringArray(const json &j) {
  std::vector<std::string> out;
  if (j.is_array())
    for (const auto &v : j)
      if (v.is_string()) out.push_back(v.get<std::string>());
  return out;
}

std::vector<Namespace> parseNamespaces(const json &arr) {
  std::vector<Namespace> out;
  std::set<std::string> seen;
  if (!arr.is_array()) return out;
  for (const auto &n : arr) {
    if (!n.is_object()) continue;
    auto id = Str(n, "id");
    auto label = Str(n, "label");
    if (id && label && seen.insert(*id).second)
      out.push_back({*id, *label, false});
  }
  return out;
}

std::vector<Item> parseItems(const json &arr,
                             const std::vector<Namespace> *known) {
  std::vector<Item> out;
  std::set<std::string> seen;
  if (!arr.is_array()) return out;
  for (const auto &i : arr) {
    if (!i.is_object()) continue;
    auto ns = Str(i, "ns");
    auto id = Str(i, "id");
    auto name = Str(i, "name");
    if (!ns || !id || !name) continue;
    if (known) {
      const bool ok = std::any_of(known->begin(), known->end(),
                                  [&](const auto &n) { return n.id == *ns; });
      if (!ok) continue;
    }
    Item item{.ns = *ns,
              .id = *id,
              .name = *name,
              .icon = Str(i, "icon").value_or(""),
              .custom = false};
    if (seen.insert(item.Key()).second) out.push_back(std::move(item));
  }
  return out;
}

json namespaceJson(const Namespace &n) {
  return json{{"id", n.id}, {"label", n.label}};
}
json itemJson(const Item &i) {
  json j{{"ns", i.ns}, {"id", i.id}, {"name", i.name}};
  if (!i.icon.empty()) j["icon"] = i.icon;
  return j;
}
} // namespace

std::optional<Catalog> parseCatalog(const std::string &text) {
  json j = json::parse(text, nullptr, /*allow_exceptions=*/false);
  if (!j.is_object()) return std::nullopt;
  Catalog c;
  c.versions = stringArray(j.value("versions", json::array()));
  c.namespaces = parseNamespaces(j.value("namespaces", json::array()));
  if (c.versions.empty() || c.namespaces.empty()) return std::nullopt;
  c.items = parseItems(j.value("items", json::array()), &c.namespaces);
  return c;
}

std::optional<PersistedState> parsePersisted(const std::string &text) {
  json j = json::parse(text, nullptr, false);
  if (!j.is_object()) return std::nullopt;
  PersistedState p;
  if (j.contains("versions") && j["versions"].is_array())
    p.versions = stringArray(j["versions"]);
  if (j.contains("created") && j["created"].is_array())
    p.created = stringArray(j["created"]);
  if (auto s = Str(j, "sort")) p.sort = sortKeyFromString(*s);
  p.custom_namespaces =
      parseNamespaces(j.value("customNamespaces", json::array()));
  p.custom_items = parseItems(j.value("customItems", json::array()),
                              nullptr); // re-validated in Store
  return p;
}

std::string toJson(const PersistedState &p) {
  json j = json::object();
  if (p.versions) j["versions"] = *p.versions;
  if (p.created) j["created"] = *p.created;
  if (p.sort) j["sort"] = toString(*p.sort);
  j["customNamespaces"] = json::array();
  for (const auto &n : p.custom_namespaces)
    j["customNamespaces"].push_back(namespaceJson(n));
  j["customItems"] = json::array();
  for (const auto &i : p.custom_items)
    j["customItems"].push_back(itemJson(i));
  return j.dump(2);
}

bool writeFile(const fs::path &path, const std::string &text) {
  std::error_code ec;
  if (path.has_parent_path())
    fs::create_directories(path.parent_path(), ec);
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  if (!out) return false;
  out << text;
  return static_cast<bool>(out);
}

} // namespace et
