#pragma once

#include "core/project/minecraft_download.hpp"

#include <cstddef>
#include <set>
#include <string>
#include <vector>

namespace enchantment_tweaks::project {

struct VersionRow {
  std::string id;
  std::string type;
  bool installed = false; // present anywhere
  bool managed = false; // downloaded by the app, so it can be uninstalled here
};

struct VersionGroup {
  std::string type;
  size_t total = 0;   // versions of this type, before the search
  size_t matches = 0; // versions of this type matching the search
  bool open = false;
  /// Populated only for open groups, up to the configured row limit.
  std::vector<VersionRow> rows;
  bool truncated = false; // more matches than rowLimit
};

/// Version filters used by the download list.
struct VersionFilter {
  std::string minRelease = "1.21.4"; // releases below this are hidden
  bool includeSnapshots = false;     // snapshots are off by default
  // TODO: support new snapshot format
  std::string snapshotCutoff = "24w44a";
};

/// Returns true when a dotted-numeric release ID meets the minimum version.
bool releaseAtLeast(const std::string &id, const std::string &minimum);

/// Returns true when a version passes the filter.
bool passesFilter(const RemoteVersion &version, const VersionFilter &filter);

struct VersionViewOptions {
  std::string query;               // case-insensitive, matches id or type
  std::set<std::string> openTypes; // groups the user has expanded
  size_t rowLimit = 200;
  VersionFilter filter;
};

/// Groups manifest entries by type, preserving manifest order.
std::vector<VersionGroup>
groupVersions(const std::vector<RemoteVersion> &versions,
              const std::set<std::string> &installedIds,
              const std::set<std::string> &managedIds,
              const VersionViewOptions &options);

} // namespace enchantment_tweaks::project
