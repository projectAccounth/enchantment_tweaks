#include "selector_application.hpp"
#include "RmlUi/Config/Config.h"
#include "RmlUi/Core/Context.h"
#include "RmlUi/Core/Core.h"
#include "RmlUi/Core/Log.h"
#include "RmlUi/Core/StyleTypes.h"
#include "core/util/debug.hpp"
#include "model/json.h"
#include "render/driveker_rml_renderer.h"
#include "selector/model/store.h"
#include "selector/model/types.h"
#include "selector/ui/selector_ui.h"

#include <RmlUi/Core.h>
#include <RmlUi_Platform_SDL.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_stdinc.h>
#include <SDL3/SDL_timer.h>
#include <SDL3/SDL_video.h>
#include <driveker/core/render_system.hpp>
#include <driveker/gl/gl_device_backend.hpp>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

namespace enchantment_tweaks::native {

using enchantment_tweaks::debug::Logger;

namespace {
class AppSystem final : public SystemInterface_SDL {
public:
  using SystemInterface_SDL::SystemInterface_SDL;
  bool LogMessage(Rml::Log::Type type, const Rml::String &message) override {
    if (type == Rml::Log::LT_ERROR || type == Rml::Log::LT_WARNING)
      debug::Logger::error() << "RmlUi: " << message << '\n';
    return true;
  }
  void JoinPath(Rml::String &out, const Rml::String &document,
                const Rml::String &path) override {
    if (et::IsExistingAbsoluteFile(path)) {
      out = path;
      return;
    }
    SystemInterface_SDL::JoinPath(out, document, path);
  }
};

bool loadFonts(const std::filesystem::path &assets) {
  for (int weight : {400, 500, 600, 700})
    if (!Rml::LoadFontFace(
            (assets / "fonts" / ("Inter-" + std::to_string(weight) + ".ttf"))
                .string(),
            false, static_cast<Rml::Style::FontWeight>(weight)))
      return false;
  return true;
}
} // namespace

struct SelectorApplication::Impl {
  Impl(et::Catalog catalog, SelectorApplicationConfig config,
       et::UiCallbacks callbacks)
      : catalog_(std::move(catalog)), config_(std::move(config)),
        callbacks_(std::move(callbacks)),
        store_(catalog_, et::parsePersisted(
                             et::readFile(config_.state_file).value_or(""))) {
    if (config_.assets_directory.empty())
      throw std::invalid_argument(
          "SelectorApplication requires assets_directory");

    if (!callbacks_.persist && !config_.state_file.empty())
      callbacks_.persist =
          [path = config_.state_file](const et::PersistedState &state) {
            et::writeFile(path, et::toJson(state));
          };

    if (!callbacks_.save_pack) {
      const auto directory = config_.pack_directory.empty()
                                 ? config_.state_file.parent_path() / "packs"
                                 : config_.pack_directory;

      callbacks_.save_pack = [directory](const et::SavePackRequest &request) {
        const auto destination = directory / request.suggested_name;
        bool saved = true;

        for (const auto &file : request.files)
          saved &= et::writeFile(destination / file.path, file.text);
        return et::SavePackResult{
            .saved = saved,
            .path = destination.string(),
            .error = saved ? "" : "Could not write pack files"};
      };
    }
  }

  et::Catalog catalog_;
  SelectorApplicationConfig config_;
  et::UiCallbacks callbacks_;
  et::Store store_;
  bool running_ = false;
};

SelectorApplication::SelectorApplication(et::Catalog catalog,
                                         SelectorApplicationConfig config,
                                         et::UiCallbacks callbacks)
    : impl_(std::make_unique<Impl>(std::move(catalog), std::move(config),
                                   std::move(callbacks))) {}
SelectorApplication::~SelectorApplication() = default;
et::Store &SelectorApplication::store() { return impl_->store_; }
const et::Store &SelectorApplication::store() const { return impl_->store_; }
void SelectorApplication::requestClose() noexcept { impl_->running_ = false; }

bool SelectorApplication::init() {
  if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
    return false;
  }

  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

