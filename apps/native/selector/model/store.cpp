#include "store.h"
#include "selector/model/types.h"
#include <algorithm>
#include <cctype>
#include <cstddef>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace et {

namespace {
std::string lower(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  return s;
}
// Case-insensitive, ties broken by the raw comparison (stands in for JS
// localeCompare; use ICU if you need real locale rules).
int compare(const std::string &a, const std::string &b) {
  const std::string la = lower(a), lb = lower(b);
  if (int c = la.compare(lb)) return c;
  return a.compare(b);
}
std::string trim(const std::string &s) {
  size_t a = 0, b = s.size();
  while (a < b && std::isspace(static_cast<unsigned char>(s[a])))
    ++a;
  while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1])))
    --b;
  return s.substr(a, b - a);
}
template <class T> bool toggle(std::set<T> &set, const T &v) {
  if (set.erase(v)) return false;
  set.insert(v);
  return true;
}
} // namespace

Store::Store(Catalog catalog, const std::optional<PersistedState> &persisted)
    : state_(build(catalog, persisted)) {}

State Store::build(const Catalog &catalog,
                   const std::optional<PersistedState> &persisted) {
  State s;
  s.available_versions = catalog.versions;
  s.namespaces = catalog.namespaces;
  s.items = catalog.items;
  if (!catalog.versions.empty())
    s.versions.insert(catalog.versions.front()); // default: newest version

  auto hasItem = [&](const std::string &key) {
    return std::ranges::any_of(s.items,
                               [&](const Item &i) { return i.Key() == key; });
  };
  if (persisted) {
    for (Namespace n : persisted->custom_namespaces) {
      const bool taken = std::ranges::any_of(
          s.namespaces, [&](const Namespace &x) { return x.id == n.id; });
      if (!taken) {
        n.custom = true;
        s.namespaces.push_back(std::move(n));
      }
    }
    for (Item i : persisted->custom_items) {
      const bool nsok =
          std::any_of(s.namespaces.begin(), s.namespaces.end(),
                      [&](const Namespace &n) { return n.id == i.ns; });
      if (nsok && !hasItem(i.Key())) {
        i.custom = true;
        s.items.push_back(std::move(i));
      }
    }
    if (persisted->versions) {
      s.versions.clear();
      for (const auto &v : *persisted->versions)
        if (std::find(catalog.versions.begin(), catalog.versions.end(), v) !=
            catalog.versions.end())
          s.versions.insert(v);
    }
    if (persisted->created)
      for (const auto &k : *persisted->created)
        if (hasItem(k)) s.created.insert(k);
    if (persisted->sort) s.sort = *persisted->sort;
  }
  return s;
}

void Store::notify(unsigned changes) {
  for (auto &l : listeners_)
    l(changes);
}

void Store::toggleVersion(const std::string &v) {
  toggle(state_.versions, v);
  notify(change::Selection);
}

void Store::setVersions(const std::vector<std::string> &versions) {
  state_.versions.clear();
  for (const auto &v : versions)
    if (std::find(state_.available_versions.begin(),
                  state_.available_versions.end(),
                  v) != state_.available_versions.end())
      state_.versions.insert(v);
  notify(change::Selection);
}

void Store::toggleItem(const std::string &key) {
  toggle(state_.created, key);
  notify(change::Selection);
}
void Store::removeItem(const std::string &key) {
  state_.created.erase(key);
  notify(change::Selection);
}

void Store::setCreated(const std::vector<std::string> &keys) {
  state_.created.clear();
  for (const auto &k : keys)
    if (findItem(k)) state_.created.insert(k);
  notify(change::Selection);
}

void Store::toggleCollapsed(const std::string &key) {
  toggle(state_.collapsed, key);
  notify(change::View);
}
void Store::setSort(SortKey sort) {
  state_.sort = sort;
  notify(change::Prefs);
}
void Store::setQuery(const std::string &query) {
  state_.query = query;
  notify(change::View);
}
void Store::resetSelection() {
  state_.created.clear();
  notify(change::Selection);
}

bool Store::addNamespace(Namespace ns) {
  for (const auto &n : state_.namespaces)
    if (n.id == ns.id) return false;
  ns.custom = true;
  state_.namespaces.push_back(std::move(ns));
  notify(change::Catalog);
  return true;
}

