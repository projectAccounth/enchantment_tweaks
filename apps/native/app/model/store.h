#pragma once
#include "types.h"
#include <cstddef>
#include <functional>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace et {

struct State {
  std::vector<std::string> available_versions;
  std::vector<Namespace> namespaces;
  std::vector<Item> items;
  std::set<std::string> versions;  // selected versions
  std::set<std::string> created;   // selected items
  std::set<std::string> collapsed; // collapsed panels
  SortKey sort = SortKey::NameAsc;
  std::string query;
};

namespace collapse {
inline std::string VersionsPanel() { return "side:versions"; }
inline std::string CreatedPanel() { return "side:created"; }
inline std::string CreatedGroup(const std::string &ns) { return "side:" + ns; }
inline std::string Grid(const std::string &ns) { return "grid:" + ns; }
} // namespace collapse

struct SectionView {
  Namespace ns;
  std::vector<Item>
      items; // ALL items of the namespace, in the current sort order
  std::vector<char> visible; // parallel to items: does it match the search?
  size_t visible_count = 0;
  bool open = true;
};

struct CatalogView {
  std::vector<SectionView> sections;
  size_t total = 0; // visible items across all sections
  bool filtering = false;
  std::string query; // trimmed
};
struct CreatedGroup {
  Namespace ns;
  std::vector<Item> items;
  bool open = true;
};

namespace change {
constexpr unsigned Selection = 1; // versions or created items
constexpr unsigned Catalog = 2;   // namespaces or items
constexpr unsigned Prefs = 4;     // sort order
constexpr unsigned View = 8;      // search text, collapsed panels
constexpr unsigned Persisted = Selection | Catalog | Prefs;
} // namespace change

class Store {
public:
  using Listener = std::function<void(unsigned changes)>;

  explicit Store(Catalog catalog,
                 const std::optional<PersistedState> &persisted = std::nullopt);

  const State &state() const { return state_; }
  void Subscribe(Listener listener) {
    listeners_.push_back(std::move(listener));
  }

  void toggleVersion(const std::string &version);
  void setVersions(const std::vector<std::string> &versions);
  void toggleItem(const std::string &key);
  void removeItem(const std::string &key);
  void setCreated(const std::vector<std::string> &keys);
  void toggleCollapsed(const std::string &key);
  void setSort(SortKey sort);
  void setQuery(const std::string &query);
  void resetSelection();
  bool addNamespace(Namespace ns);
  bool addItem(Item item);
  void replaceCatalog(Catalog catalog);

  CatalogView getCatalogView() const;
  std::vector<CreatedGroup> getCreatedGroups() const;
  const Item *findItem(const std::string &key) const;
  PersistedState toPersisted() const;
  std::vector<Item> selectedItems() const;
  std::vector<std::string> selectedVersions() const; // in catalog order

private:
  static State build(const Catalog &catalog,
                     const std::optional<PersistedState> &persisted);
  void notify(unsigned changes);

  State state_;
  std::vector<Listener> listeners_;
};

} // namespace et
