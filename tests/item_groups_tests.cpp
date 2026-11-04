#include "core/api/item_groups.hpp"
#include "core/util/debug.hpp"
#include "test_utils.hpp"

using namespace enchantment_tweaks;
using namespace enchantment_tweaks::catalog;

int main() {
  test_util::Suite suite;

  suite.run("group mode round-trips and unknown text means off", [] {
    test_util::check(groupModeFromString("prefix") == GroupMode::Prefix,
                     "prefix");
    test_util::check(groupModeFromString("suffix") == GroupMode::Suffix,
                     "suffix");
    test_util::check(groupModeFromString("whatever") == GroupMode::None,
                     "unknown should be none");
    test_util::check(groupModeToString(GroupMode::Suffix) == "suffix",
                     "to string");
  });

  suite.run("grouping off gives no label", [] {
    test_util::check(groupLabelFor("Iron Sword", GroupMode::None).empty(),
                     "label while off");
  });

  suite.run("prefix groups use the first word or words", [] {
    test_util::check(groupLabelFor("Iron Sword", GroupMode::Prefix) == "Iron",
                     "iron");
    test_util::check(groupLabelFor("Dark Oak Sign", GroupMode::Prefix) ==
                         "Dark Oak",
                     "longest prefix wins");
    test_util::check(groupLabelFor("Oak Sign", GroupMode::Prefix) == "Oak",
                     "oak");
    test_util::check(groupLabelFor("Ironwood Plank", GroupMode::Prefix) ==
                         "Other",
                     "a keyword must end at a word boundary");
  });

  suite.run("suffix groups match whole words at the end", [] {
    test_util::check(groupLabelFor("Iron Pickaxe", GroupMode::Suffix) ==
                         "Pickaxe",
                     "pickaxe, not axe");
    test_util::check(groupLabelFor("Iron Axe", GroupMode::Suffix) == "Axe",
                     "axe");
    test_util::check(groupLabelFor("Glass Pane", GroupMode::Suffix) ==
                         "Glass Pane",
                     "glass pane");
    test_util::check(groupLabelFor("Oak Sapling", GroupMode::Suffix) ==
                         "Sapling",
                     "sapling");
    test_util::check(groupLabelFor("Pickaxe Helper", GroupMode::Suffix) ==
                         "Other",
                     "a keyword in the middle is not a suffix");
  });

  suite.run("names that match nothing go to Other", [] {
    test_util::check(groupLabelFor("Apple", GroupMode::Prefix) == "Other",
                     "apple prefix");
    test_util::check(groupLabelFor("Apple", GroupMode::Suffix) == "Other",
                     "apple suffix");
  });

  return suite.finish();
}
