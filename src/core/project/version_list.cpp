#include "core/project/version_list.hpp"

#include "core/project/minecraft_download.hpp"
#include "core/util/text.hpp"
#include <algorithm>
#include <cctype>
#include <cstddef>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace enchantment_tweaks::project {

namespace {

// "1.21.10" -> {1, 21, 10}. Returns false unless every part is a plain number.
bool parseDotted(const std::string &id, std::vector<long> &parts) {
  parts.clear();
  if (id.empty()) return false;
  size_t start = 0;
  while (start <= id.size()) {
    const size_t dot = id.find('.', start);
    const std::string piece = id.substr(
        start, dot == std::string::npos ? std::string::npos : dot - start);
    if (piece.empty() || piece.size() > 6 ||
        !std::all_of(piece.begin(), piece.end(),
                     [](unsigned char c) { return std::isdigit(c) != 0; })) {
      return false;
    }
    parts.push_back(std::stol(piece));
    if (dot == std::string::npos) break;
    start = dot + 1;
  }
  return true;
}

} // namespace

bool releaseAtLeast(const std::string &id, const std::string &minimum) {
  std::vector<long> a, b;
  if (!parseDotted(id, a) || !parseDotted(minimum, b)) return false;
  const size_t n = std::max(a.size(), b.size());
  for (size_t i = 0; i < n; ++i) {
    const long x = i < a.size() ? a[i] : 0;
    const long y = i < b.size() ? b[i] : 0;
    if (x != y) return x > y;
  }
  return true; // equal
}

namespace {

// Weekly snapshot ids: "YYwWWx", e.g. "24w44a". A text comparison orders them.
bool isWeeklySnapshot(const std::string &id) {
  return id.size() == 6 && id[2] == 'w';
}

// Numbered snapshot ids: "major.minor-snapshot-N"
struct NumberedSnapshot {
  long major = 0;
  long minor = 0;
  long number = 0;
};

std::optional<NumberedSnapshot> parseNumberedSnapshot(const std::string &id) {
  const std::string marker = "-snapshot-";
  const size_t at = id.find(marker);
  if (at == std::string::npos) return std::nullopt;
  const std::string version = id.substr(0, at);
  const std::string number = id.substr(at + marker.size());
  const size_t dot = version.find('.');
  if (dot == std::string::npos || number.empty()) return std::nullopt;
  const auto allDigits = [](const std::string &t) {
    return !t.empty() && std::ranges::all_of(t, [](unsigned char c) {
      return std::isdigit(c) != 0;
    });
  };
  if (!allDigits(version.substr(0, dot)) ||
      !allDigits(version.substr(dot + 1)) || !allDigits(number)) {
    return std::nullopt;
  }
  NumberedSnapshot out;
  out.major = std::stol(version.substr(0, dot));
  out.minor = std::stol(version.substr(dot + 1));
  out.number = std::stol(number);
  return out;
}

bool numberedAtLeast(const NumberedSnapshot &a, const NumberedSnapshot &b) {
  if (a.major != b.major) return a.major > b.major;
  if (a.minor != b.minor) return a.minor > b.minor;
  return a.number >= b.number;
}

} // namespace

bool passesFilter(const RemoteVersion &version, const VersionFilter &filter) {
  if (version.type == "release")
    return releaseAtLeast(version.id, filter.minRelease);
  if (version.type != "snapshot" || !filter.includeSnapshots) return false;

  if (const auto numbered = parseNumberedSnapshot(version.id)) {
    // Numbered snapshots are newer than every weekly one, so they pass against
    // a weekly cutoff.
    if (isWeeklySnapshot(filter.snapshotCutoff)) return true;
    const auto cutoff = parseNumberedSnapshot(filter.snapshotCutoff);
    return cutoff && numberedAtLeast(*numbered, *cutoff);
  }
  // A weekly snapshot never passes against a numbered cutoff: it is older than
  // that cutoff.
  return isWeeklySnapshot(version.id) &&
         isWeeklySnapshot(filter.snapshotCutoff) &&
         version.id >= filter.snapshotCutoff;
}

std::vector<VersionGroup>
groupVersions(const std::vector<RemoteVersion> &versions,
              const std::set<std::string> &installedIds,
              const std::set<std::string> &managedIds,
              const VersionViewOptions &options) {
  const std::string query = text::asciiLower(options.query);
  const bool searching = !query.empty();

  std::vector<VersionGroup> groups;
  std::map<std::string, size_t> indexOf;
  for (const auto &v : versions) {
    if (!passesFilter(v, options.filter)) continue;
    auto found = indexOf.find(v.type);
    if (found == indexOf.end()) {
      indexOf[v.type] = groups.size();
      VersionGroup g;
      g.type = v.type;
      groups.push_back(std::move(g));
      found = indexOf.find(v.type);
    }
    VersionGroup &group = groups[found->second];
    ++group.total;

    if (searching && text::asciiLower(v.id).find(query) == std::string::npos &&
        text::asciiLower(v.type).find(query) == std::string::npos) {
      continue;
    }
    ++group.matches;
    if (group.rows.size() >= options.rowLimit) {
      group.truncated = true;
      continue;
    }
    if (options.openTypes.count(v.type) > 0 || searching) {
      VersionRow row;
      row.id = v.id;
      row.type = v.type;
      row.installed = installedIds.count(v.id) > 0;
      row.managed = managedIds.count(v.id) > 0;
      group.rows.push_back(std::move(row));
    }
  }

  for (auto &g : groups) {
    g.open =
        options.openTypes.count(g.type) > 0 || (searching && g.matches > 0);
    if (!g.open) {
      g.rows.clear();
      g.truncated = false;
    }
  }
  // A search that matched nothing in a group should not show that group.
  if (searching) {
    groups.erase(
        std::remove_if(groups.begin(), groups.end(),
                       [](const VersionGroup &g) { return g.matches == 0; }),
        groups.end());
  }
  return groups;
}

} // namespace enchantment_tweaks::project
