#include "application.hpp"
#include "RmlUi/Config/Config.h"
#include "RmlUi/Core/Context.h"
#include "RmlUi/Core/Core.h"
#include "RmlUi/Core/Log.h"
#include "RmlUi/Core/StyleTypes.h"
#include "app/app_session.hpp"
#include "app/download_manager.hpp"
#include "app/file_menu.hpp"
#include "app/minecraft_downloader.hpp"
#include "app/model/store.h"
#include "app/model/types.h"
#include "app/ui/selector_ui.h"
#include "core/project/minecraft_download.hpp"
#include "core/project/project_file.hpp"
#include "core/project/version_list.hpp"
#include "core/util/debug.hpp"
#include "file_dialogs.hpp"

#include "render/driveker_rml_renderer.h"

#include <RmlUi/Core.h>
#include <RmlUi_Platform_SDL.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_keycode.h>
#include <SDL3/SDL_stdinc.h>
#include <SDL3/SDL_timer.h>
#include <SDL3/SDL_video.h>
#include <cstdint>
#include <driveker/core/render_system.hpp>
#include <driveker/gl/gl_device_backend.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <deque>
#include <fstream>
#include <functional>
#include <future>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <system_error>
#include <vector>
#if defined(__linux__)
#include <unistd.h>
#endif
#include <cmath>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

namespace fs = std::filesystem;

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

bool loadFonts(const fs::path &assets) {
  for (int weight : {400, 500, 600, 700})
    if (!Rml::LoadFontFace(
            (assets / "fonts" / ("Inter-" + std::to_string(weight) + ".ttf"))
                .string(),
            false, static_cast<Rml::Style::FontWeight>(weight)))
      return false;
  return true;
}
} // namespace

namespace {

et::Catalog emptyCatalog() { return et::Catalog{}; }

} // namespace

struct Application::Impl {
  static void openLog(const fs::path &appDirectory) {
    const auto dir = appDirectory / "logs";
    const auto file = dir / "enchantment-tweaks.log";
    std::error_code ec;
    fs::create_directories(dir, ec);
    // Rotate logs above 2 MB to keep the current log bounded.
    if (fs::file_size(file, ec) > uintmax_t{2} * 1024 * 1024 && !ec) {
      fs::rename(file, dir / "enchantment-tweaks.previous.log", ec);
    }
    if (!debug::Logger::setLogFile(file.string(), /*append=*/true)) {
      debug::Logger::error() << "could not open the log file " << file;
    } else {
      debug::Logger::info() << "log file: " << file;
    }
  }

  explicit Impl(ApplicationConfig config)
      : config_((openLog(config.app_directory), std::move(config))),
        session_(config_.app_directory, config_.assets_directory),
        downloads_(MinecraftDownloader(config_.app_directory)),
        store_(emptyCatalog(), std::nullopt) {
    MinecraftDownloader::initialiseNetwork();
    if (config_.assets_directory.empty())
      throw std::invalid_argument("Application requires assets_directory");

    callbacks_.persist = [this](const et::PersistedState &state) {
      // Session synchronization is not a user change.
      if (syncing_) return;
      session_.setSelection(state.created.value_or(std::vector<std::string>{}));
      if (state.sort) session_.setSort(et::toString(*state.sort));
      if (state.versions && session_.isPackProject()) {
        std::vector<std::string> wanted = *state.versions;
        std::vector<std::string> current = session_.supportedVersions();
        std::ranges::sort(wanted);
        std::ranges::sort(current);
        if (wanted != current) {
          AppSession::LoadRequest request;
          const std::string error =
              session_.prepareSupported(*state.versions, request);
          if (error.empty()) {
            beginLoad(request);
          } else {
            showError(error);
          }
        }
        pendingStoreSync_ =
            true; // the store is changing right now: resync on the next poll
        return;
      }
      refreshMenu();
    };

    callbacks_.save_pack =
        [this](const et::SavePackRequest &request) -> et::SavePackResult {
      if (!session_.hasProject()) return et::SavePackResult{};
      const auto target = dialogs_.saveFile({"zip"}, request.suggested_name,
                                            config_.app_directory);
      if (!target)
        return et::SavePackResult{.saved = false, .path = {}, .error = {}};
      std::string fileName = target->filename().string();
      if (target->extension() != ".zip") fileName += ".zip";
      const auto outcome = session_.exportPack(target->parent_path(), fileName);
      if (outcome.saved) refreshMenu();
      return et::SavePackResult{.saved = outcome.saved,
                                .path = outcome.path.string(),
                                .error = outcome.error};
    };

    callbacks_.menu_action = [this](const std::string &action,
                                    const std::string &arg) {
      handleMenu(action, arg);
    };

    callbacks_.app_settings_changed =
        [this](const et::AppSettingsValues &values) {
          auto next = values;
          next.groupMode =
              session_.appSettings()
                  .groupMode; // the grid's grouping is not in this dialog
          const auto error = session_.setAppSettings(next);
          if (ui_) {
            ui_->SetAppSettings(session_.appSettings());
            ui_->ShowToast(error.empty() ? "Application settings saved"
                                         : "Could not save settings: " + error);
          }
        };

    callbacks_.item_override_changed =
        [this](const std::vector<std::string> &keys,
               const et::ItemOverrideValues &values, bool reset) {
          if (reset) {
            session_.clearOverrides(keys);
          } else {
            project::ItemOverride o;
            o.thicknessPx = values.thicknessPx;
            o.outlineSizePx = values.outlineSizePx;
            o.outlineColor = values.outlineColor;
            o.unenchantedOutline = values.unenchantedOutline;
            o.unenchantedUnshaded = values.unenchantedUnshaded;
            o.mergeColumns = values.mergeColumns;
            session_.setOverride(keys, o);
          }
          refreshMenu();
        };

    callbacks_.item_options_changed =
        [this](const et::ItemOptionsValues &values) {
          const auto error = session_.setOptions(values);
          if (ui_ && !error.empty()) ui_->ShowToast(error, 6);
          refreshMenu();
        };
  }

