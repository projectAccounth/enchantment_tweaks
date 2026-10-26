#include "core/types/items/item_model.hpp"
#include "core/util/debug.hpp"
#include "core/util/json/utils.hpp"
#include "libraries/nlohmann/json.hpp"
#include "test_utils.hpp"

#include <cstdlib>
#include <optional>
#include <string>

using namespace enchantment_tweaks;
using namespace enchantment_tweaks::debug;
using namespace enchantment_tweaks::item;
using nlohmann::json;

int main() {
  size_t passed = 0;
  size_t total = 0;
  auto run = [&](const std::string &name, auto &&fn) {
    ++total;
    if (test_util::runCase(name, std::forward<decltype(fn)>(fn))) {
      ++passed;
    }
  };

  test_util::begin();

  run("readOptional returns nullopt when missing", [] {
    json j = json::object();
    auto v = util::readOptional<int>(j, "missing");
    test_util::check(!v.has_value(), "should be nullopt");
  });

  run("readOptional returns value when present", [] {
    json j = {{"x", 42}};
    auto v = util::readOptional<int>(j, "x");
    test_util::check(v.has_value(), "should have value");
    test_util::check(*v == 42, "value mismatch");
  });

  run("writeOptional writes when has value", [] {
    json j = json::object();
    std::optional<std::string> s = std::string("hello");
    util::writeOptional(j, "name", s);
    test_util::check(j.contains("name"), "should contain name");
    test_util::check(j["name"] == "hello", "value mismatch");
  });

  run("writeOptional omits when nullopt", [] {
    json j = json::object();
    std::optional<std::string> s = std::nullopt;
    util::writeOptional(j, "name", s);
    test_util::check(!j.contains("name"), "should not contain name");
  });

  run("EnumTable roundtrip", [] {
    auto bt = booleanPropertyTypeFromString("minecraft:broken");
    test_util::check(toString(bt) == "minecraft:broken", "roundtrip mismatch");
  });

  debug::Logger::info() << "[summary] " << passed << "/" << total
                        << " tests passed\n";
  test_util::end();
  return passed == total ? 0 : 1;
}
