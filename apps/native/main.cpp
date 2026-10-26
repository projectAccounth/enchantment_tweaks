#include "core/util/debug.hpp"
#include "selector/model/json.h"
#include "selector/selector_application.hpp"

#include <exception>
#include <filesystem>
#include <utility>

using enchantment_tweaks::debug::Logger;
using namespace enchantment_tweaks;

int main(int argc, char **argv) {
  namespace fs = std::filesystem;
  const fs::path executableDirectory = fs::absolute(argv[0]).parent_path();
  const fs::path assets =
      argc > 1 ? argv[1] : executableDirectory / "selector-assets";
  const fs::path state =
      argc > 2 ? argv[2] : executableDirectory / "selector-state.json";
  const auto catalog = et::parseCatalog(
      et::readFile(assets / "catalog.sample.json").value_or(""));
  if (!catalog) {
    Logger::error() << "Could not parse selector catalog: "
                    << (assets / "catalog.sample.json") << '\n';
    return 1;
  }

  try {
    native::SelectorApplicationConfig config{
        .assets_directory = assets,
        .state_file = state,
        .pack_directory = executableDirectory / "packs",
    };
    native::SelectorApplication app(*catalog, std::move(config));
    return app.run();
  } catch (const std::exception &error) {
    Logger::error() << "Could not run selector: " << error.what() << '\n';
    return 1;
  }
}