  // Prevent session updates from being read back as user changes.
  void refreshStore() {
    syncing_ = true;
    store_.replaceCatalog(session_.uiCatalog());
    if (session_.isPackProject()) {
      store_.setVersions(session_.supportedVersions());
    } else {
      store_.setVersions(
          session_.gameVersion().empty()
              ? std::vector<std::string>{}
              : std::vector<std::string>{session_.gameVersion()});
    }
    store_.setCreated(session_.selection());
    if (const auto sort = et::sortKeyFromString(session_.sort()))
      store_.setSort(*sort);
    syncing_ = false;
    refreshMenu();
  }

  void refreshMenu() {
    if (!ui_) return;
    std::string status = session_.statusLine();
    size_t active = 0;
    for (const auto &entry : downloads_.snapshot()) {
      if (entry.state == DownloadState::Queued ||
          entry.state == DownloadState::Running)
        ++active;
    }
    if (active > 0) status += "  ·  Downloading " + std::to_string(active);
    ui_->SetMenuData(buildFileMenu(session_, session_.installedGames()),
                     status);
    ui_->SetProjectState(session_.hasProject());
  }

  // Use the newest installed version as the pack project's reference.
  std::optional<std::pair<fs::path, std::string>> referenceGame() {
    const auto games = session_.installedGames();
    if (!games.empty())
      return std::make_pair(games.front().jar, games.front().id);
    if (const auto jar =
            dialogs_.openFile({"jar", "zip"}, config_.app_directory)) {
      return std::make_pair(*jar, jar->stem().string());
    }
    return std::nullopt;
  }

  void newPackProject(const std::optional<fs::path> &pack) {
    if (!pack) return;
    auto games = session_.installedGames();
    if (games.empty()) {
      showError("Install or download a Minecraft version first (File > "
                "Download Minecraft version).");
      return;
    }
    // Include the newest installed version by default.
    games.resize(1);
    AppSession::LoadRequest request;
    const std::string error = session_.prepareNewPack(*pack, games, request);
    if (error.empty()) {
      beginLoad(request);
    } else {
      showError(error);
    }
  }

  bool isManaged(const fs::path &jar) const {
    const std::string root = downloads_.downloader().versionsRoot().string();
    return jar.string().starts_with(root);
  }