  resource_.window = SDL_CreateWindow(
      impl_->config_.window_title.c_str(), impl_->config_.width,
      impl_->config_.height,
      SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
  if (!resource_.window) {
    SDL_Quit();
    return false;
  }

  auto *window = resource_.window;

  resource_.gl = SDL_GL_CreateContext(window);
  if (!resource_.gl) {
    SDL_DestroyWindow(window);
    SDL_Quit();
    return false;
  }
  SDL_GL_SetSwapInterval(1);

  int width = 0, height = 0;
  SDL_GetWindowSizeInPixels(window, &width, &height);

  driveker::initializeGLRenderSystem();

  resource_.renderer = std::make_unique<DrivekerRmlRenderer>(width, height);

  return true;
}

void SelectorApplication::cleanup() {
  Rml::Shutdown();
  resource_.renderer.reset();
  driveker::RenderSystem::shutdown();
  SDL_GL_DestroyContext(resource_.gl);
  SDL_DestroyWindow(resource_.window);
  SDL_Quit();
}

int SelectorApplication::run() {

  if (!init()) {
    throw std::runtime_error("Initialization of the app failed");
  }

  auto system = std::make_unique<AppSystem>(resource_.window);
  auto &renderer = resource_.renderer;

  int width = 0, height = 0;
  SDL_GetWindowSizeInPixels(resource_.window, &width, &height);

  Rml::SetSystemInterface(system.get());
  Rml::SetRenderInterface(renderer.get());

  TextInputMethodEditor_SDL textInput;
  Rml::SetTextInputHandler(&textInput);

  if (!Rml::Initialise() || !loadFonts(impl_->config_.assets_directory)) {
    system.reset();
    cleanup();
    throw std::runtime_error("Could not initialise RmlUi selector");
  }

  Rml::Context *context = Rml::CreateContext("selector", {width, height});

  auto ui = std::make_unique<et::SelectorUi>(
      context, impl_->store_, impl_->callbacks_,
      impl_->config_.assets_directory.string());

  if (!context || !ui->Initialise()) {
    ui.reset();
    system.reset();
    cleanup();
    throw std::runtime_error("Could not load RmlUi selector documents");
  }

  Uint64 previous = SDL_GetTicks();
  impl_->running_ = true;
  bool redraw = true;

  while (impl_->running_) {
    SDL_Event event;
    const double next =
        std::min(context->GetNextUpdateDelay(), ui->NextUpdateDelay());
    int timeout = 0;

    if (redraw) {
      timeout = 0;
    } else if (std::isinf(next)) {
      timeout = -1;
    } else {
      timeout = std::max(0, static_cast<int>(std::ceil(next * 1000)));
    }

    bool hasEvent = SDL_WaitEventTimeout(&event, timeout),
         receivedEvent = hasEvent;

    while (hasEvent) {

      if (event.type == SDL_EVENT_QUIT)
        impl_->running_ = false;

      else if (event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {
        width = event.window.data1;
        height = event.window.data2;

        context->SetDimensions({width, height});
        renderer->SetViewport(width, height);

      } else if (event.type == SDL_EVENT_TEXT_EDITING)
        textInput.HandleEdit(event.edit);

      RmlSDL::InputEventHandler(context, resource_.window, event);
      hasEvent = SDL_PollEvent(&event);

      receivedEvent |= hasEvent;
    }

    if (!impl_->running_) break;
    if (!redraw && !receivedEvent && std::isinf(next)) continue;

    const Uint64 now = SDL_GetTicks();

    ui->Update(static_cast<double>(now - previous) / 1000.0);
    previous = now;

    context->Update();
    renderer->BeginFrame();

    context->Render();
    renderer->EndFrame();

    SDL_GL_SwapWindow(resource_.window);
    redraw = false;
  }

  ui.reset();
  system.reset();
  cleanup();

  return 0;
}
} // namespace enchantment_tweaks::native
