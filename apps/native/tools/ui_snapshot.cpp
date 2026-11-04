
// random bullshit to help debugging the ui as the app is unstable
//   ui_snapshot <assets dir> <game.jar> <output dir>

#include "app/app_session.hpp"
#include "app/file_menu.hpp"
#include "app/ui/selector_ui.h"
#include "core/project/version_list.hpp"
#include "soft_renderer.hpp"

#include <set>

#include <RmlUi/Core.h>
#include <RmlUi/Core/Elements/ElementFormControlInput.h>

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace enchantment_tweaks;

namespace {

constexpr int kWidth = 1280;
constexpr int kHeight = 860;

class SnapshotSystem final : public Rml::SystemInterface {
public:
  double GetElapsedTime() override { return time_; }
  bool LogMessage(Rml::Log::Type type, const Rml::String &message) override {
    if (type == Rml::Log::LT_ERROR || type == Rml::Log::LT_WARNING)
      std::printf("RmlUi %s: %s\n",
                  type == Rml::Log::LT_ERROR ? "error" : "warning",
                  message.c_str());
    return true;
  }
  void JoinPath(Rml::String &out, const Rml::String &document,
                const Rml::String &path) override {
    if (et::IsExistingAbsoluteFile(path)) {
      out = path;
      return;
    }
    Rml::SystemInterface::JoinPath(out, document, path);
  }
  double time_ = 0;
};

struct Harness {
  Rml::Context *context = nullptr;
  SoftRenderer *renderer = nullptr;
  SnapshotSystem *system = nullptr;
  et::SelectorUi *ui = nullptr;
  fs::path outDir;

  void advance(int frames = 3) {
    for (int i = 0; i < frames; ++i) {
      system->time_ += 1.0 / 60.0;
      ui->Update(1.0 / 60.0);
      context->Update();
    }
  }

  void save(const char *name) {
    advance();
    renderer->BeginFrame();
    context->Render();
    renderer->SavePng((outDir / name).string(), 238, 238, 238);
    std::printf("wrote %s\n", (outDir / name).string().c_str());
  }

  // Finds the first visible element matching `selector` (data-for templates are
  // hidden and skipped).
  Rml::Element *find(const char *selector) {
    Rml::ElementList list;
    ui->MainDocument()->QuerySelectorAll(list, selector);
    for (auto *e : list) {
      bool visible = true;
      for (auto *p = e; p; p = p->GetParentNode())
        if (p->GetComputedValues().display() == Rml::Style::Display::None) {
          visible = false;
          break;
        }
      if (visible) return e;
    }
    return nullptr;
  }
  Rml::Element *findIn(Rml::ElementDocument *doc, const char *selector,
                       const char *contains = nullptr) {
    Rml::ElementList list;
    doc->QuerySelectorAll(list, selector);
    for (auto *e : list) {
      if (contains && e->GetInnerRML().find(contains) == std::string::npos)
        continue;
      bool visible = true;
      for (auto *p = e; p; p = p->GetParentNode())
        if (p->GetComputedValues().display() == Rml::Style::Display::None) {
          visible = false;
          break;
        }
      if (visible) return e;
    }
    return nullptr;
  }

  void rightClick(Rml::Element *e) {
    if (!e) {
      std::printf("right click: target not found\n");
      return;
    }
    const auto off = e->GetAbsoluteOffset(Rml::BoxArea::Border);
    const auto size = e->GetBox().GetSize(Rml::BoxArea::Border);
    context->ProcessMouseMove(static_cast<int>(off.x + size.x / 2),
                              static_cast<int>(off.y + size.y / 2), 0);
    advance(1);
    context->ProcessMouseButtonDown(1, 0); // right button
    context->ProcessMouseButtonUp(1, 0);
    advance();
  }

  void click(Rml::Element *e) {
    if (!e) {
      std::printf("click: target not found\n");
      return;
    }
    const auto off = e->GetAbsoluteOffset(Rml::BoxArea::Border);
    const auto size = e->GetBox().GetSize(Rml::BoxArea::Border);
    context->ProcessMouseMove(static_cast<int>(off.x + size.x / 2),
                              static_cast<int>(off.y + size.y / 2), 0);
    advance(1);
    context->ProcessMouseButtonDown(0, 0);
    context->ProcessMouseButtonUp(0, 0);
    advance();
  }
};

} // namespace

