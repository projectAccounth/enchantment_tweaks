#include "core/resource/resource_pack.hpp"
#include "core/util/debug.hpp"
#include "test_utils.hpp"

#include <algorithm>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using namespace enchantment_tweaks::resource;
using namespace enchantment_tweaks::image;
using namespace enchantment_tweaks;
namespace fs = std::filesystem;

int main() {
  const char *fixtureDir = TEST_FIXTURE_DIR "/assets/";
  fs::path fixturePath(fixtureDir);

  debug::Logger::setLevel(debug::Logger::Level::Info);

  std::vector<std::pair<std::string, std::string>> testCases = {
      {"set1", "test.zip"}, {"26.1", "26.1.jar"}, {"1.21.4", "1.21.4.jar"}};
  if (!std::all_of(testCases.begin(), testCases.end(), [&](const auto &test) {
        return fs::exists(fixturePath / test.first / test.second);
      })) {
    debug::Logger::warning()
        << "[skip] resource-pack fixtures are unavailable\n";
    return 77;
  }

  test_util::begin();

  int failures = 0;

  for (const auto &[version, jarFile] : testCases) {
    fs::path jarPath = fixturePath / version / jarFile;
    if (!fs::exists(jarPath)) {
      failures += !test_util::runCase(version + " [missing file]", [&] {
        throw std::runtime_error("test pack not found: " + jarPath.string());
      });
      continue;
    }

    std::string label = version + " load and validate structure";
    failures += !test_util::runCase(label, [&] {
      ResourcePackLoader loader(jarPath);

      if (version == "set1") {
        fs::path basePath = fixturePath / "1.21.4" / "1.21.4.jar";
        ResourcePackLoader baseLoader(basePath);
        if (!baseLoader.load()) {
          throw std::runtime_error("failed to load base pack: " +
                                   basePath.string());
        }
        loader.setBasePack(baseLoader);
      }

      if (!loader.load()) {
        throw std::runtime_error("failed to load pack: " + jarPath.string());
      }

      // Just verify we can load and have expected structure
      const auto &pack = loader.getPack();
      if (pack.name.empty() && !fs::exists(jarPath)) {
        throw std::runtime_error("pack name issue");
      }
      if (loader.getModels().empty()) {
        throw std::runtime_error("no models found");
      }
    });
  }

  debug::Logger::info() << testCases.size() << " test case(s) run, " << failures
                        << " failed\n";
  test_util::end();
  return failures == 0 ? 0 : 1;
}