  // Update the dialog with manifest, installation, and download state.
  void refreshRemoteView() {
    if (!ui_) return;
    std::set<std::string> installed, managed;
    for (const auto &game : session_.installedGames()) {
      installed.insert(game.id);
      if (isManaged(game.jar)) managed.insert(game.id);
    }
    project::VersionViewOptions options;
    options.query = remoteQuery_;
    options.openTypes = openTypes_;
    options.rowLimit = 200;
    options.filter.minRelease =
        "1.21.4"; // the project only supports 1.21.4 and newer
    options.filter.includeSnapshots = includeSnapshots_;
    const auto groups =
        project::groupVersions(manifest_.versions, installed, managed, options);

    std::map<std::string, DownloadStatus> live;
    for (const auto &status : downloads_.snapshot())
      live[status.id] = status;

    std::vector<et::SelectorUi::RemoteGroupChoice> choices;
    for (const auto &group : groups) {
      et::SelectorUi::RemoteGroupChoice g;
      g.type = group.type;
      g.total = group.total;
      g.matches = group.matches;
      g.open = group.open;
      g.truncated = group.truncated;
      for (const auto &row : group.rows) {
        et::SelectorUi::RemoteRowChoice r;
        r.id = row.id;
        r.type = row.type;
        r.button =
            row.managed ? "uninstall" : (row.installed ? "none" : "download");
        r.installed = row.installed;
        const auto status = live.find(row.id);
        if (status != live.end()) {
          switch (status->second.state) {
          case DownloadState::Queued:
            r.button = "cancel";
            r.showBar = true;
            r.status = "Queued";
            r.installed = row.installed;
            break;
          case DownloadState::Running:
            r.button = "cancel";
            r.showBar = true;
            r.fraction = status->second.fraction;
            r.status = status->second.message;
            break;
          case DownloadState::Failed:
            r.button = "retry";
            r.status = status->second.message.substr(0, 60);
            break;
          case DownloadState::Done:
            r.button = "uninstall";
            r.installed = true;
            r.status.clear();
            break;
          case DownloadState::Canceled:
            break;
          }
        }
        g.rows.push_back(r);
      }
      choices.push_back(std::move(g));
    }

    size_t shown = 0;
    for (const auto &group : groups)
      shown += group.total;
    std::string summary = std::to_string(shown) + " versions from 1.21.4" +
                          (includeSnapshots_ ? " and newer snapshots" : " on");
    size_t active = 0;
    for (const auto &status : live) {
      if (status.second.state == DownloadState::Queued ||
          status.second.state == DownloadState::Running)
        ++active;
    }
    if (active > 0)
      summary += "  ·  " + std::to_string(active) + " downloading";
    ui_->SetRemoteView(choices, remoteNote_, remoteQuery_, summary,
                       includeSnapshots_);
  }

  void openDownloadDialog() {
    if (!ui_) return;
    if (manifest_.versions.empty() && !manifestRequested_) {
      downloads_.requestManifest();
      manifestRequested_ = true;
    }
    if (manifest_.versions.empty()) remoteNote_ = "Loading the version list...";
    refreshRemoteView();
    ui_->OpenDownloadDialog();
  }

  // Applies completed manifest/download work and reports whether the UI changed.
  bool poll() {
    bool changed = false;
    if (pendingStoreSync_) {
      pendingStoreSync_ = false;
      refreshStore();
      changed = true;
    }
    if (pollLoad()) changed = true;
    if (auto loaded = downloads_.takeManifest()) {
      manifestRequested_ = false;
      if (loaded->versions.empty()) {
        remoteNote_ = loaded->error;
        showError("Could not load the Minecraft version list: " +
                  loaded->error);
        debug::Logger::error() << "version list failed: " << loaded->error;
      } else {
        manifest_ = *loaded;
        remoteNote_ = manifest_.error; // a note, when the saved list is used
        manifest_.error.clear();
        debug::Logger::info()
            << "version list loaded: " << manifest_.versions.size()
            << " versions";
      }

      refreshRemoteView();
      changed = true;
    }

    const auto statuses = downloads_.snapshot();
    if (statuses.size() != lastStatuses_.size() ||
        !std::equal(statuses.begin(), statuses.end(), lastStatuses_.begin(),
                    [](const DownloadStatus &a, const DownloadStatus &b) {
                      return a.id == b.id && a.state == b.state &&
                             a.message == b.message &&
                             std::abs(a.fraction - b.fraction) < 0.01;
                    })) {
      for (const auto &status : statuses) {
        const bool wasRunning = std::any_of(
            lastStatuses_.begin(), lastStatuses_.end(),
            [&](const DownloadStatus &p) {
              return p.id == status.id && (p.state == DownloadState::Running ||
                                           p.state == DownloadState::Queued);
            });
        if (wasRunning && status.state == DownloadState::Done) {
          if (ui_) ui_->ShowToast("Minecraft " + status.id + " installed", 4);
          refreshMenu(); // the version is now a game you can open
        } else if (wasRunning && status.state == DownloadState::Failed) {
          showError("Download of " + status.id + " failed: " + status.message);
        }
      }
      lastStatuses_ = statuses;
      downloads_.clearFinished();
      lastStatuses_.erase(
          std::remove_if(lastStatuses_.begin(), lastStatuses_.end(),
                         [](const DownloadStatus &s) {
                           return s.state == DownloadState::Done ||
                                  s.state == DownloadState::Failed ||
                                  s.state == DownloadState::Canceled;
                         }),
          lastStatuses_.end());
      refreshRemoteView();
      refreshMenu();
      changed = true;
    } else if (!statuses.empty()) {
      // Progress changes refresh rows without rebuilding the menu.
      refreshRemoteView();
      changed = true;
    }
    return changed;
  }

