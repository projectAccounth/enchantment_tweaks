#include "core/resource/resource_pack.hpp"
#include "core/util/debug.hpp"
#include "test_utils.hpp"

#include <filesystem>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using namespace enchantment_tweaks::resource;
using namespace enchantment_tweaks;
namespace fs = std::filesystem;

int main() {
  const char *fixtureDir = TEST_FIXTURE_DIR "/assets/";
  fs::path fixturePath(fixtureDir);

  debug::Logger::setLevel(debug::Logger::Level::Info);

  std::vector<std::pair<std::string, std::string>> testCases = {
      {"set1", "test.zip"}, {"26.1", "26.1.jar"}, {"1.21.4", "1.21.4.jar"}};

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

    std::string label = version + " load from pack";
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

      auto models = loader.getModels();
      auto textures = loader.getTextures();
      auto itemDefs = loader.getDef();

      if (models.empty()) {
        throw std::runtime_error("no models found in " + version);
      }
      if (textures.empty()) {
        throw std::runtime_error("no textures found in " + version);
      }
      if (itemDefs.empty()) {
        throw std::runtime_error("no item definitions found in " + version);
      }
    });
  }

  debug::Logger::info() << testCases.size() << " test case(s) run, " << failures
                        << " failed\n";
  test_util::end();
  return failures == 0 ? 0 : 1;
}
