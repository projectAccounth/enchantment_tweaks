#include "core/project/project_file.hpp"
#include "core/util/debug.hpp"
#include "test_utils.hpp"

using namespace enchantment_tweaks;
using namespace enchantment_tweaks::project;

int main() {
  test_util::Suite suite;

  suite.run("an override replaces only the fields it sets", [] {
    ItemOptions base;
    base.thicknessPx = 1.0;
    base.outlineColor = "ffffff";
    ItemOverride o;
    o.outlineColor = "ff0000";
    const ItemOptions merged = applyOverride(base, o);
    test_util::check(merged.outlineColor == "ff0000", "color not overridden");
    test_util::check(merged.thicknessPx == 1.0, "unset field changed");
  });

  suite.run("overrides survive a save and load", [] {
    Project p;
    p.gameJar = "/a.jar";
    ItemOverride o;
    o.thicknessPx = 2.5;
    o.unenchantedOutline = true;
    p.overrides["minecraft:diamond_sword"] = o;
    const auto back = projectFromJson(projectToJson(p));
    test_util::check(back.project.has_value(),
                     "project did not load: " + back.error);
    if (back.project) {
      const auto it = back.project->overrides.find("minecraft:diamond_sword");
      test_util::check(it != back.project->overrides.end(), "override lost");
      if (it != back.project->overrides.end()) {
        test_util::check(it->second.thicknessPx &&
                             *it->second.thicknessPx == 2.5,
                         "thickness lost");
        test_util::check(it->second.unenchantedOutline &&
                             *it->second.unenchantedOutline,
                         "flag lost");
        test_util::check(!it->second.outlineColor, "unset color appeared");
      }
    }
  });

  suite.run("a bad color in an override is dropped, the rest is kept", [] {
    const auto r =
        projectFromJson("{\"format\":\"enchantment-tweaks-project\","
                        "\"version\":1,\"game\":{\"path\":\"/a.jar\"},"
                        "\"overrides\":{\"minecraft:bow\":{\"outlineColor\":"
                        "\"zz\",\"thicknessPx\":3}}}");
    test_util::check(r.project &&
                         r.project->overrides.count("minecraft:bow") == 1,
                     "entry missing");
    if (r.project && r.project->overrides.count("minecraft:bow")) {
      const auto &o = r.project->overrides.at("minecraft:bow");
      test_util::check(!o.outlineColor && o.thicknessPx &&
                           *o.thicknessPx == 3.0,
                       "bad field handling wrong");
    }
  });

  suite.run("the merge option is stored in the project and in overrides", [] {
    Project p;
    p.gameJar = "/a.jar";
    p.options.mergeColumns = true;
    ItemOverride o;
    o.mergeColumns = false;
    p.overrides["minecraft:wooden_sword"] = o;
    const auto back = projectFromJson(projectToJson(p));
    test_util::check(back.project && back.project->options.mergeColumns,
                     "project merge flag lost");
    test_util::check(
        back.project &&
            back.project->overrides.count("minecraft:wooden_sword") &&
            !*back.project->overrides.at("minecraft:wooden_sword").mergeColumns,
        "override merge flag lost");
    ItemOptions base;
    const ItemOptions merged = applyOverride(p.options, o);
    test_util::check(!merged.mergeColumns && base.mergeColumns == false,
                     "override did not apply");
  });

  return suite.finish();
}
