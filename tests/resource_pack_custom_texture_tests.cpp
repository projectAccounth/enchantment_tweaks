#include "core/resource/resource_pack.hpp"
#include "core/util/debug.hpp"
#include "core/util/image.hpp"
#include "test_utils.hpp"

#include <algorithm>
#include <cstdlib>
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

  int failures = 0;
  test_util::begin();

  for (const auto &[version, jarFile] : testCases) {
    fs::path jarPath = fixturePath / version / jarFile;
    if (!fs::exists(jarPath)) {
      failures += !test_util::runCase(version + " [missing file]", [&] {
        throw std::runtime_error("test pack not found: " + jarPath.string());
      });
      continue;
    }

    std::string label = version + " texture image operations";
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

      Image customTex(16, 16);
      auto &p = customTex.pixels();
      for (size_t i = 0; i < p.size(); i += 4) {
        p[i] = 255;
        p[i + 1] = 0;
        p[i + 2] = 0;
        p[i + 3] = 255;
      }

      fs::path tempDir =
          fs::temp_directory_path() /
          ("rp_test_ct_" + version + "_" + std::to_string(std::rand()));
      fs::create_directories(tempDir);
      fs::path customOutPath = tempDir / "custom_test.png";
      customTex.save(customOutPath.string());
      if (!fs::exists(customOutPath)) {
        fs::remove_all(tempDir);
        throw std::runtime_error("Texture file not created");
      }

      Image loaded = Image::load(customOutPath.string());
      if (loaded.width() != 16 || loaded.height() != 16) {
        fs::remove_all(tempDir);
        throw std::runtime_error("Loaded texture has wrong dimensions");
      }
      fs::remove_all(tempDir);
    });
  }

  debug::Logger::info() << testCases.size() << " test case(s) run, " << failures
                        << " failed\n";
  test_util::end();
  return failures == 0 ? 0 : 1;
}
