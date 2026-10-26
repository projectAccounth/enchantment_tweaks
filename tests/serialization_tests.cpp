#include "core/resource/resource_pack.hpp"
#include "core/types/items/item_model.hpp"
#include "core/types/items/item_model_definition.hpp"
#include "core/types/items/special_model.hpp"
#include "core/types/mesh/types.hpp"
#include "core/util/debug.hpp"
#include "libraries/nlohmann/json.hpp"
#include "test_utils.hpp"

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using namespace enchantment_tweaks;
using namespace enchantment_tweaks::item;
using namespace enchantment_tweaks::mesh;
using nlohmann::json;

namespace {

namespace fs = std::filesystem;

std::string fixtureLabel(const fs::path &path, const fs::path &root) {
  return path.lexically_relative(root).generic_string();
}

json canonicalModelFixture(json fixture) {
  if (!fixture.contains("textures") || !fixture["textures"].is_object()) {
    return fixture;
  }

  for (auto it = fixture["textures"].begin(); it != fixture["textures"].end();
       ++it) {
    if (!it.value().is_string()) {
      continue;
    }
    std::string texture = it.value().get<std::string>();
    if (!texture.empty() && texture.front() != '#' &&
        texture.find(':') == std::string::npos) {
      texture.insert(0, "minecraft:");
      it.value() = std::move(texture);
    }
  }
  return fixture;
}

void testMeshModel(const json &fixture, const std::string &label) {
  Model model;
  model.deserialize(fixture);
  const json serialized = model.serialize();
  test_util::checkJsonEqual(canonicalModelFixture(fixture),
                            canonicalModelFixture(serialized),
                            label + " serialize");

  Model roundTripped;
  roundTripped.deserialize(serialized);
  test_util::checkJsonEqual(canonicalModelFixture(serialized),
                            canonicalModelFixture(roundTripped.serialize()),
                            label + " second round trip");
}

void testItemDefinition(const json &fixture, const std::string &label) {
  ItemModelDefinition definition;
  definition.deserialize(fixture);
  const json serialized = definition.serialize();
  test_util::checkJsonEqual(fixture, serialized, label + " serialize");

  ItemModelDefinition roundTripped;
  roundTripped.deserialize(serialized);
  test_util::checkJsonEqual(serialized, roundTripped.serialize(),
                            label + " second round trip");
}

void testItemModel(const json &fixture, const std::string &label) {
  std::unique_ptr<ItemModel> model = ItemModel::create(fixture);
  const json serialized = model->serialize();
  test_util::checkJsonEqual(fixture, serialized, label + " serialize");

  std::unique_ptr<ItemModel> roundTripped = ItemModel::create(serialized);
  test_util::checkJsonEqual(serialized, roundTripped->serialize(),
                            label + " second round trip");
}

void testSpecialModel(const json &fixture, const std::string &label) {
  std::unique_ptr<SpecialModel> model = SpecialModel::create(fixture);
  const json serialized = model->serialize();
  test_util::checkJsonEqual(fixture, serialized, label + " serialize");

  std::unique_ptr<SpecialModel> roundTripped = SpecialModel::create(serialized);
  test_util::checkJsonEqual(serialized, roundTripped->serialize(),
                            label + " second round trip");
}
int runFixture(const fs::path &path, const fs::path &root,
               const std::string &suite, size_t &caseCount) {
  const std::string label = fixtureLabel(path, root);
  json fixture;
  if (!test_util::runCase(label + " [parse]",
                          [&] { fixture = test_util::readJson(path); })) {
    return 1;
  }

  if (suite == "serialize_model") {
    ++caseCount;
    return test_util::runCase(label, [&] { testMeshModel(fixture, label); })
               ? 0
               : 1;
  }

  if (suite != "serialize_item") {
    ++caseCount;
    test_util::runCase(label, [&] {
      throw std::runtime_error("No serializer registered for suite '" + suite +
                               "'");
    });
    return 1;
  }

  if (fixture.is_array()) {
    if (fixture.empty()) {
      ++caseCount;
      test_util::runCase(label, [] {
        throw std::runtime_error("special-model fixture array is empty");
      });
      return 1;
    }

    int failures = 0;
    for (size_t index = 0; index < fixture.size(); ++index) {
      const json &special = fixture[index];
      const std::string type =
          special.contains("type") && special["type"].is_string()
              ? special["type"].get<std::string>()
              : "unknown-special-model";
      std::string caseLabel = label;
      caseLabel += "[";
      caseLabel += std::to_string(index);
      caseLabel += "]: ";
      caseLabel += type;
      ++caseCount;
      failures += !test_util::runCase(
          caseLabel, [&] { testSpecialModel(special, caseLabel); });
    }
    return failures;
  }

  if (fixture.is_object() && fixture.contains("model") &&
      !fixture.contains("type")) {
    ++caseCount;
    return test_util::runCase(label,
                              [&] { testItemDefinition(fixture, label); })
               ? 0
               : 1;
  }

  ++caseCount;
  return test_util::runCase(label, [&] { testItemModel(fixture, label); }) ? 0
                                                                           : 1;
}

int discoverAndRun() {
  const fs::path fixtureRoot(TEST_FIXTURE_DIR);
  if (!fs::is_directory(fixtureRoot)) {
    debug::Logger::error() << "[fail] fixture root is not a directory: "
                           << fixtureRoot << '\n';
    return 1;
  }

  test_util::begin();

  std::vector<std::pair<std::string, fs::path>> fixtures;
  for (const auto &suiteEntry : fs::directory_iterator(fixtureRoot)) {
    if (!suiteEntry.is_directory()) {
      continue;
    }
    const std::string suite = suiteEntry.path().filename().string();
    if (!suite.starts_with("serialize_")) {
      continue;
    }

    for (const auto &entry :
         fs::recursive_directory_iterator(suiteEntry.path())) {
      if (entry.is_regular_file() && entry.path().extension() == ".json") {
        fixtures.emplace_back(suite, entry.path());
      }
    }
  }

  std::ranges::sort(fixtures, {}, [&fixtureRoot](const auto &fixture) {
    return fixtureLabel(fixture.second, fixtureRoot);
  });

  if (fixtures.empty()) {
    debug::Logger::error()
        << "[FAIL] no JSON fixtures found in serialize_* directories under "
        << fixtureRoot << '\n';
    return 1;
  }

  int failures = 0;
  size_t caseCount = 0;
  for (const auto &[suite, path] : fixtures) {
    failures += runFixture(path, fixtureRoot, suite, caseCount);
  }

  debug::Logger::info() << fixtures.size() << " fixture file(s) discovered; "
                        << caseCount << " test case(s) run, " << failures
                        << " failed\n";
  test_util::end();
  return failures == 0 ? 0 : 1;
}

} // namespace

int main() { return discoverAndRun(); }
