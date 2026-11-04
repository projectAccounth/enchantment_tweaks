#include "selector_ui.h"
#include "../model/pack.h"
#include "../model/validation.h"
#include "RmlUi/Config/Config.h"
#include "RmlUi/Core/Context.h"
#include "RmlUi/Core/DataModelHandle.h"
#include "RmlUi/Core/Element.h"
#include "RmlUi/Core/ElementDocument.h"
#include "RmlUi/Core/Event.h"
#include "RmlUi/Core/Input.h"
#include "RmlUi/Core/Types.h"
#include "RmlUi/Core/Variant.h"
#include "app/model/store.h"
#include "app/model/types.h"
#include "core/api/item_groups.hpp"
#include "core/util/text.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <exception>
#include <filesystem>
#include <functional>
#include <limits>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

namespace et {

namespace {

constexpr double kHeaderRowDp = 42.0;
constexpr double kCardRowDp = 195.0;

constexpr double kEmptyRowDp = 56.0;

constexpr double kCardPitchDp = 155.625;

constexpr double kGridSidePaddingDp = 4.0;

constexpr double kOverscanDp = 600.0;

double rowHeight(bool isHeader, bool isCards) {
  return isHeader ? kHeaderRowDp : (isCards ? kCardRowDp : kEmptyRowDp);
}

const char *const kQuote = "\xE2\x80\x9C";
const char *const kUnquote = "\xE2\x80\x9D";

Rml::String Arg0(const Rml::VariantList &args) {
  return args.empty() ? Rml::String() : args[0].Get<Rml::String>();
}

} // namespace

bool IsExistingAbsoluteFile(const std::string &path) {
  const fs::path p(path);
  std::error_code ec;
  return p.is_absolute() && !p.has_root_name() && fs::is_regular_file(p, ec);
}

Rml::String SelectorUi::ResolveIcon(const std::string &icon) const {
  if (icon.empty() || icon.starts_with("data:"))
    return "../sprites/placeholder.png";
  if (icon.find_first_of("/\\.") == std::string::npos)
    return "../sprites/" + icon + ".png";

  return icon;
}

SelectorUi::SelectorUi(Rml::Context *context, Store &store,
                       UiCallbacks callbacks, std::string assetDir)
    : ctx_(context), store_(store), cb_(std::move(callbacks)),
      asset_dir_(std::move(assetDir)) {}

SelectorUi::~SelectorUi() {
  if (!ctx_) return;
  for (auto *doc : {main_doc_, item_doc_, ns_doc_})
    if (doc) doc->Close();
  ctx_->RemoveDataModel("selector");
}

bool SelectorUi::Initialize() {
  if (!RegisterModel()) return false;

  store_.Subscribe([this](unsigned changes) {
    Refresh();
    if ((changes & change::Persisted) && cb_.persist)
      cb_.persist(store_.toPersisted());
    if ((changes & change::Selection) && cb_.selection_changed)
      cb_.selection_changed(store_.selectedVersions(), store_.state().created);
  });

  Refresh();

  const std::string ui = asset_dir_ + "/ui/";
  main_doc_ = ctx_->LoadDocument(ui + "selector.rml");
  item_doc_ = ctx_->LoadDocument(ui + "add_item.rml");
  ns_doc_ = ctx_->LoadDocument(ui + "add_namespace.rml");
  settings_doc_ = ctx_->LoadDocument(ui + "settings.rml");
  project_settings_doc_ = ctx_->LoadDocument(ui + "project_settings.rml");
  download_doc_ = ctx_->LoadDocument(ui + "download.rml");
  pick_game_doc_ = ctx_->LoadDocument(ui + "pick_game.rml");
  message_doc_ = ctx_->LoadDocument(ui + "message.rml");
  unsaved_doc_ = ctx_->LoadDocument(ui + "unsaved.rml");
  item_options_doc_ = ctx_->LoadDocument(ui + "item_options.rml");

  if (!main_doc_ || !item_doc_ || !ns_doc_ || !settings_doc_ ||
      !project_settings_doc_ || !download_doc_ || !pick_game_doc_ ||
      !message_doc_ || !item_options_doc_ || !unsaved_doc_)
    return false; // RmlUi logged

  item_doc_->Hide();
  ns_doc_->Hide();
  item_doc_->AddEventListener("keydown", &dialog_keys_,
                              /*in_capture_phase=*/true);
  ns_doc_->AddEventListener("keydown", &dialog_keys_,
                            /*in_capture_phase=*/true);
  settings_doc_->Hide();
  settings_doc_->AddEventListener("keydown", &dialog_keys_,
                                  /*in_capture_phase=*/true);
  project_settings_doc_->Hide();
  project_settings_doc_->AddEventListener("keydown", &dialog_keys_,
                                          /*in_capture_phase=*/true);
  download_doc_->Hide();
  download_doc_->AddEventListener("keydown", &dialog_keys_,
                                  /*in_capture_phase=*/true);
  pick_game_doc_->Hide();
  pick_game_doc_->AddEventListener("keydown", &dialog_keys_,
                                   /*in_capture_phase=*/true);
  message_doc_->Hide();
  message_doc_->AddEventListener("keydown", &dialog_keys_,
                                 /*in_capture_phase=*/true);
  unsaved_doc_->Hide();
  unsaved_doc_->AddEventListener("keydown", &dialog_keys_,
                                 /*in_capture_phase=*/true);
  item_options_doc_->Hide();
  item_options_doc_->AddEventListener("keydown", &dialog_keys_,
                                      /*in_capture_phase=*/true);
  main_doc_->Show();
  return true;
}

bool SelectorUi::RegisterModel() {
  Rml::DataModelConstructor c = ctx_->CreateDataModel("selector");
  if (!c) return false;

  // Register each struct before registering arrays of that struct.
  if (auto h = c.RegisterStruct<VersionVm>()) {
    h.RegisterMember("label", &VersionVm::label);
    h.RegisterMember("on", &VersionVm::on);
  }
  c.RegisterArray<Rml::Vector<VersionVm>>();

  if (auto h = c.RegisterStruct<CardVm>()) {
    h.RegisterMember("key", &CardVm::key);
    h.RegisterMember("name", &CardVm::name);
    h.RegisterMember("id", &CardVm::id);
    h.RegisterMember("icon", &CardVm::icon);
    h.RegisterMember("selected", &CardVm::selected);
    h.RegisterMember("visible", &CardVm::visible);
  }
  c.RegisterArray<Rml::Vector<CardVm>>();
  if (auto h = c.RegisterStruct<SectionVm>()) {
    h.RegisterMember("ns_id", &SectionVm::ns_id);
    h.RegisterMember("label", &SectionVm::label);
    h.RegisterMember("collapse_key", &SectionVm::collapse_key);
    h.RegisterMember("open", &SectionVm::open);
    h.RegisterMember("empty", &SectionVm::empty);
    h.RegisterMember("visible", &SectionVm::visible);
    h.RegisterMember("cards", &SectionVm::cards);
  }
  c.RegisterArray<Rml::Vector<SectionVm>>();

  if (auto h = c.RegisterStruct<ChipVm>()) {
    h.RegisterMember("key", &ChipVm::key);
    h.RegisterMember("name", &ChipVm::name);
  }
  c.RegisterArray<Rml::Vector<ChipVm>>();
  if (auto h = c.RegisterStruct<GroupVm>()) {
    h.RegisterMember("ns_id", &GroupVm::ns_id);
    h.RegisterMember("label", &GroupVm::label);
    h.RegisterMember("collapse_key", &GroupVm::collapse_key);
    h.RegisterMember("open", &GroupVm::open);
    h.RegisterMember("chips", &GroupVm::chips);
  }
  c.RegisterArray<Rml::Vector<GroupVm>>();

  if (auto h = c.RegisterStruct<SortOptionVm>()) {
    h.RegisterMember("key", &SortOptionVm::key);
    h.RegisterMember("label", &SortOptionVm::label);
    h.RegisterMember("checked", &SortOptionVm::checked);
  }
  c.RegisterArray<Rml::Vector<SortOptionVm>>();
  if (auto h = c.RegisterStruct<NamespaceOptionVm>()) {
    h.RegisterMember("id", &NamespaceOptionVm::id);
    h.RegisterMember("label", &NamespaceOptionVm::label);
  }
  c.RegisterArray<Rml::Vector<NamespaceOptionVm>>();
  if (auto h = c.RegisterStruct<ItemFormVm>()) {
    h.RegisterMember("name", &ItemFormVm::name);
    h.RegisterMember("id", &ItemFormVm::id);
    h.RegisterMember("ns", &ItemFormVm::ns);
    h.RegisterMember("icon_name", &ItemFormVm::icon_name);
    h.RegisterMember("error", &ItemFormVm::error);
  }
  if (auto h = c.RegisterStruct<NamespaceFormVm>()) {
    h.RegisterMember("label", &NamespaceFormVm::label);
    h.RegisterMember("id", &NamespaceFormVm::id);
    h.RegisterMember("error", &NamespaceFormVm::error);
  }

  if (auto h = c.RegisterStruct<MenuItemVm>()) {
    h.RegisterMember("label", &MenuItemVm::label);
    h.RegisterMember("action", &MenuItemVm::action);
    h.RegisterMember("arg", &MenuItemVm::arg);
    h.RegisterMember("enabled", &MenuItemVm::enabled);
    h.RegisterMember("separator", &MenuItemVm::separator);
    h.RegisterMember("shortcut", &MenuItemVm::shortcut);
  }
  c.RegisterArray<Rml::Vector<MenuItemVm>>();
  if (auto h = c.RegisterStruct<AppFormVm>()) {
    h.RegisterMember("display_mode", &AppFormVm::display_mode);
    h.RegisterMember("error", &AppFormVm::error);
    h.RegisterMember("dark", &AppFormVm::dark);
  }

  if (auto h = c.RegisterStruct<ProjectFormVm>()) {
    h.RegisterMember("thickness", &ProjectFormVm::thickness);
    h.RegisterMember("outline_size", &ProjectFormVm::outline_size);
    h.RegisterMember("outline_color", &ProjectFormVm::outline_color);
    h.RegisterMember("error", &ProjectFormVm::error);
    h.RegisterMember("unenchanted", &ProjectFormVm::unenchanted);
    h.RegisterMember("unshaded", &ProjectFormVm::unshaded);
    h.RegisterMember("merge", &ProjectFormVm::merge);
    h.RegisterMember("pack_note", &ProjectFormVm::pack_note);
    h.RegisterMember("pack_range", &ProjectFormVm::pack_range);
  }

  c.Bind("file_menu", &file_menu_);
  c.Bind("menu_open", &menu_open_);
  c.Bind("status_text", &status_text_);
  c.Bind("dark_mode", &dark_mode_);

  if (auto h = c.RegisterStruct<SupportedVm>()) {
    h.RegisterMember("id", &SupportedVm::id);
    h.RegisterMember("on", &SupportedVm::on);
    h.RegisterMember("installed", &SupportedVm::installed);
  }
  c.RegisterArray<Rml::Vector<SupportedVm>>();
  if (auto h = c.RegisterStruct<RemoteRowVm>()) {
    h.RegisterMember("id", &RemoteRowVm::id);
    h.RegisterMember("label", &RemoteRowVm::label);
    h.RegisterMember("button", &RemoteRowVm::button);
    h.RegisterMember("status", &RemoteRowVm::status);
    h.RegisterMember("bar", &RemoteRowVm::bar);
    h.RegisterMember("bar_text", &RemoteRowVm::bar_text);
    h.RegisterMember("installed", &RemoteRowVm::installed);
    h.RegisterMember("show_bar", &RemoteRowVm::show_bar);
    h.RegisterMember("show_detail", &RemoteRowVm::show_detail);
  }
  c.RegisterArray<Rml::Vector<RemoteRowVm>>();
  if (auto h = c.RegisterStruct<RemoteGroupVm>()) {
    h.RegisterMember("type", &RemoteGroupVm::type);
    h.RegisterMember("header", &RemoteGroupVm::header);
    h.RegisterMember("open", &RemoteGroupVm::open);
    h.RegisterMember("rows", &RemoteGroupVm::rows);
  }
  c.RegisterArray<Rml::Vector<RemoteGroupVm>>();
  c.Bind("supported_versions", &supported_versions_);
  c.Bind("download_note", &download_note_);
  if (auto h = c.RegisterStruct<InstalledVm>()) {
    h.RegisterMember("id", &InstalledVm::id);
    h.RegisterMember("jar", &InstalledVm::jar);
  }
  c.RegisterArray<Rml::Vector<InstalledVm>>();
  c.Bind("installed_games", &installed_games_);
  c.Bind("message_title", &message_title_);
  c.Bind("message_text", &message_text_);
  if (auto h = c.RegisterStruct<OverrideFormVm>()) {
    h.RegisterMember("title", &OverrideFormVm::title);
    h.RegisterMember("thickness", &OverrideFormVm::thickness);
    h.RegisterMember("outline_size", &OverrideFormVm::outline_size);
    h.RegisterMember("outline_color", &OverrideFormVm::outline_color);
    h.RegisterMember("error", &OverrideFormVm::error);
    h.RegisterMember("unenchanted", &OverrideFormVm::unenchanted);
    h.RegisterMember("unshaded", &OverrideFormVm::unshaded);
    h.RegisterMember("merge", &OverrideFormVm::merge);
  }
  c.Bind("override_form", &override_form_);
  if (auto h = c.RegisterStruct<CtxItemVm>()) {
    h.RegisterMember("label", &CtxItemVm::label);
    h.RegisterMember("action", &CtxItemVm::action);
    h.RegisterMember("enabled", &CtxItemVm::enabled);
  }
  c.RegisterArray<Rml::Vector<CtxItemVm>>();
  c.Bind("ctx_items", &ctx_items_);
  c.Bind("ctx_open", &ctx_open_);
  c.Bind("ctx_x", &ctx_x_);
  c.Bind("ctx_y", &ctx_y_);

  c.Bind("remote_groups", &remote_groups_);
  c.Bind("remote_query", &remote_query_);
  c.Bind("remote_summary", &remote_summary_);
  c.Bind("remote_snapshots", &remote_snapshots_);
  c.Bind("is_pack", &is_pack_);
  c.Bind("has_project", &has_project_);
  c.Bind("app_form", &app_form_);
  c.Bind("project_form", &project_form_);
  c.Bind("versions", &versions_);
  c.Bind("versions_collapsed", &versions_collapsed_);
  c.Bind("created_groups", &created_groups_);
  c.Bind("created_collapsed", &created_collapsed_);
  c.Bind("created_label", &created_label_);
  c.Bind("can_download", &can_download_);
  c.Bind("download_hint", &download_hint_);
  c.Bind("sections", &sections_);
  c.Bind("group_label", &group_label_);
  c.Bind("debug_visible", &debug_visible_);
  c.Bind("busy", &busy_);
  c.Bind("tip_text", &tip_text_);
  c.Bind("tip_visible", &tip_visible_);
  c.Bind("tip_x", &tip_x_);
  c.Bind("tip_y", &tip_y_);
  c.Bind("busy_text", &busy_text_);
  c.Bind("debug_text", &debug_text_);
  if (auto h = c.RegisterStruct<GridRowVm>()) {
    h.RegisterMember("key", &GridRowVm::key);
    h.RegisterMember("label", &GridRowVm::label);
    h.RegisterMember("ns_id", &GridRowVm::ns_id);
    h.RegisterMember("collapse_key", &GridRowVm::collapse_key);
    h.RegisterMember("is_header", &GridRowVm::is_header);
    h.RegisterMember("is_cards", &GridRowVm::is_cards);
    h.RegisterMember("is_empty", &GridRowVm::is_empty);
    h.RegisterMember("open", &GridRowVm::open);
    h.RegisterMember("cards", &GridRowVm::cards);
  }
  c.RegisterArray<Rml::Vector<GridRowVm>>();
  c.Bind("grid_window", &grid_window_);
  c.Bind("grid_top", &grid_top_);
  c.Bind("grid_bottom", &grid_bottom_);
  c.Bind("no_match", &no_match_);
  c.Bind("no_match_text", &no_match_text_);
  c.Bind("sort_open", &sort_open_);
  c.Bind("sort_options", &sort_options_);
  c.Bind("namespaces", &namespaces_);
  c.Bind("item_form", &item_form_);
  c.Bind("ns_form", &ns_form_);
  c.Bind("toast_visible", &toast_visible_);
  c.Bind("toast_text", &toast_text_);

  // Bind the search box directly to the store.
  c.BindFunc(
      "query", [this](Rml::Variant &v) { v = store_.state().query; },
      [this](const Rml::Variant &v) { store_.setQuery(v.Get<Rml::String>()); });

  BindEvents(c);
  model_ = c.GetModelHandle();
  return true;
}

void SelectorUi::BindEvents(Rml::DataModelConstructor &c) {
  using Args = const Rml::VariantList &;
  auto on = [&](const char *name, std::function<void(Rml::Event &, Args)> fn) {
    c.BindEventCallback(
        name, [fn = std::move(fn)](Rml::DataModelHandle, Rml::Event &ev,
                                   Args args) { fn(ev, args); });
  };

  on("toggle_version",
     [this](Rml::Event &, Args a) { store_.toggleVersion(Arg0(a)); });
  on("toggle_item",
     [this](Rml::Event &, Args a) { store_.toggleItem(Arg0(a)); });
  on("remove_item",
     [this](Rml::Event &, Args a) { store_.removeItem(Arg0(a)); });
  on("toggle_collapsed",
     [this](Rml::Event &, Args a) { store_.toggleCollapsed(Arg0(a)); });
  on("open_link", [this](Rml::Event &, Args a) {
    if (cb_.open_link) cb_.open_link(Arg0(a));
  });
  on("download", [this](Rml::Event &, Args) {
    if (!can_download_) {
      ShowMessage("Nothing to export",
                  download_hint_.empty()
                      ? std::string("Select at least one item first.")
                      : std::string(download_hint_));
      return;
    }
    Download();
  });

  on("toggle_sort_menu", [this](Rml::Event &, Args) {
    sort_open_ = !sort_open_;
    Dirty("sort_open");
  });
  on("set_sort", [this](Rml::Event &, Args a) {
    if (auto key = sortKeyFromString(Arg0(a))) store_.setSort(*key);
    sort_open_ = false;
    Dirty("sort_open");
  });
  // RmlUi has no outside-click event, so close the sort menu on other clicks.
  on("body_click", [this](Rml::Event &ev, Args) {
    Rml::Element *target = ev.GetTargetElement();
    if (sort_open_ && !(target && target->Closest(".sortrow"))) {
      sort_open_ = false;
      Dirty("sort_open");
    }
    if (ctx_open_ && !(target && target->Closest(".ctx-menu"))) {
      ctx_open_ = false;
      Dirty("ctx_open");
    }
    if (!menu_open_.empty() && !(target && target->Closest(".menu-group"))) {
      menu_open_ = Rml::String();
      Dirty("menu_open");
    }
  });

  on("toggle_menu", [this](Rml::Event &, Args a) {
    const Rml::String name = Arg0(a);
    menu_open_ = menu_open_ == name ? Rml::String() : name;
    Dirty("menu_open");
  });
  on("menu_action", [this](Rml::Event &, Args a) {
    menu_open_ = Rml::String();
    Dirty("menu_open");
    const std::string action = Arg0(a);
    const std::string arg =
        a.size() > 1 ? a[1].Get<Rml::String>() : std::string();
    // Disabled entries do nothing; export errors are reported by the action.
    if (!MenuActionEnabled(action, arg)) {
      if (action == "export_pack") {
        ShowMessage("Nothing to export",
                    "Select at least one item first, or open a pack project.");
      }
      return;
    }
    if (cb_.menu_action) cb_.menu_action(action, arg);
  });
  on("toggle_app_setting",
     [this](Rml::Event &, Args a) { ToggleAppSetting(Arg0(a)); });
  on("toggle_override",
     [this](Rml::Event &, Args a) { ToggleOverride(Arg0(a)); });
  on("context_click", [this](Rml::Event &ev, Args a) {
    if (ev.GetParameter<int>("button", 0) != 1)
      return; // right button only (RmlUi: 0 left, 1 right)
    const std::string kind =
        a.size() > 1 ? a[1].Get<Rml::String>() : std::string();
    OpenContextMenu(kind, Arg0(a), ev.GetParameter<float>("mouse_x", 0.f),
                    ev.GetParameter<float>("mouse_y", 0.f));
  });
  on("ctx_action", [this](Rml::Event &, Args a) { RunContextAction(Arg0(a)); });
  on("show_tip", [this](Rml::Event &ev, Args a) {
    const std::string text = TipText(Arg0(a));
    tip_visible_ = !text.empty();
    tip_text_ = Rml::String(text);
    tip_x_ = Rml::String(
        std::to_string(
            static_cast<int>(ev.GetParameter<float>("mouse_x", 0.f)) + 14) +
        "px");
    tip_y_ = Rml::String(
        std::to_string(
            static_cast<int>(ev.GetParameter<float>("mouse_y", 0.f)) + 20) +
        "px");
    Dirty("tip_visible");
    Dirty("tip_text");
    Dirty("tip_x");
    Dirty("tip_y");
  });

  on("hide_tip", [this](Rml::Event &, Args) {
    tip_visible_ = false;
    Dirty("tip_visible");
  });

  on("item_options_submit", [this](Rml::Event &, Args) {
    ItemOverrideValues values;
    try {
      size_t used = 0;
      values.thicknessPx = std::stod(override_form_.thickness, &used);
      if (used != override_form_.thickness.size() ||
          !(values.thicknessPx > 0.0))
        throw std::invalid_argument("t");
      values.outlineSizePx = std::stod(override_form_.outline_size, &used);
      if (used != override_form_.outline_size.size() ||
          values.outlineSizePx < 0.0)
        throw std::invalid_argument("o");
    } catch (const std::exception &) {
      override_form_.error =
          "Thickness must be above 0, and outline size 0 or more.";
      Dirty("override_form");
      return;
    }
    const std::string color = override_form_.outline_color;
    if (!enchantment_tweaks::text::isHexColor(color)) {
      override_form_.error =
          "Outline color must be six hex digits, like ffffff.";
      Dirty("override_form");
      return;
    }
    values.outlineColor = color;
    values.unenchantedOutline = override_form_.unenchanted;
    values.unenchantedUnshaded = override_form_.unshaded;
    values.mergeColumns = override_form_.merge;
    if (cb_.item_override_changed)
      cb_.item_override_changed(override_targets_, values, false);
    CloseDialog();
  });
  on("item_options_reset", [this](Rml::Event &, Args) {
    if (cb_.item_override_changed)
      cb_.item_override_changed(override_targets_, ItemOverrideValues{}, true);
    CloseDialog();
  });
  on("message_ok", [this](Rml::Event &, Args) { CloseDialog(); });
  on("remote_search_changed", [this](Rml::Event &ev, Args) {
    remote_query_ = ev.GetParameter("value", Rml::String());
    if (cb_.menu_action) cb_.menu_action("remote_search", remote_query_);
  });
  on("open_app_settings_button", [this](Rml::Event &, Args) {
    if (cb_.menu_action) cb_.menu_action("open_app_settings", std::string());
  });
  on("open_project_settings_button", [this](Rml::Event &, Args) {
    if (cb_.menu_action)
      cb_.menu_action("open_project_settings", std::string());
  });
  on("toggle_project_setting",
     [this](Rml::Event &, Args a) { ToggleProjectSetting(Arg0(a)); });
  on("submit_app_settings",
     [this](Rml::Event &, Args) { SubmitAppSettings(); });
  on("submit_project_settings",
     [this](Rml::Event &, Args) { SubmitProjectSettings(); });

  on("open_item_dialog", [this](Rml::Event &, Args) { OpenItemDialog(); });
  on("open_ns_dialog", [this](Rml::Event &, Args) { OpenNamespaceDialog(); });

  on("close_dialog", [this](Rml::Event &, Args) { CloseDialog(); });

  on("dialog_backdrop", [this](Rml::Event &ev, Args) {
    if (ev.GetTargetElement() == ev.GetCurrentElement())
      CloseDialog(); // only clicks on the dimmed area
  });
  // Keep the ID in sync with the name until the user edits it.
  on("item_name_changed", [this](Rml::Event &ev, Args) {
    item_form_.name = ev.GetParameter("value", Rml::String());
    if (!id_touched_) {
      item_form_.id = slugify(item_form_.name);
      Dirty("item_form");
    }
  });
  on("item_id_changed", [this](Rml::Event &ev, Args) {
    item_form_.id = ev.GetParameter("value", Rml::String());
    if (item_form_.id != slugify(item_form_.name)) id_touched_ = true;
  });
  on("pick_icon", [this](Rml::Event &, Args) {
    if (!cb_.pick_icon) return;
    if (auto path = cb_.pick_icon()) {
      icon_path_ = *path;
      item_form_.icon_name = fs::path(*path).filename().string();
      Dirty("item_form");
    }
  });
  on("submit_item", [this](Rml::Event &, Args) { SubmitItem(); });
  on("submit_ns", [this](Rml::Event &, Args) { SubmitNamespace(); });
}

void SelectorUi::Refresh() {
  const State &s = store_.state();

  versions_.clear();
  for (const auto &v : s.available_versions)
    versions_.push_back({.label = v, .on = s.versions.count(v) > 0});
  versions_collapsed_ = s.collapsed.count(collapse::VersionsPanel()) > 0;
  created_collapsed_ = s.collapsed.count(collapse::CreatedPanel()) > 0;

  created_groups_.clear();
  for (const auto &g : store_.getCreatedGroups()) {
    GroupVm vm{.ns_id = g.ns.id,
               .label = g.ns.label,
               .collapse_key = collapse::CreatedGroup(g.ns.id),
               .open = g.open,
               .chips = {}};
    for (const auto &i : g.items)
      vm.chips.push_back({.key = i.Key(), .name = i.name});
    created_groups_.push_back(std::move(vm));
  }

  // Keep sections and cards in place while filtering; toggle their visibility.
  const CatalogView view = store_.getCatalogView();
  sections_.clear();
  for (const auto &sec : view.sections) {
    SectionVm vm{.ns_id = sec.ns.id,
                 .label = sec.ns.label,
                 .collapse_key = collapse::Grid(sec.ns.id),
                 .open = sec.open,
                 .empty = sec.items.empty(),
                 .visible = !view.filtering || sec.visible_count > 0,
                 .cards = {}};
    for (size_t k = 0; k < sec.items.size(); ++k) {
      const Item &i = sec.items[k];
      vm.cards.push_back({.key = i.Key(),
                          .name = i.name,
                          .id = i.id,
                          .icon = ResolveIcon(i.icon),
                          .selected = s.created.count(i.Key()) > 0,
                          .visible = sec.visible[k] != 0});
    }
    if (group_mode_ == "none") {
      sections_.push_back(std::move(vm));
      continue;
    }

    const enchantment_tweaks::catalog::GroupMode mode =
        enchantment_tweaks::catalog::groupModeFromString(group_mode_);

    std::map<std::string, std::vector<CardVm>> byLabel;

    for (const auto &card : vm.cards) {
      byLabel[enchantment_tweaks::catalog::groupLabelFor(std::string(card.name),
                                                         mode)]
          .push_back(card);
    }

    const auto &keywords =
        mode == enchantment_tweaks::catalog::GroupMode::Prefix
            ? enchantment_tweaks::catalog::prefixKeywords()
            : enchantment_tweaks::catalog::suffixKeywords();

    std::vector<std::string> order = keywords;

    order.emplace_back("Other");
    for (const auto &label : order) {
      const auto found = byLabel.find(label);
      if (found == byLabel.end()) continue;
      const std::string key = collapse::Grid(sec.ns.id) + "#" + label;
      SectionVm sub;

      sub.ns_id = sec.ns.id;
      sub.label = label;
      sub.collapse_key = key;
      sub.open = s.collapsed.count(key) == 0;
      sub.empty = false;

      sub.visible = !view.filtering ||
                    std::ranges::any_of(found->second, [](const CardVm &c) {
                      return c.visible;
                    });
      sub.cards = found->second;

      sections_.push_back(std::move(sub));
    }
  }
  section_keys_.clear();
  for (const auto &sec : sections_) {
    std::vector<std::string> keys;
    for (const auto &card : sec.cards) {
      if (card.visible) keys.push_back(std::string(card.key));
    }
    section_keys_[sec.collapse_key] = std::move(keys);
  }
  BuildGridRows();
  UpdateGridWindow();
  no_match_ = view.filtering && view.total == 0;
  no_match_text_ =
      no_match_
          ? std::string("No items match ") + kQuote + view.query + kUnquote +
                ". Try a different name or ID, or add it with Add Item."
          : "";

  created_label_ = std::to_string(s.created.size()) + " created";
  can_download_ = !s.created.empty() && !s.versions.empty();
  download_hint_ = s.created.empty()    ? "Select at least one item first"
                   : s.versions.empty() ? "Select at least one version first"
                                        : "";

  static const struct {
    SortKey key;
    const char *label;
  } kSorts[] = {{.key = SortKey::NameAsc, .label = "Name (A-Z)"},
                {.key = SortKey::NameDesc,
                 .label = "Name (Z-"
                          "A)"},
                {.key = SortKey::Id, .label = "Item ID"},
                {.key = SortKey::Created, .label = "Created first"}};
  sort_options_.clear();
  for (const auto &o : kSorts)
    sort_options_.push_back(
        {.key = toString(o.key), .label = o.label, .checked = o.key == s.sort});

  namespaces_.clear();
  for (const auto &n : s.namespaces)
    namespaces_.push_back({n.id, n.label});

  // The search box owns `query`; rewriting it would move the caret.
  for (const char *name :
       {"versions", "versions_collapsed", "created_groups", "created_collapsed",
        "created_label", "can_download", "download_hint", "sections",
        "no_match", "no_match_text", "sort_options", "namespaces"})
    Dirty(name);
}

void SelectorUi::DialogKeys::ProcessEvent(Rml::Event &event) {
  const int key = event.GetParameter("key_identifier", 0);
  if (key == Rml::Input::KI_ESCAPE) {
    ui.CloseDialog();
    event.StopPropagation();
  } else if (key == Rml::Input::KI_RETURN ||
             key == Rml::Input::KI_NUMPADENTER) {
    // Submit on Enter only in text fields; preserve button activation.
    Rml::Element *target = event.GetTargetElement();
    if (!target || target->GetTagName() != "input") return;
    if (ui.active_dialog_ == ui.item_doc_)
      ui.SubmitItem();
    else if (ui.active_dialog_ == ui.settings_doc_)
      ui.SubmitAppSettings();
    else if (ui.active_dialog_ == ui.project_settings_doc_)
      ui.SubmitProjectSettings();
    else if (ui.active_dialog_ == ui.ns_doc_)
      ui.SubmitNamespace();
    event.StopPropagation();
  }
}

void SelectorUi::BuildGridRows() {
  grid_all_.clear();
  const int cols = std::max(1, grid_cols_);
  for (const auto &sec : sections_) {
    if (!sec.visible) continue;
    GridRowVm header;
    header.key = "h:" + sec.ns_id;
    header.label = sec.label;
    header.ns_id = sec.ns_id;
    header.collapse_key = sec.collapse_key;
    header.is_header = true;
    header.open = sec.open;
    grid_all_.push_back(std::move(header));
    if (!sec.open) continue;
    if (sec.empty) {
      GridRowVm notice;
      notice.key = "e:" + sec.ns_id;
      notice.is_empty = true;
      grid_all_.push_back(std::move(notice));
      continue;
    }
    std::vector<CardVm> visible;
    for (const auto &card : sec.cards) {
      if (card.visible) visible.push_back(card);
    }
    for (size_t start = 0; start < visible.size();
         start += static_cast<size_t>(cols)) {
      GridRowVm row;
      row.key = "c:" + sec.ns_id + ":" + std::to_string(start);
      row.is_cards = true;
      for (size_t k = start;
           k < visible.size() && k < start + static_cast<size_t>(cols); ++k) {
        row.cards.push_back(visible[k]);
      }
      // Keep row sizes stable while RmlUi is using the list.
      while (row.cards.size() < static_cast<size_t>(cols)) {
        CardVm spare;
        spare.visible = false;
        row.cards.push_back(std::move(spare));
      }
      grid_all_.push_back(std::move(row));
    }
  }
  grid_offsets_.assign(grid_all_.size() + 1, 0.0);
  for (size_t i = 0; i < grid_all_.size(); ++i) {
    grid_offsets_[i + 1] = grid_offsets_[i] + rowHeight(grid_all_[i].is_header,
                                                        grid_all_[i].is_cards);
  }
}

void SelectorUi::UpdateGridWindow() {
  const double scroll = main_el_ ? main_el_->GetScrollTop() : 0.0;
  const double viewport = main_el_ ? main_el_->GetClientHeight() : 800.0;
  grid_scroll_ = scroll;
  grid_viewport_ = viewport;

  const double first_y = std::max(0.0, scroll - kOverscanDp);
  const double last_y = scroll + viewport + kOverscanDp;
  // Find the visible window in the sorted row offsets.
  size_t first = static_cast<size_t>(
      std::upper_bound(grid_offsets_.begin(), grid_offsets_.end(), first_y) -
      grid_offsets_.begin());
  first = first == 0 ? 0 : first - 1;
  size_t last = static_cast<size_t>(
      std::upper_bound(grid_offsets_.begin(), grid_offsets_.end(), last_y) -
      grid_offsets_.begin());
  last = std::min(last, grid_all_.size());
  if (first > last) first = last;

  grid_window_.clear();
  for (size_t i = first; i < last; ++i) {
    const GridRowVm &row = grid_all_[i];
    GridRowVm copy;
    copy.key = row.key;
    copy.label = row.label;
    copy.ns_id = row.ns_id;
    copy.collapse_key = row.collapse_key;
    copy.is_header = row.is_header;
    copy.is_cards = row.is_cards;
    copy.is_empty = row.is_empty;
    copy.open = row.open;
    copy.cards = row.cards;
    grid_window_.push_back(std::move(copy));
  }
  grid_drawn_ = last - first;
  // Keep the window just large enough for the view, growing capacity in steps
  // of eight without shrinking the RmlUi-bound list.
  if (grid_drawn_ > grid_capacity_) grid_capacity_ = (grid_drawn_ + 7) / 8 * 8;
  while (grid_window_.size() < grid_capacity_)
    grid_window_.push_back(GridRowVm{});
  const size_t slots = static_cast<size_t>(std::max(1, grid_cols_));
  for (auto &row : grid_window_) {
    while (row.cards.size() < slots) {
      CardVm spare;
      spare.visible = false;
      row.cards.push_back(std::move(spare));
    }
  }
  const double top = grid_offsets_.empty() ? 0.0 : grid_offsets_[first];
  const double total = grid_offsets_.empty() ? 0.0 : grid_offsets_.back();
  const double bottom =
      grid_offsets_.empty() ? 0.0 : total - grid_offsets_[last];
  grid_top_ = std::to_string(static_cast<long>(top)) + "dp";
  grid_bottom_ =
      std::to_string(static_cast<long>(std::max(0.0, bottom))) + "dp";
  Dirty("grid_window");
  Dirty("grid_top");
  Dirty("grid_bottom");
}

void SelectorUi::UpdateGridColumns() {
  if (!main_el_) return;
  const double width =
      std::max(0.0, static_cast<double>(main_el_->GetClientWidth()) -
                        kGridSidePaddingDp);
  const int cols = std::max(1, static_cast<int>((width + 12.0) / kCardPitchDp));
  if (cols != grid_cols_) {
    grid_cols_ = cols;
    BuildGridRows();
    UpdateGridWindow();
  }
}

void SelectorUi::Update(double dt) {
  // Update the visible window after scrolling or resizing.
  if (!main_el_ && main_doc_) main_el_ = main_doc_->QuerySelector(".main");
  if (main_el_) {
    UpdateGridColumns();
    const double scroll = main_el_->GetScrollTop();
    const double viewport = main_el_->GetClientHeight();
    if (std::abs(scroll - grid_scroll_) > 0.5 ||
        std::abs(viewport - grid_viewport_) > 0.5) {
      UpdateGridWindow();
    }
  }
  if (!toast_visible_) return;
  toast_remaining_ -= dt;
  if (toast_remaining_ <= 0) {
    toast_visible_ = false;
    Dirty("toast_visible");
  }
}

double SelectorUi::NextUpdateDelay() const {
  return toast_visible_ ? std::max(0.0, toast_remaining_)
                        : std::numeric_limits<double>::infinity();
}

void SelectorUi::ShowToast(const std::string &text, double seconds) {
  toast_text_ = text;
  toast_visible_ = true;
  toast_remaining_ = seconds;
  Dirty("toast_text");
  Dirty("toast_visible");
}

void SelectorUi::ShowDialog(Rml::ElementDocument *doc, const char *focus_id) {
  if (!doc) return;
  if (active_dialog_ && active_dialog_ != doc) active_dialog_->Hide();
  active_dialog_ = doc;
  doc->Show(Rml::ModalFlag::Modal, Rml::FocusFlag::Document);
  if (Rml::Element *el = doc->GetElementById(focus_id)) el->Focus();
}

void SelectorUi::CloseDialog() {
  if (!active_dialog_) return;
  active_dialog_->Hide();
  active_dialog_ = nullptr;
}

void SelectorUi::OpenItemDialog() {
  item_form_ = {};
  item_form_.ns = store_.state().namespaces.empty()
                      ? ""
                      : store_.state().namespaces.front().id;
  icon_path_.clear();
  id_touched_ = false;
  Dirty("item_form");
  Dirty("namespaces");
  ShowDialog(item_doc_, "item-name");
}

void SelectorUi::OpenNamespaceDialog() {
  ns_form_ = {};
  Dirty("ns_form");
  ShowDialog(ns_doc_, "ns-label");
}

void SelectorUi::SubmitItem() {
  if (auto problem = validateItemDraft(item_form_.ns, item_form_.id,
                                       item_form_.name, store_.state().items)) {
    item_form_.error = *problem;
    Dirty("item_form");
    return;
  }
  Item item{.ns = item_form_.ns,
            .id = trim(item_form_.id),
            .name = trim(item_form_.name),
            .icon = icon_path_,
            .custom = true};
  if (!store_.addItem(
          item)) { // validation passed, so this is a namespace that vanished
    item_form_.error = "Could not add the item.";
    Dirty("item_form");
    return;
  }
  if (cb_.item_added) cb_.item_added(item);
  CloseDialog();
  ShowToast("Added " + item.name);
}

void SelectorUi::SubmitNamespace() {
  if (auto problem = validateNamespaceDraft(ns_form_.label, ns_form_.id,
                                            store_.state().namespaces)) {
    ns_form_.error = *problem;
    Dirty("ns_form");
    return;
  }
  Namespace ns{
      .id = trim(ns_form_.id), .label = trim(ns_form_.label), .custom = true};
  if (!store_.addNamespace(ns)) {
    ns_form_.error = "Could not add the namespace.";
    Dirty("ns_form");
    return;
  }
  if (cb_.namespace_added) cb_.namespace_added(ns);
  CloseDialog();
  ShowToast("Added namespace " + ns.id);
}

void SelectorUi::Download() {
  if (!can_download_) return;
  if (!cb_.save_pack) {
    ShowToast("No pack writer configured");
    return;
  }

  SavePackRequest req;
  req.suggested_name = "enchantment-tweaks-pack.zip";
  req.versions = store_.selectedVersions();
  req.items = store_.selectedItems();
  req.files = buildPackFiles(req.versions, req.items);

  const SavePackResult r = cb_.save_pack(req);
  if (!r.error.empty())
    ShowToast("Could not save pack: " + r.error, 6);
  else if (r.saved)
    ShowToast(r.path.empty() ? "Pack saved" : "Pack saved to " + r.path, 6);
}

bool SelectorUi::MenuActionEnabled(const std::string &action,
                                   const std::string &arg) const {
  if (action == "open_project_settings_button") return has_project_;
  if (action.empty()) return false;
  for (const auto &entry : file_menu_) {
    if (entry.action == action && entry.arg == arg) return entry.enabled;
  }
  return true; // actions that are not in the File menu (dialogs, toggles) are
               // always live
}

void SelectorUi::SetMenuData(const std::vector<MenuEntry> &fileMenu,
                             const std::string &status) {
  file_menu_.clear();
  for (const auto &entry : fileMenu) {
    MenuItemVm vm;
    vm.label = entry.label;
    vm.action = entry.action;
    vm.arg = entry.arg;
    vm.enabled = entry.enabled;
    vm.separator = entry.separator;
    vm.shortcut = entry.shortcut;
    file_menu_.push_back(std::move(vm));
  }
  status_text_ = status;
  Dirty("file_menu");
  Dirty("status_text");
}

void SelectorUi::SetSupportedVersions(
    bool isPackProject, const std::vector<VersionChoice> &versions,
    const std::string &range) {
  is_pack_ = isPackProject;
  supported_versions_.clear();
  for (const auto &v : versions) {
    SupportedVm vm;
    vm.id = v.id;
    vm.on = v.on;
    vm.installed = v.installed;
    supported_versions_.push_back(vm);
  }
  project_form_.pack_note =
      isPackProject ? Rml::String("Pack supports these versions. Item data "
                                  "comes from the newest selected one.")
                    : Rml::String();
  project_form_.pack_range = range;
  Dirty("is_pack");
  Dirty("supported_versions");
  Dirty("project_form");
}

void SelectorUi::SetRemoteView(const std::vector<RemoteGroupChoice> &groups,
                               const std::string &note,
                               const std::string &query,
                               const std::string &summary,
                               bool includeSnapshots) {
  remote_groups_.clear();
  for (const auto &g : groups) {
    RemoteGroupVm vm;
    vm.type = g.type;
    std::string header = g.type + "   " + std::to_string(g.matches);
    if (g.matches != g.total) header += " of " + std::to_string(g.total);
    if (g.truncated) header += "  (first shown, refine the search)";
    vm.header = header;
    vm.open = g.open;
    for (const auto &r : g.rows) {
      RemoteRowVm row;
      row.id = r.id;
      row.label = r.id;
      row.button = r.button;
      row.installed = r.installed;
      row.status = r.status;
      row.show_bar = r.showBar;
      row.show_detail = r.button == "retry";
      const int percent = std::max(
          0, std::min(100, static_cast<int>(std::lround(r.fraction * 100.0))));
      row.bar = std::to_string(percent) + "%";
      // The bar text includes the download state and progress.
      row.bar_text =
          r.button == "cancel" && r.status == "Queued"
              ? Rml::String("Queued")
              : Rml::String(
                    (r.status.empty() ? std::string("Downloading") : r.status) +
                    "  " + row.bar);
      vm.rows.push_back(std::move(row));
    }
    remote_groups_.push_back(std::move(vm));
  }
  download_note_ = note;
  remote_query_ = query;
  remote_summary_ = summary;
  remote_snapshots_ = includeSnapshots ? Rml::String("Snapshots: on")
                                       : Rml::String("Snapshots: off");
  Dirty("remote_groups");
  Dirty("download_note");
  Dirty("remote_query");
  Dirty("remote_summary");
  Dirty("remote_snapshots");
}

void SelectorUi::SetInstalledGames(const std::vector<InstalledChoice> &games) {
  installed_games_.clear();
  for (const auto &g : games) {
    InstalledVm vm;
    vm.id = g.id;
    vm.jar = g.jar;
    installed_games_.push_back(std::move(vm));
  }
  Dirty("installed_games");
}

void SelectorUi::OpenPickGameDialog() {
  ShowDialog(pick_game_doc_, "pick-close");
}

void SelectorUi::ShowMessage(const std::string &title,
                             const std::string &text) {
  message_title_ = title;
  message_text_ = text;
  Dirty("message_title");
  Dirty("message_text");
  ShowDialog(message_doc_, "message-ok");
}

void SelectorUi::SetProjectState(bool hasProject) {
  has_project_ = hasProject;
  Dirty("has_project");
}

void SelectorUi::SetItemOverrideForm(const std::string &title,
                                     const ItemOverrideValues &values) {
  override_form_.title = title;
  SetItemOverrideValues(values);
}

void SelectorUi::SetItemOverrideValues(const ItemOverrideValues &values) {
  std::ostringstream thickness, outline;
  thickness << values.thicknessPx;
  outline << values.outlineSizePx;
  override_form_.thickness = thickness.str();
  override_form_.outline_size = outline.str();
  override_form_.outline_color = values.outlineColor;
  override_form_.unenchanted = values.unenchantedOutline;
  override_form_.unshaded = values.unenchantedUnshaded;
  override_form_.merge = values.mergeColumns;
  override_form_.error = Rml::String();
  Dirty("override_form");
}

void SelectorUi::OpenItemOptionsDialog() {
  ShowDialog(item_options_doc_, "set-thickness");
}

void SelectorUi::OpenContextMenu(const std::string &kind,
                                 const std::string &key, float x, float y) {
  ctx_items_.clear();
  ctx_keys_.clear();
  ctx_is_group_ = kind == "group";
  if (!ctx_is_group_) {
    ctx_keys_.push_back(key);
    const bool selected = store_.state().created.count(key) > 0;
    ctx_items_.push_back(
        {Rml::String(selected ? "Deselect" : "Select"), "toggle_select", true});
    ctx_items_.push_back(
        {Rml::String("Outline and thickness..."), "item_options", true});
    ctx_items_.push_back(
        {Rml::String("Reset item overrides"), "item_reset", true});
    ctx_items_.push_back({Rml::String("Preview (3D, planned)"), "", false});
  } else {
    const auto found = section_keys_.find(key);
    if (found == section_keys_.end() || found->second.empty()) return;
    ctx_keys_ = found->second;
    ctx_title_.clear();
    for (const auto &sec : sections_) {
      if (sec.collapse_key == key) ctx_title_ = std::string(sec.label);
    }
    ctx_items_.push_back(
        {Rml::String("Select all in group"), "group_select", true});
    ctx_items_.push_back(
        {Rml::String("Deselect all in group"), "group_deselect", true});
    ctx_items_.push_back({Rml::String("Outline and thickness for group..."),
                          "group_options", true});
    ctx_items_.push_back(
        {Rml::String("Reset overrides for group"), "group_reset", true});
  }
  ctx_x_ = Rml::String(std::to_string(static_cast<int>(x)) + "px");
  ctx_y_ = Rml::String(std::to_string(static_cast<int>(y)) + "px");
  ctx_open_ = true;
  Dirty("ctx_items");
  Dirty("ctx_open");
  Dirty("ctx_x");
  Dirty("ctx_y");
}

void SelectorUi::RunContextAction(const std::string &action) {
  bool wasOpen = ctx_open_;
  ctx_open_ = false;
  Dirty("ctx_open");
  if (!wasOpen) return;
  const auto &selected = store_.state().created;
  if (action == "toggle_select" && !ctx_keys_.empty()) {
    store_.toggleItem(ctx_keys_.front());
  } else if (action == "group_select") {
    std::vector<std::string> keys(selected.begin(), selected.end());
    for (const auto &k : ctx_keys_) {
      if (!selected.count(k)) keys.push_back(k);
    }
    store_.setCreated(keys);
  } else if (action == "group_deselect") {
    std::vector<std::string> keys;
    for (const auto &k : selected) {
      if (std::ranges::find(ctx_keys_, k) == ctx_keys_.end()) keys.push_back(k);
    }
    store_.setCreated(keys);
  } else if (action == "item_options" || action == "group_options") {
    override_targets_ = ctx_keys_;
    override_form_.title =
        Rml::String(ctx_is_group_ ? ("Group: " + ctx_title_)
                                  : std::string("Item overrides"));
    if (cb_.menu_action && !ctx_keys_.empty())
      cb_.menu_action("item_options", ctx_keys_.front());
  } else if (action == "item_reset" || action == "group_reset") {
    if (cb_.item_override_changed)
      cb_.item_override_changed(ctx_keys_, ItemOverrideValues{}, true);
  }
}

bool SelectorUi::TextInputFocused() const {
  Rml::Element *focus = ctx_ ? ctx_->GetFocusElement() : nullptr;
  return focus && focus->GetTagName() == "input";
}

std::vector<std::string> SelectorUi::VisibleItemKeys() const {
  std::vector<std::string> keys;
  for (const auto &entry : section_keys_) {
    keys.insert(keys.end(), entry.second.begin(), entry.second.end());
  }
  return keys;
}

void SelectorUi::ShowUnsavedPrompt() {
  ShowDialog(unsaved_doc_, "unsaved-cancel");
}

void SelectorUi::SetBusy(bool busy, const std::string &text) {
  busy_ = busy;
  busy_text_ = Rml::String(text);
  Dirty("busy");
  Dirty("busy_text");
}

void SelectorUi::SetDebugVisible(bool visible) {
  debug_visible_ = visible;
  Dirty("debug_visible");
}

void SelectorUi::SetDebugText(const std::string &text) {
  debug_text_ = Rml::String(text);
  Dirty("debug_text");
}

SelectorUi::GridStats SelectorUi::GridStatistics() const {
  GridStats out;
  out.totalRows = grid_all_.size();
  out.drawnRows = grid_drawn_;
  out.columns = grid_cols_;
  return out;
}

void SelectorUi::SetGroupMode(const std::string &mode) {
  group_mode_ = enchantment_tweaks::catalog::groupModeToString(
      enchantment_tweaks::catalog::groupModeFromString(mode));
  group_label_ = Rml::String(group_mode_ == "none" ? "Group: off"
                                                   : "Group: " + group_mode_);
  Dirty("group_label");
  Refresh();
}

void SelectorUi::OpenDownloadDialog() {
  ShowDialog(download_doc_, "download-close");
}

void SelectorUi::SetAppSettings(const AppSettingsValues &values) {
  app_form_.display_mode = values.displayMode;
  app_form_.dark = values.darkMode;
  app_form_.error = Rml::String();
  dark_mode_ = values.darkMode;
  Dirty("app_form");
  Dirty("dark_mode");
}

void SelectorUi::SetItemOptions(const ItemOptionsValues &values) {
  std::ostringstream thickness, outline;
  thickness << values.thicknessPx;
  outline << values.outlineSizePx;
  project_form_.thickness = thickness.str();
  project_form_.outline_size = outline.str();
  project_form_.outline_color = values.outlineColor;
  project_form_.unenchanted = values.unenchantedOutline;
  project_form_.unshaded = values.unenchantedUnshaded;
  project_form_.merge = values.mergeColumns;
  project_form_.error = Rml::String();
  Dirty("project_form");
}

void SelectorUi::OpenAppSettingsDialog() {
  ShowDialog(settings_doc_, "set-display");
}

void SelectorUi::OpenProjectSettingsDialog() {
  ShowDialog(project_settings_doc_, "set-thickness");
}

void SelectorUi::ToggleAppSetting(const std::string &name) {
  if (name == "dark")
    app_form_.dark = !app_form_.dark;
  else if (name == "display")
    app_form_.display_mode =
        app_form_.display_mode == "baked" ? "rotating" : "baked";
  Dirty("app_form");
}

std::string SelectorUi::TipText(const std::string &key) {

  /// TODO: Move this text to a resource file.
  if (key == "thickness")
    return "Depth of the item's sprite, in model pixels. A larger value gives "
           "a thicker edge. 1 is the default.";
  if (key == "outline_size")
    return "Width of the outline drawn around the item, in model pixels. 0 "
           "means no outline.";
  if (key == "outline_color")
    return "Color of the outline, as six hex digits (rrggbb). ffffff is white.";
  if (key == "unenchanted")
    return "Also outlines items without an enchantment. When off, unenchanted "
           "items keep their original model.";
  if (key == "unshaded")
    return "The unenchanted outlined model is drawn without shading. Only "
           "applies when unenchanted outlines are on. Enchanted models are "
           "always unshaded.";
  if (key == "merge")
    return "Merges horizontal runs of opaque pixels into one element. The file "
           "is smaller, but it can change the look in some packs. Only "
           "generated items are merged; custom 3D models are never merged.";
  if (key == "custom")
    return "Custom 3D model (experimental). Its elements are kept as they are, "
           "and its textures get unshaded copies for the glow. It is not "
           "outlined.";
  if (key == "dark") return "Color theme for every window.";
  if (key == "group")
    return "Splits each namespace into sub-groups by name: prefix (Wooden, "
           "Iron, Oak...) or suffix (Sword, Axe, Hoe...). Changes only the "
           "display, not the export.";
  if (key == "supported")
    return "The game versions this pack supports. Its item data comes from the "
           "newest selected version.";
  return {};
}

void SelectorUi::ToggleOverride(const std::string &name) {
  if (name == "unenchanted")
    override_form_.unenchanted = !override_form_.unenchanted;
  else if (name == "unshaded")
    override_form_.unshaded = !override_form_.unshaded;
  else if (name == "merge")
    override_form_.merge = !override_form_.merge;
  Dirty("override_form");
}

void SelectorUi::ToggleProjectSetting(const std::string &name) {
  if (name == "unenchanted")
    project_form_.unenchanted = !project_form_.unenchanted;
  else if (name == "unshaded")
    project_form_.unshaded = !project_form_.unshaded;
  else if (name == "merge")
    project_form_.merge = !project_form_.merge;
  Dirty("project_form");
}

void SelectorUi::SubmitAppSettings() {
  AppSettingsValues values;
  values.darkMode = app_form_.dark;
  values.displayMode = app_form_.display_mode;
  if (cb_.app_settings_changed) cb_.app_settings_changed(values);
  CloseDialog();
}

void SelectorUi::SubmitProjectSettings() {
  ItemOptionsValues values;
  try {
    size_t used = 0;
    values.thicknessPx = std::stod(project_form_.thickness, &used);
    if (used != project_form_.thickness.size() || !(values.thicknessPx > 0.0))
      throw std::invalid_argument("thickness");
    values.outlineSizePx = std::stod(project_form_.outline_size, &used);
    if (used != project_form_.outline_size.size() || values.outlineSizePx < 0.0)
      throw std::invalid_argument("outline");
  } catch (const std::exception &) {
    project_form_.error = "Thickness must be a number above 0, and outline "
                          "size a number of 0 or more.";
    Dirty("project_form");
    return;
  }
  const std::string color = project_form_.outline_color;
  if (!enchantment_tweaks::text::isHexColor(color)) {
    project_form_.error = "Outline color must be six hex digits, like ffffff.";
    Dirty("project_form");
    return;
  }
  values.outlineColor = color;
  values.unenchantedOutline = project_form_.unenchanted;
  values.unenchantedUnshaded = project_form_.unshaded;
  values.mergeColumns = project_form_.merge;
  if (cb_.item_options_changed) cb_.item_options_changed(values);
  CloseDialog();
}

} // namespace et
