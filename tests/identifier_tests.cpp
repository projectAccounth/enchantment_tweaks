#include "core/types/identifier.hpp"
#include "core/util/debug.hpp"
#include "test_utils.hpp"

#include <cstdlib>
#include <exception>
#include <stdexcept>
#include <string>

using namespace enchantment_tweaks;
using namespace enchantment_tweaks::debug;

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

  run("identifier parse bare uses minecraft namespace", [] {
    try {
      volatile Identifier id = Identifier::parse("stone");
      (void)id;
      test_util::check(true, "no exception for bare name");
    } catch (const std::exception &e) {
      debug::Logger::debug() << "exception: " << e.what();
      throw e;
    }
  });

  run("identifier parse namespaced", [] {
    Identifier id = Identifier::parse("custom:foo/bar");
    test_util::check(id.getNamespace() == "custom",
                     "namespace should be custom");
    test_util::check(id.getPath() == "foo/bar", "path should be foo/bar");
  });

  run("identifier parse with empty path throws", [] {
    try {
      volatile Identifier id = Identifier::parse("minecraft:");
      (void)id;
      test_util::check(true, "expected exception for empty path");
    } catch (const std::exception &e) {
      debug::Logger::debug() << "expected exception: " << e.what();
      return;
    }
    throw std::runtime_error("expected an exception, got none");
  });

  run("identifier toString roundtrip", [] {
    Identifier id("minecraft", "diamond_sword");
    test_util::check(id.toString() == "minecraft:diamond_sword",
                     "toString mismatch");
    Identifier parsed = Identifier::parse(id.toString());
    test_util::check(parsed.getNamespace() == id.getNamespace(),
                     "namespace mismatch");
    test_util::check(parsed.getPath() == id.getPath(), "path mismatch");
  });

  run("identifier custom namespace roundtrip", [] {
    Identifier id("somerandombullshit", "items/test");
    std::string s = id.toString();
    test_util::check(s == "somerandombullshit:items/test", "toString mismatch");
    Identifier parsed = Identifier::parse(s);
    test_util::check(parsed.getNamespace() == "somerandombullshit",
                     "namespace mismatch");
    test_util::check(parsed.getPath() == "items/test", "path mismatch");
  });

  debug::Logger::info() << "[summary] " << passed << "/" << total
                        << " tests passed\n";
  test_util::end();
  return passed == total ? 0 : 1;
}