bool Store::addItem(Item item) {
  const bool nsok = std::ranges::any_of(
      state_.namespaces, [&](const Namespace &n) { return n.id == item.ns; });
  if (!nsok || findItem(item.Key())) return false;
  item.custom = true;
  state_.collapsed.erase(
      collapse::Grid(item.ns)); // make sure the new item is visible
  state_.query.clear();
  state_.items.push_back(std::move(item));
  notify(change::Catalog | change::View);
  return true;
}

void Store::replaceCatalog(Catalog catalog) {
  State next = build(catalog, toPersisted());
  next.query = state_.query;
  next.collapsed = state_.collapsed;
  state_ = std::move(next);
  notify(change::Selection | change::Catalog | change::View);
}

const Item *Store::findItem(const std::string &key) const {
  for (const auto &i : state_.items)
    if (i.Key() == key) return &i;
  return nullptr;
}

CatalogView Store::getCatalogView() const {
  CatalogView view;
  view.query = trim(state_.query);
  view.filtering = !view.query.empty();
  const std::string q = lower(view.query);

  auto cmp = [&](const Item &a, const Item &b) {
    switch (state_.sort) {
    case SortKey::NameAsc:
      return compare(a.name, b.name) < 0;
    case SortKey::NameDesc:
      return compare(b.name, a.name) < 0;
    case SortKey::Id:
      return compare(a.id, b.id) < 0;
    case SortKey::Created: {
      const bool ca = state_.created.count(a.Key()) > 0,
                 cb = state_.created.count(b.Key()) > 0;
      if (ca != cb) return ca; // created first
      return compare(a.name, b.name) < 0;
    }
    }
    return false;
  };

  for (const Namespace &ns : state_.namespaces) {
    SectionView sec{
        ns, {}, {}, 0, state_.collapsed.count(collapse::Grid(ns.id)) == 0};
    for (const Item &i : state_.items)
      if (i.ns == ns.id) sec.items.push_back(i);
    std::stable_sort(sec.items.begin(), sec.items.end(), cmp);
    for (const Item &i : sec.items) {
      const bool match = !view.filtering ||
                         lower(i.name).find(q) != std::string::npos ||
                         lower(i.id).find(q) != std::string::npos;
      sec.visible.push_back(match);
      sec.visible_count += match;
    }
    view.total += sec.visible_count;
    view.sections.push_back(std::move(sec));
  }
  return view;
}

std::vector<CreatedGroup> Store::getCreatedGroups() const {
  std::vector<CreatedGroup> groups;
  for (const Namespace &ns : state_.namespaces) {
    CreatedGroup g{
        ns, {}, state_.collapsed.count(collapse::CreatedGroup(ns.id)) == 0};
    for (const Item &i : state_.items)
      if (i.ns == ns.id && state_.created.count(i.Key())) g.items.push_back(i);
    if (g.items.empty()) continue;
    std::stable_sort(g.items.begin(), g.items.end(),
                     [](const Item &a, const Item &b) {
                       return compare(a.name, b.name) < 0;
                     });
    groups.push_back(std::move(g));
  }
  return groups;
}

PersistedState Store::toPersisted() const {
  PersistedState p;
  p.versions =
      std::vector<std::string>(state_.versions.begin(), state_.versions.end());
  p.created =
      std::vector<std::string>(state_.created.begin(), state_.created.end());
  p.sort = state_.sort;
  for (const auto &n : state_.namespaces)
    if (n.custom)
      p.custom_namespaces.push_back(
          {.id = n.id, .label = n.label, .custom = false});
  for (const auto &i : state_.items)
    if (i.custom)
      p.custom_items.push_back({.ns = i.ns,
                                .id = i.id,
                                .name = i.name,
                                .icon = i.icon,
                                .custom = false});
  return p;
}

std::vector<Item> Store::selectedItems() const {
  std::vector<Item> out;
  for (const auto &k : state_.created)
    if (const Item *i = findItem(k)) out.push_back(*i);
  return out;
}

std::vector<std::string> Store::selectedVersions() const {
  std::vector<std::string> out;
  for (const auto &v : state_.available_versions)
    if (state_.versions.count(v)) out.push_back(v);
  return out;
}

} // namespace et
