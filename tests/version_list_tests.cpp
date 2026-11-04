#include "core/project/version_list.hpp"
#include "core/util/debug.hpp"
#include "test_utils.hpp"

#include <string>
#include <vector>

using namespace enchantment_tweaks;
using namespace enchantment_tweaks::project;

namespace {

// 3 releases, 4 snapshots, 2 old betas. The old betas never pass the filter.
std::vector<RemoteVersion> sampleManifest() {
  std::vector<RemoteVersion> v;
  auto add = [&](const char *id, const char *type) {
    v.push_back({id, type, std::string("u/") + id});
  };
  add("1.21.4", "release");
  add("1.21.5", "release");
  add("1.21.10", "release");
  add("24w44a", "snapshot");
  add("24w45a", "snapshot");
  add("25w01a", "snapshot");
  add("26w01a", "snapshot");
  add("b1.7.3", "old_beta");
  add("b1.6", "old_beta");
  return v;
}

// Options that keep the sample data: snapshots on, and every release from 1.0
// on.
VersionViewOptions keepAll() {
  VersionViewOptions o;
  o.filter.minRelease = "1.0";
  o.filter.includeSnapshots = true;
  return o;
}

const VersionGroup *find(const std::vector<VersionGroup> &groups,
                         const std::string &type) {
  for (const auto &g : groups) {
    if (g.type == type) return &g;
  }
  return nullptr;
}

} // namespace

int main() {
  test_util::Suite suite;

  suite.run("groups keep manifest order and count what passes the filter", [] {
    const auto groups = groupVersions(sampleManifest(), {}, {}, keepAll());
    test_util::check(groups.size() == 2,
                     "expected 2 groups (old betas are dropped)");
    if (groups.size() == 2) {
      test_util::check(groups[0].type == "release" &&
                           groups[1].type == "snapshot",
                       "group order wrong");
      test_util::check(groups[1].total == 4 && groups[1].matches == 4,
                       "snapshot counts wrong");
    }
  });

  suite.run("closed groups build no rows", [] {
    const auto groups = groupVersions(sampleManifest(), {}, {}, keepAll());
    for (const auto &g : groups) {
      test_util::check(g.rows.empty() && !g.open,
                       "closed group '" + g.type + "' has rows");
    }
  });

  suite.run("an opened group lists its versions with install state", [] {
    VersionViewOptions o = keepAll();
    o.openTypes = {"release"};
    const auto groups =
        groupVersions(sampleManifest(), {"1.21.4"}, {"1.21.4"}, o);
    const auto *release = find(groups, "release");
    test_util::check(release && release->open && release->rows.size() == 3,
                     "release group not opened");
    if (release && release->rows.size() == 3) {
      test_util::check(release->rows[0].installed && release->rows[0].managed,
                       "install state lost");
      test_util::check(!release->rows[1].installed,
                       "wrong row marked installed");
    }
    test_util::check(find(groups, "snapshot") &&
                         find(groups, "snapshot")->rows.empty(),
                     "other groups should stay closed");
  });

  suite.run("search matches ids and types, case-insensitively", [] {
    VersionViewOptions o = keepAll();
    o.query = "24W";
    const auto groups = groupVersions(sampleManifest(), {}, {}, o);
    test_util::check(groups.size() == 1 && groups[0].type == "snapshot",
                     "search did not narrow to snapshots");
    if (!groups.empty()) {
      test_util::check(groups[0].matches == 2 && groups[0].rows.size() == 2 &&
                           groups[0].open,
                       "search matches not listed");
    }
    VersionViewOptions release = keepAll();
    release.query = "release";
    const auto byType = groupVersions(sampleManifest(), {}, {}, release);
    test_util::check(byType.size() == 1 && byType[0].matches == 3,
                     "type search failed");
  });

  suite.run("a search with no matches shows no groups", [] {
    VersionViewOptions o = keepAll();
    o.query = "nothing like this";
    test_util::check(groupVersions(sampleManifest(), {}, {}, o).empty(),
                     "empty search still lists groups");
  });

  suite.run("the default filter hides everything below 1.21.4", [] {
    VersionViewOptions o;
    o.openTypes = {"release"};
    const auto groups = groupVersions(sampleManifest(), {}, {}, o);
    test_util::check(groups.size() == 1 && groups[0].total == 3,
                     "default filter wrong");
  });

  suite.run("a row limit caps the rows and reports truncation", [] {
    std::vector<RemoteVersion> many;
    for (int i = 0; i < 50; ++i)
      many.push_back({"1." + std::to_string(i + 4), "release", "u"});
    VersionViewOptions o = keepAll();
    o.openTypes = {"release"};
    o.rowLimit = 10;
    const auto groups = groupVersions(many, {}, {}, o);
    test_util::check(groups.size() == 1, "expected one group");
    if (!groups.empty()) {
      test_util::check(groups[0].rows.size() == 10 && groups[0].truncated,
                       "limit not applied");
      test_util::check(groups[0].total == 50,
                       "total should still count everything");
    }
  });

  suite.run("release ids compare as dotted numbers", [] {
    test_util::check(releaseAtLeast("1.21.4", "1.21.4"),
                     "equal ids must match");
    test_util::check(releaseAtLeast("1.21.10", "1.21.4"),
                     "1.21.10 is newer than 1.21.4");
    test_util::check(!releaseAtLeast("1.21.3", "1.21.4"), "1.21.3 is older");
    test_util::check(releaseAtLeast("26.1", "1.21.4"), "26.1 is newer");
    test_util::check(!releaseAtLeast("1.20.6", "1.21.4"), "1.20.6 is older");
    test_util::check(!releaseAtLeast("1.21.4-pre1", "1.21.4"),
                     "pre-release must not match");
    test_util::check(!releaseAtLeast("1.14 Pre-Release 5", "1.21.4"),
                     "old pre-release must not match");
  });

  suite.run(
      "the filter keeps releases from 1.21.4 on and drops older types", [] {
        const std::vector<RemoteVersion> list = {
            {"1.21.3", "release", "u"},  {"1.21.4", "release", "u"},
            {"26.1", "release", "u"},    {"26w01a", "snapshot", "u"},
            {"b1.7.3", "old_beta", "u"}, {"a1.2.6", "old_alpha", "u"}};
        const auto groups = groupVersions(list, {}, {}, VersionViewOptions{});
        test_util::check(groups.size() == 1 && groups[0].type == "release",
                         "snapshots/old types not dropped");
        test_util::check(groups.size() == 1 && groups[0].total == 2,
                         "wrong release count");
      });

  suite.run(
      "snapshots appear only when enabled, and only from the cut-off on", [] {
        const std::vector<RemoteVersion> list = {{"24w14a", "snapshot", "u"},
                                                 {"24w44a", "snapshot", "u"},
                                                 {"25w01a", "snapshot", "u"}};
        VersionViewOptions off;
        test_util::check(groupVersions(list, {}, {}, off).empty(),
                         "snapshots shown while off");
        VersionViewOptions on;
        on.filter.includeSnapshots = true;
        const auto groups = groupVersions(list, {}, {}, on);
        test_util::check(groups.size() == 1 && groups[0].total == 2,
                         "cut-off not applied");
      });

  return suite.finish();
}