  // Starts the single background load and shows a busy overlay.
  bool beginLoad(const AppSession::LoadRequest &request) {
    if (loading_) {
      showError("Still loading " + loadRequest_.label + ". Please wait.");
      return false;
    }
    loading_ = true;
    loadRequest_ = request;
    debug::Logger::info() << "load started: " << request.label;
    if (ui_) ui_->SetBusy(true, "Loading " + request.label + "...");
    loadFuture_ = std::async(std::launch::async, &AppSession::performLoad,
                             request.target, config_.app_directory);
    return true;
  }

  // Applies the completed load on the UI thread.
  bool pollLoad() {
    if (!loading_ || loadFuture_.wait_for(std::chrono::seconds(0)) !=
                         std::future_status::ready)
      return false;
    AppSession::LoadResult result = loadFuture_.get();
    loading_ = false;
    if (ui_) ui_->SetBusy(false, {});
    const std::string error =
        session_.commitLoad(loadRequest_, std::move(result));
    if (error.empty()) {
      debug::Logger::info() << "load finished: " << loadRequest_.label;
    } else {
      debug::Logger::error()
          << "load failed (" << loadRequest_.label << "): " << error;
      showError(error);
    }
    refreshStore(); // Keep the grid and sidebar in sync, even on failure.
    return true;
  }

  // Handles menu shortcuts unless a text field has focus.
  bool handleShortcut(SDL_Keycode key, SDL_Keymod mod) {
    if (!ui_ || (mod & SDL_KMOD_CTRL) == 0 || ui_->TextInputFocused())
      return false;
    const bool shift = (mod & SDL_KMOD_SHIFT) != 0;
    SDL_Keycode k = key;
    if (k >= 'A' && k <= 'Z') k = k - 'A' + 'a';

    std::string action;
    switch (k) {
    case 'n':
      action = "open_pick_game";
      break;
    case 'o':
      action = "open_project";
      break;
    case 's':
      action = shift ? "save_project_as" : "save_project";
      break;
    case 'e':
      action = "export_pack";
      break;
    case 'w':
      action = "close_pack";
      break;
    case 'q':
      action = "quit";
      break;
    case 'd':
      action = "open_download";
      break;
    case 'g':
      action = "cycle_group";
      break;
    case 'p':
      action = "open_project_settings";
      break;
    case ',':
      action = "open_app_settings";
      break;
    case 'a':
      action = shift ? "deselect_all" : "select_visible";
      break;
    default:
      return false;
    }

    // Respect the File menu's enabled state.
    for (const auto &entry :
         buildFileMenu(session_, session_.installedGames())) {
      if (entry.action == action && !entry.enabled) return true;
    }
    if (action == "select_visible") {
      auto keys = ui_->VisibleItemKeys();
      if (keys.empty()) return true;
      for (const auto &key : store_.state().created)
        keys.push_back(key);
      std::sort(keys.begin(), keys.end());
      keys.erase(std::unique(keys.begin(), keys.end()), keys.end());
      store_.setCreated(keys);
      return true;
    }
    if (action == "deselect_all") {
      if (!store_.state().created.empty()) store_.setCreated({});
      return true;
    }
    if (action == "open_project_settings" && !session_.hasProject())
      return true;
    handleMenu(action, {});
    return true;
  }

