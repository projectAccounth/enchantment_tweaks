#pragma once

#include "model/store.h"
#include "selector/model/types.h"
#include "selector/render/driveker_rml_renderer.h"
#include "ui/selector_ui.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_video.h>
#include <filesystem>
#include <memory>
#include <string>

namespace enchantment_tweaks::native {

using SelectorCatalog = et::Catalog;
using SelectorCallbacks = et::UiCallbacks;

struct SelectorApplicationConfig {
  std::filesystem::path assets_directory;
  std::filesystem::path state_file;
  std::filesystem::path pack_directory;
  std::string window_title = "Enchantment Tweaks";
  int width = 1280;
  int height = 860;
};

class SelectorApplication final {
public:
  SelectorApplication(SelectorCatalog catalog, SelectorApplicationConfig config,
                      SelectorCallbacks callbacks = {});
  ~SelectorApplication();
  SelectorApplication(const SelectorApplication &) = delete;
  SelectorApplication &operator=(const SelectorApplication &) = delete;

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