// Counts the elements of a document, hidden ones included: they are what the
// layout keeps.
static size_t countElements(Rml::Element *e) {
  size_t n = 1;
  for (int i = 0; i < e->GetNumChildren(); ++i)
    n += countElements(e->GetChild(i));
  return n;
}

int main(int argc, char **argv) {
  if (argc < 4) {
    std::printf("usage: ui_snapshot <assets dir> <game.jar> <output dir>\n");
    return 2;
  }
  const fs::path assets = argv[1];
  const fs::path jar = argv[2];
  Harness h;
  h.outDir = argv[3];
  fs::create_directories(h.outDir);
  fs::path appDir = h.outDir / "app-data";
  fs::remove_all(appDir);

  SnapshotSystem system;
  SoftRenderer renderer(kWidth, kHeight);
  h.system = &system;
  h.renderer = &renderer;
  Rml::SetSystemInterface(&system);
  Rml::SetRenderInterface(&renderer);
  if (!Rml::Initialise()) return 1;
  for (int w : {400, 500, 600, 700}) {
    Rml::LoadFontFace(
        (assets / "fonts" / ("Inter-" + std::to_string(w) + ".ttf")).string(),
        false, static_cast<Rml::Style::FontWeight>(w));
  }
  h.context = Rml::CreateContext("snapshot", {kWidth, kHeight});

  // The same objects the application uses: session, store and UI.
  native::AppSession session(appDir);
  et::Store store(et::Catalog{}, std::nullopt);
  et::UiCallbacks callbacks;
  callbacks.persist = [&](const et::PersistedState &state) {
    session.setSelection(state.created.value_or(std::vector<std::string>{}));
  };
  et::SelectorUi ui(h.context, store, callbacks, assets.string());
  if (!ui.Initialize()) {
    std::printf("could not load the UI documents\n");
    return 1;
  }
  h.ui = &ui;
  const bool bench = argc > 4 && std::string(argv[4]) == "--bench";
  ui.SetAppSettings(et::AppSettingsValues{});
  ui.SetItemOptions(et::ItemOptionsValues{});
  auto refreshMenus = [&] {
    ui.SetMenuData(native::buildFileMenu(session, session.installedGames()),
                   session.statusLine());
  };
  refreshMenus();

  if (bench && !jar.empty() && fs::exists(jar)) {
    native::AppSession::LoadRequest request;
    std::string error = session.prepareNewGame(
        jar, jar.parent_path().filename().string(), request);
    if (error.empty())
      error = session.commitLoad(
          request, native::AppSession::performLoad(request.target, appDir));
    if (!error.empty()) std::printf("bench: %s\n", error.c_str());
    store.replaceCatalog(session.uiCatalog());
    store.setVersions({session.gameVersion()});
    h.advance(5);
    Rml::Element *main = ui.MainDocument()->QuerySelector(".main");
    using clock = std::chrono::steady_clock;
    std::vector<double> frame;
    const int frames = 120;
    for (int i = 0; i < frames; ++i) {
      if (main) main->SetScrollTop(static_cast<float>(i * 45));
      const auto t0 = clock::now();
      system.time_ += 1.0 / 60.0;
      ui.Update(1.0 / 60.0);
      h.context->Update();
      renderer.BeginFrame();
      h.context->Render();
      frame.push_back(
          std::chrono::duration<double, std::milli>(clock::now() - t0).count());
    }
    double sum = 0, worst = 0;
    for (double ms : frame) {
      sum += ms;
      worst = std::max(worst, ms);
    }
    std::printf("bench: items %zu, document elements %zu, frame work avg %.1f "
                "ms, worst %.1f ms over %d frames\n",
                store.state().items.size(), countElements(ui.MainDocument()),
                sum / frame.size(), worst, frames);
    return 0;
  }

  // 1. Blank window, nothing open.
  h.save("01_blank.png");

  // 2. File menu open.
  h.click(h.findIn(ui.MainDocument(), "button.menu-title", "File"));
  h.save("02_file_menu.png");
  h.click(h.findIn(ui.MainDocument(), "button.menu-title",
                   "File")); // close it again

  // 3. A game project with selections.
  if (!jar.empty() && fs::exists(jar)) {
    // The same steps the application runs, just without the worker thread.
    native::AppSession::LoadRequest request;
    std::string error = session.prepareNewGame(
        jar, jar.parent_path().filename().string(), request);
    if (error.empty())
      error = session.commitLoad(
          request, native::AppSession::performLoad(request.target, appDir));
    if (!error.empty()) std::printf("newGameProject: %s\n", error.c_str());
    store.replaceCatalog(session.uiCatalog());
    store.setVersions({session.gameVersion()});
    store.setCreated({"minecraft:diamond_sword", "minecraft:iron_sword"});
    refreshMenus();
    h.save("03_game_loaded.png");
  }

  // 4. Application settings, opened from the Settings button.
  ui.SetAppSettings(et::AppSettingsValues{});
  ui.OpenAppSettingsDialog();
  h.save("04_app_settings.png");

  // 5. Project settings dialog, with the item options of the project.
  ui.CloseDialog();
  et::ItemOptionsValues options;
  options.thicknessPx = 1.5;
  options.outlineSizePx = 1.0;
  ui.SetItemOptions(options);
  ui.SetSupportedVersions(false, {});
  ui.OpenProjectSettingsDialog();
  h.save("05_project_settings.png");

  // 5b. Download dialog with a manifest of about 900 versions, grouped and
  // searchable.
  {
    std::vector<project::RemoteVersion> manifest;
    for (int minor = 1; minor <= 21; ++minor) {
      for (int patch = 0; patch <= 6; ++patch) {
        const std::string id = "1." + std::to_string(minor) +
                               (patch ? "." + std::to_string(patch) : "");
        manifest.push_back({id, "release", "u/" + id});
      }
    }
    for (int year = 10; year <= 26; ++year) {
      for (int week = 1; week <= 52; ++week) {
        for (char letter : {'a', 'b'}) {
          const std::string id = std::to_string(year) + "w" +
                                 (week < 10 ? "0" : "") + std::to_string(week) +
                                 letter;
          manifest.push_back({id, "snapshot", "u/" + id});
        }
      }
    }
    for (int i = 1; i <= 120; ++i)
      manifest.push_back({"b1." + std::to_string(i % 9), "old_beta",
                          "u/b" + std::to_string(i)});
    for (int i = 1; i <= 30; ++i)
      manifest.push_back({"a1." + std::to_string(i % 3), "old_alpha",
                          "u/a" + std::to_string(i)});

    project::VersionViewOptions options;
    options.openTypes = {"release"};
    options.rowLimit = 200;
    std::set<std::string> installed{"1.21.4", "1.21.2"}, managed{"1.21.4"};
    std::vector<et::SelectorUi::RemoteGroupChoice> groups;
    for (const auto &group :
         project::groupVersions(manifest, installed, managed, options)) {
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
        if (row.id == "1.21.5") {
          r.button = "cancel";
          r.showBar = true;
          r.fraction = 0.58;
          r.status = "Downloading";
        }
        if (row.id == "1.21.3") {
          r.button = "retry";
          r.status = "Connection reset";
        }
        g.rows.push_back(r);
      }
      groups.push_back(std::move(g));
    }
    ui.SetRemoteView(groups, "", "", "versions from 1.21.4", false);
    ui.OpenDownloadDialog();
    h.save("06_download.png");

    ui.CloseDialog();
    options.query = "24w";
    groups.clear();
    for (const auto &group :
         project::groupVersions(manifest, installed, managed, options)) {
      et::SelectorUi::RemoteGroupChoice g;
      g.type = group.type;
      g.total = group.total;
      g.matches = group.matches;
      g.open = group.open;
      for (const auto &row : group.rows) {
        et::SelectorUi::RemoteRowChoice r;
        r.id = row.id;
        r.type = row.type;
        r.button = "download";
        g.rows.push_back(r);
      }
      groups.push_back(std::move(g));
    }
    ui.SetRemoteView(groups, "", "24w", "search: 24w", false);
    ui.OpenDownloadDialog();
    h.save("06b_download_search.png");
    ui.CloseDialog();

    // The rows that show progress, failure and uninstall.
    ui.CloseDialog();
    options.query = "1.21.";
    groups.clear();
    for (const auto &group :
         project::groupVersions(manifest, installed, managed, options)) {
      et::SelectorUi::RemoteGroupChoice g;
      g.type = group.type;
      g.total = group.total;
      g.matches = group.matches;
      g.open = group.open;
      for (const auto &row : group.rows) {
        et::SelectorUi::RemoteRowChoice r;
        r.id = row.id;
        r.type = row.type;
        r.button =
            row.managed ? "uninstall" : (row.installed ? "none" : "download");
        r.installed = row.installed;
        if (row.id == "1.21.5") {
          r.button = "cancel";
          r.showBar = true;
          r.fraction = 0.58;
          r.status = "Downloading";
        }
        if (row.id == "1.21.3") {
          r.button = "retry";
          r.status = "Connection reset";
        }
        g.rows.push_back(r);
      }
      groups.push_back(std::move(g));
    }
    ui.SetRemoteView(groups, "", "1.21.", "search: 1.21.", false);
    ui.OpenDownloadDialog();
    h.save("06c_download_states.png");
    ui.CloseDialog();
  }

  // 5c. The grid grouped by prefix.
  ui.SetGroupMode("prefix");
  h.save("08_grouped.png");
  ui.SetGroupMode("none");

  // 5d. Right-click menus: one card, then a group header.
  if (!jar.empty() && fs::exists(jar)) {
    store.setCreated({"minecraft:diamond_sword"});
    refreshMenus();
    Rml::ElementList cards;
    ui.MainDocument()->QuerySelectorAll(cards, "button.card");
    for (auto *card : cards) {
      bool visible = true;
      for (auto *p = card; p; p = p->GetParentNode())
        if (p->GetComputedValues().display() == Rml::Style::Display::None) {
          visible = false;
          break;
        }
      if (visible) {
        h.rightClick(card);
        break;
      }
    }
    h.save("09_card_context.png");
    h.click(h.findIn(ui.MainDocument(), "button.menu-title",
                     "File")); // closes the menu on an outside click
    Rml::ElementList heads;
    ui.MainDocument()->QuerySelectorAll(heads, "button.sec-head");
    if (!heads.empty()) h.rightClick(heads[0]);
    h.save("10_group_context.png");
  }

  // 5e. Hover tooltip on the merge option in project settings.
  {
    ui.CloseDialog();
    ui.SetItemOptions(et::ItemOptionsValues{});
    ui.OpenProjectSettingsDialog();
    Rml::ElementList labels;
    if (ui.MainDocument()) {
    }
    Rml::ElementDocument *doc = ui.ActiveDialog();
    if (doc) {
      doc->QuerySelectorAll(labels, "label");
      for (auto *label : labels) {
        if (label->GetInnerRML().find("Merge runs") != std::string::npos) {
          const auto off = label->GetAbsoluteOffset(Rml::BoxArea::Border);
          h.context->ProcessMouseMove(static_cast<int>(off.x + 30),
                                      static_cast<int>(off.y + 6), 0);
          h.advance(2);
          break;
        }
      }
    }
    h.save("11_merge_tooltip.png");
    ui.CloseDialog();
  }

  // 5f. The Created items chips: hover one (the remove state).
  if (!jar.empty() && fs::exists(jar)) {
    // Close any open menu first: a click on empty space outside it.
    h.context->ProcessMouseMove(1240, 800, 0);
    h.advance(1);
    h.context->ProcessMouseButtonDown(0, 0);
    h.context->ProcessMouseButtonUp(0, 0);
    h.advance(2);
    Rml::ElementList chips;
    ui.MainDocument()->QuerySelectorAll(chips, "button.chip");
    if (!chips.empty()) {
      const auto off = chips[0]->GetAbsoluteOffset(Rml::BoxArea::Border);
      const auto size = chips[0]->GetBox().GetSize(Rml::BoxArea::Border);
      h.context->ProcessMouseMove(static_cast<int>(off.x + size.x / 2),
                                  static_cast<int>(off.y + size.y / 2), 0);
      h.advance(3);
    }
    h.save("12_chip_hover.png");
    h.context->ProcessMouseMove(0, 0, 0);
    h.advance(2);
  }

  // 5g. Selected tiles, in light and in dark.
  if (!jar.empty() && fs::exists(jar)) {
    auto visible = ui.VisibleItemKeys();
    if (visible.size() > 3) visible.resize(3);
    store.setCreated(visible);
    h.advance(3);
    h.save("13_selected_light.png");
    et::AppSettingsValues darkSel;
    darkSel.darkMode = true;
    ui.SetAppSettings(darkSel);
    h.advance(3);
    h.save("13b_selected_dark.png");
    ui.SetAppSettings(et::AppSettingsValues{});
  }

  // 6. Dark theme on the whole page.
  ui.CloseDialog();
  et::AppSettingsValues dark;
  dark.darkMode = true;
  ui.SetAppSettings(dark);
  h.click(h.findIn(ui.MainDocument(), "button.menu-title", "File"));
  h.save("07_dark_page.png");
  return 0;
}