  void requestQuit() {
    if (!session_.hasProject() || !session_.dirty() || !ui_) {
      running_ = false;
      return;
    }
    ui_->ShowUnsavedPrompt();
  }

  void answerUnsaved(const std::string &answer) {
    if (answer == "unsaved_cancel") {
      if (ui_) ui_->CloseDialog();
      return;
    }
    if (answer == "unsaved_dont_save") {
      running_ = false;
      return;
    }
    if (answer == "unsaved_save") {
      std::string error;
      if (session_.hasProjectFile()) {
        error = session_.saveProject();
      } else {
        const auto file = dialogs_.saveFile({"etproj"}, "project.etproj",
                                            config_.app_directory);
        if (!file) return; // canceled: stay open
        fs::path target = *file;
        if (target.extension() != ".etproj") target += ".etproj";
        error = session_.saveProjectAs(target);
      }
      if (!error.empty()) {
        showError("Could not save the project: " + error);
        return;
      }
      running_ = false;
      return;
    }
    if (answer == "unsaved_export") {
      const auto target = dialogs_.saveFile(
          {"zip"}, "enchantment-tweaks-pack.zip", config_.app_directory);
      if (!target) return; // canceled: stay open
      std::string name = target->filename().string();
      if (target->extension() != ".zip") name += ".zip";
      const auto outcome = session_.exportPack(target->parent_path(), name);
      if (!outcome.saved) {
        showError("Export failed: " + outcome.error);
        return;
      }
      running_ = false;
      return;
    }
  }

  void toggleDebug() {
    debugVisible_ = !debugVisible_;
    if (ui_) ui_->SetDebugVisible(debugVisible_);
    if (ui_) updateDebug(0.0, true);
  }

  // Measures CPU work for drawn frames; excludes the vsync wait.
  void recordDrawnFrame(double workMs) {
    if (!debugVisible_) return;
    frameMs_.push_back(workMs);
    if (frameMs_.size() > 120) frameMs_.pop_front();
    drawTicks_.push_back(SDL_GetTicks());
  }

  // Limits overlay refreshes to twice per second.
  bool updateDebug(double frameSeconds, bool force = false) {
    if (!debugVisible_ || !ui_) return false;
    debugTimer_ += frameSeconds;
    if (!force && debugTimer_ < 0.5) return false;
    debugTimer_ = 0.0;

    const Uint64 now = SDL_GetTicks();
    while (!drawTicks_.empty() && now - drawTicks_.front() > 1000)
      drawTicks_.pop_front();
    double sum = 0.0, worst = 0.0;
    for (double ms : frameMs_) {
      sum += ms;
      worst = std::max(worst, ms);
    }
    const double avg =
        frameMs_.empty() ? 0.0 : sum / static_cast<double>(frameMs_.size());
    const auto grid = ui_->GridStatistics();
    size_t active = 0;
    for (const auto &entry : downloads_.snapshot()) {
      if (entry.state == DownloadState::Queued ||
          entry.state == DownloadState::Running)
        ++active;
    }
    std::string text;
    text +=
        "Frames drawn per second: " + std::to_string(drawTicks_.size()) + "\n";
    text += "Work per drawn frame: avg " + format1(avg) + " ms, worst " +
            format1(worst) + " ms (last " + std::to_string(frameMs_.size()) +
            ")\n";
    text += "Frame rate this work allows: " +
            format1(avg > 0.0 ? 1000.0 / avg : 0.0) + " per second\n";
    text += "Grid rows " + std::to_string(grid.totalRows) + ", drawn " +
            std::to_string(grid.drawnRows) + ", columns " +
            std::to_string(grid.columns) + "\n";
    text += "Items " + std::to_string(store_.state().items.size()) +
            ", selected " + std::to_string(store_.state().created.size()) +
            "\n";
    text += "Downloads active " + std::to_string(active) + "\n";
    text += "Memory " + residentMemoryText();
    ui_->SetDebugText(text);
    return true;
  }

  static std::string format1(double value) {
    std::array<char, 32> buffer{};
    if (std::snprintf(buffer.data(), buffer.size(), "%.1f", value) < 0)
      return "?";
    return buffer.data();
  }

  static std::string residentMemoryText() {
#if defined(__linux__)
    // /proc/self/statm: "size resident shared ...", in pages.
    std::ifstream statm("/proc/self/statm");
    std::string sizePages, residentPages;
    if (statm >> sizePages >> residentPages) {
      const double mb = std::stod(residentPages) *
                        static_cast<double>(sysconf(_SC_PAGESIZE)) /
                        (1024.0 * 1024.0);
      return format1(mb) + " MB resident";
    }
#endif
    return "not available on this system";
  }

