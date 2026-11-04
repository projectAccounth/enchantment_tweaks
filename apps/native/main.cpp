#include "app/application.hpp"
#include "core/util/debug.hpp"

#include <exception>
#include <filesystem>

namespace fs = std::filesystem;

using enchantment_tweaks::debug::Logger;

int main(int argc, char **argv) {
  const fs::path exeDir = fs::absolute(argv[0]).parent_path();

  enchantment_tweaks::native::ApplicationConfig config{
      .assets_directory = argc > 1 ? fs::path(argv[1]) : exeDir / "assets",
      .app_directory = exeDir / "app-data",
  };

  try {
    enchantment_tweaks::native::Application app(std::move(config));
    return app.run();
  } catch (const std::exception &error) {
    Logger::error() << "Could not run selector: " << error.what() << '\n';
    return 1;
  }
}
