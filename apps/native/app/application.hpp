#pragma once

#include "model/store.h"
#include "render/driveker_rml_renderer.h"
#include "ui/selector_ui.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_video.h>
#include <filesystem>
#include <memory>
#include <string>

#ifndef ET_COMMIT_HASH
#define ET_COMMIT_HASH "dev"
#endif

namespace enchantment_tweaks::native {
namespace fs = std::filesystem;

using SelectorCallbacks = et::UiCallbacks;

struct ApplicationConfig {
  fs::path assets_directory;
  fs::path app_directory;
  std::string window_title = "Enchantment Tweaks build " ET_COMMIT_HASH;
  int width = 1280;
  int height = 860;
};

class Application final {
public:
  explicit Application(ApplicationConfig config);
  ~Application();
  Application(const Application &) = delete;
  Application &operator=(const Application &) = delete;

  bool init();
  void cleanup();

  int run();
  void requestClose() noexcept;

  et::Store &store();
  const et::Store &store() const;

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;

  struct Resource {
    SDL_Window *window;
    SDL_GLContext gl;
    std::unique_ptr<DrivekerRmlRenderer> renderer;
  };

  Resource resource_;
};

} // namespace enchantment_tweaks::native