  double pollDelay() const {
    // Poll while background work is active.
    return manifestRequested_ || downloads_.busy() ||
                   downloads_.manifestLoading() || debugVisible_ || loading_
               ? 0.15
               : std::numeric_limits<double>::infinity();
  }

  void enqueueDownload(const std::string &id) {
    const auto it = std::ranges::find_if(
        manifest_.versions, [&](const auto &v) { return v.id == id; });
    if (it == manifest_.versions.end()) {
      showError("Version " + id +
                " is not in the list. Reopen Download Minecraft version.");
      return;
    }
    downloads_.enqueue(*it);
    refreshRemoteView();
    refreshMenu();
  }

  void uninstall(const std::string &id) {
    if (id.empty() || id.find_first_of("/\\") != std::string::npos ||
        id == "." || id == "..") {
      showError("Not an installed version id: " + id);
      return;
    }
    const auto folder = downloads_.downloader().versionsRoot() / id;
    if (!fs::exists(folder / (id + ".jar"))) {
      showError(id + " was not downloaded by this app. Remove it from your "
                     "launcher instead.");
      return;
    }
    const auto supported = session_.supportedVersions();
    if (session_.gameVersion() == id ||
        std::ranges::find(supported, id) != supported.end()) {
      showError("Version " + id +
                " is used by the open project. Close the project first.");
      return;
    }
    std::error_code ec;
    fs::remove_all(folder, ec);
    if (ec) {
      showError("Could not uninstall " + id + ": " + ec.message());
      return;
    }
    if (ui_) ui_->ShowToast("Uninstalled Minecraft " + id, 4);
    refreshRemoteView();
    refreshMenu();
  }

  void openProjectSettings() {
    if (!ui_ || !session_.hasProject()) return;
    ui_->SetItemOptions(session_.options());
    refreshSupportedVersions();
    ui_->OpenProjectSettingsDialog();
  }

  void refreshSupportedVersions() {
    if (!ui_) return;
    std::vector<et::SelectorUi::VersionChoice> choices;
    for (const auto &game : session_.installedGames()) {
      const auto &supported = session_.supportedVersions();
      choices.push_back(
          {game.id, std::ranges::find(supported, game.id) != supported.end(),
           true});
    }
    ui_->SetSupportedVersions(session_.isPackProject(), choices,
                              session_.supportedRangeText());
  }

  void toggleSupported(const std::string &id) {
    auto ids = session_.supportedVersions();
    const auto it = std::ranges::find(ids, id);
    if (it == ids.end())
      ids.push_back(id);
    else
      ids.erase(it);
    AppSession::LoadRequest request;
    const std::string error = session_.prepareSupported(ids, request);
    if (error.empty()) {
      beginLoad(request);
    } else {
      showError(error);
    }
    refreshSupportedVersions();
  }

  void handleMenu(const std::string &action, const std::string &arg) {
    std::string error;
    // TODO: Simplify menu action dispatch.
    if (action == "open_download") {
      openDownloadDialog();
      return;
    }
    if (action == "item_options") {
      if (ui_ && !arg.empty()) {
        const auto o = session_.overrideFor(arg);
        const auto merged = project::applyOverride(session_.options(), o);
        et::ItemOverrideValues v;
        v.thicknessPx = merged.thicknessPx;
        v.outlineSizePx = merged.outlineSizePx;
        v.outlineColor = merged.outlineColor;
        v.unenchantedOutline = merged.unenchantedOutline;
        v.unenchantedUnshaded = merged.unenchantedUnshaded;
        v.mergeColumns = merged.mergeColumns;
        ui_->SetItemOverrideValues(v);
        ui_->OpenItemOptionsDialog();
      }
      return;
    }
    if (action == "cycle_group") {
      project::AppSettings next = session_.appSettings();
      next.groupMode = next.groupMode == "none"
                           ? "prefix"
                           : (next.groupMode == "prefix" ? "suffix" : "none");
      showError(session_.setAppSettings(next));
      if (ui_) ui_->SetGroupMode(next.groupMode);
      return;
    }
    if (action == "open_pick_game") {
      if (ui_) {
        std::vector<et::SelectorUi::InstalledChoice> choices;
        for (const auto &game : session_.installedGames()) {
          choices.push_back({game.id, game.jar.string()});
        }
        ui_->SetInstalledGames(choices);
        ui_->OpenPickGameDialog();
      }
      return;
    }
    if (action == "remote_search") {
      remoteQuery_ = arg;
      refreshRemoteView();
      return;
    }
    if (action == "remote_snapshots") {
      includeSnapshots_ = !includeSnapshots_;
      refreshRemoteView();
      return;
    }
    if (action == "remote_toggle") {
      if (!openTypes_.insert(arg).second) openTypes_.erase(arg);
      refreshRemoteView();
      return;
    }
    if (action == "download_version") {
      enqueueDownload(arg);
      return;
    }
    if (action == "cancel_download") {
      downloads_.cancel(arg);
      refreshRemoteView();
      return;
    }
    if (action == "uninstall_version") {
      uninstall(arg);
      return;
    }
    if (action == "toggle_supported") {
      toggleSupported(arg);
      return;
    }
    if (action == "new_game_project") {
      if (ui_) ui_->CloseDialog();
      const fs::path jar(arg);
      AppSession::LoadRequest request;
      error = session_.prepareNewGame(
          jar, jar.parent_path().filename().string(), request);
      if (error.empty()) beginLoad(request);
    } else if (action == "new_game_browse") {
      if (ui_) ui_->CloseDialog();
      if (const auto jar =
              dialogs_.openFile({"jar", "zip"}, config_.app_directory)) {
        AppSession::LoadRequest request;
        error = session_.prepareNewGame(*jar, jar->stem().string(), request);
        if (error.empty()) beginLoad(request);
      }
    } else if (action == "new_pack_zip") {
      newPackProject(dialogs_.openFile({"zip"}, config_.app_directory));
    } else if (action == "new_pack_folder") {
      newPackProject(dialogs_.pickFolder(config_.app_directory));
    } else if (action == "open_project") {
      if (const auto file =
              dialogs_.openFile({"etproj"}, config_.app_directory)) {
        AppSession::LoadRequest request;
        error = session_.prepareOpenProject(*file, request);
        if (error.empty()) beginLoad(request);
      }
    } else if (action == "save_project") {
      error = session_.hasProjectFile() ? session_.saveProject() : saveAs();
    } else if (action == "save_project_as") {
      error = saveAs();
    } else if (action == "open_pack") {
      if (const auto zip = dialogs_.openFile({"zip"}, config_.app_directory)) {
        AppSession::LoadRequest request;
        error = session_.prepareOpenPack(*zip, request);
        if (error.empty()) beginLoad(request);
      }
    } else if (action == "close_pack") {
      if (session_.hasPack() && !session_.isPackProject()) {
        AppSession::LoadRequest request;
        error = session_.prepareClosePack(request);
        if (error.empty()) beginLoad(request);
      } else {
        session_.closeCurrent();
        refreshStore();
      }
    } else if (action == "export_pack") {
      error = exportPack();
    } else if (action == "open_app_settings") {
      if (ui_) {
        ui_->SetAppSettings(session_.appSettings());
        ui_->OpenAppSettingsDialog();
      }
      return;
    } else if (action == "open_project_settings") {
      openProjectSettings();
      return;
    } else if (action == "quit") {
      requestQuit();
      return;
    } else if (action == "unsaved_cancel" || action == "unsaved_dont_save" ||
               action == "unsaved_save" || action == "unsaved_export") {
      answerUnsaved(action);
      refreshMenu();
      return;
    }
    showError(error);
    refreshMenu();
  }

  void showError(const std::string &error) {
    if (!error.empty() && ui_) ui_->ShowToast(error, 6);
  }

  std::string saveAs() {
    if (!session_.hasProject()) return "open a project before saving";
    const auto file =
        dialogs_.saveFile({"etproj"}, "project.etproj", config_.app_directory);
    if (!file) return {};
    fs::path target = *file;
    if (target.extension() != ".etproj") target += ".etproj";
    return session_.saveProjectAs(target);
  }

  std::string exportPack() {
    if (!session_.hasProject()) return "open a project first";
    const auto target = dialogs_.saveFile(
        {"zip"}, "enchantment-tweaks-pack.zip", config_.app_directory);
    if (!target) return {};
    std::string name = target->filename().string();
    if (target->extension() != ".zip") name += ".zip";
    const auto outcome = session_.exportPack(target->parent_path(), name);
    if (outcome.saved && ui_)
      ui_->ShowToast("Pack saved to " + outcome.path.string(), 6);
    return outcome.error;
  }

  ApplicationConfig config_;
  AppSession session_;
  DownloadManager downloads_;

  project::VersionManifest manifest_; // the list from Mojang, once loaded
  std::set<std::string> openTypes_{"release"};
  bool includeSnapshots_ = false;

  std::string remoteQuery_;
  std::string remoteNote_;
  std::vector<DownloadStatus> lastStatuses_;

  bool manifestRequested_ = false;
  bool pendingStoreSync_ = false;
  bool loading_ = false;

  std::future<AppSession::LoadResult> loadFuture_;

  AppSession::LoadRequest loadRequest_;

  bool debugVisible_ = false;

  std::deque<double> frameMs_;   // work per drawn frame, in ms
  std::deque<Uint64> drawTicks_; // when each frame was drawn, in ms since start

  double debugTimer_ = 0.0;

  FileDialogs dialogs_;

  et::Store store_;
  et::UiCallbacks callbacks_;
  et::SelectorUi *ui_ = nullptr;

  bool syncing_ = false;
  bool running_ = false;
};

Application::Application(ApplicationConfig config)
    : impl_(std::make_unique<Impl>(std::move(config))) {}
Application::~Application() = default;
et::Store &Application::store() { return impl_->store_; }
const et::Store &Application::store() const { return impl_->store_; }
void Application::requestClose() noexcept { impl_->requestQuit(); }

bool Application::init() {
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

void Application::cleanup() {
  Rml::Shutdown();
  resource_.renderer.reset();
  driveker::RenderSystem::shutdown();
  SDL_GL_DestroyContext(resource_.gl);
  SDL_DestroyWindow(resource_.window);
  SDL_Quit();
}

int Application::run() {

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
    throw std::runtime_error("Could not initialize RmlUi selector");
  }

  Rml::Context *context = Rml::CreateContext("selector", {width, height});

  auto ui = std::make_unique<et::SelectorUi>(
      context, impl_->store_, impl_->callbacks_,
      impl_->config_.assets_directory.string());

  if (!context || !ui->Initialize()) {
    ui.reset();
    system.reset();
    cleanup();
    throw std::runtime_error("Could not load RmlUi selector documents");
  }

  impl_->ui_ = ui.get();
  impl_->ui_->SetAppSettings(impl_->session_.appSettings());
  impl_->ui_->SetGroupMode(impl_->session_.appSettings().groupMode);
  impl_->ui_->SetMenuData(
      buildFileMenu(impl_->session_, impl_->session_.installedGames()),
      impl_->session_.statusLine());
  impl_->refreshStore();

  Uint64 previous = SDL_GetTicks();
  impl_->running_ = true;
  bool redraw = true;

  while (impl_->running_) {
    SDL_Event event;
    const double next = std::min({context->GetNextUpdateDelay(),
                                  ui->NextUpdateDelay(), impl_->pollDelay()});
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
      if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_F3 &&
          !event.key.repeat) {
        impl_->toggleDebug();
      }
      if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat) {
        impl_->handleShortcut(event.key.key, event.key.mod);
      }

      if (event.type == SDL_EVENT_QUIT)
        impl_->requestQuit();

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
    if (impl_->poll()) redraw = true;
    if (!redraw && !receivedEvent && std::isinf(next)) continue;

    const Uint64 now = SDL_GetTicks();

    const double frameSeconds = static_cast<double>(now - previous) / 1000.0;
    ui->Update(frameSeconds);
    previous = now;
    if (impl_->updateDebug(frameSeconds)) redraw = true;

    const Uint64 workStart = SDL_GetPerformanceCounter();
    context->Update();
    renderer->BeginFrame();

    context->Render();
    renderer->EndFrame();
    const double workMs =
        static_cast<double>(SDL_GetPerformanceCounter() - workStart) * 1000.0 /
        static_cast<double>(SDL_GetPerformanceFrequency());
    impl_->recordDrawnFrame(workMs);

    SDL_GL_SwapWindow(resource_.window);
    redraw = false;
  }

  ui.reset();
  system.reset();
  cleanup();

  impl_->ui_ = nullptr;
  return 0;
}
} // namespace enchantment_tweaks::native
