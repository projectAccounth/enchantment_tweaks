#pragma once
#include "../model/store.h"
#include "../model/types.h"
#include "RmlUi/Config/Config.h"
#include "RmlUi/Core/Context.h"
#include "RmlUi/Core/DataModelHandle.h"
#include "RmlUi/Core/ElementDocument.h"
#include "RmlUi/Core/Event.h"
#include "RmlUi/Core/EventListener.h"
#include "core/project/project_file.hpp"
#include <RmlUi/Core.h>
#include <cstddef>
#include <functional>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace et {

struct SavePackRequest {
  std::string suggested_name;
  std::vector<std::string> versions;
  std::vector<Item> items;
  std::vector<PackFile> files; // the pack
};
struct SavePackResult {
  bool saved = false;
  std::string path; // shown in the toast when set
  std::string error;
};

// TODO: Move this helper out of the UI header.
bool IsExistingAbsoluteFile(const std::string &path);

using AppSettingsValues = enchantment_tweaks::project::AppSettings;
using ItemOptionsValues = enchantment_tweaks::project::ItemOptions;
using ItemOverrideValues = enchantment_tweaks::project::ItemOptions;

struct MenuEntry {
  std::string label;
  std::string action;
  std::string arg;
  bool enabled = true;
  bool separator = false;
  std::string shortcut; // shown on the right
};

struct UiCallbacks {
  std::function<SavePackResult(const SavePackRequest &)> save_pack;
  std::function<std::optional<std::string>()>
      pick_icon; // open a native file dialog, return the path
  std::function<void(const PersistedState &)>
      persist; // called on every persistable change
  std::function<void(const std::vector<std::string> &versions,
                     const std::set<std::string> &created)>
      selection_changed;
  std::function<void(const std::string &linkId)> open_link; // "home" ...
  std::function<void(const Item &)> item_added;
  std::function<void(const Namespace &)> namespace_added;
  std::function<void(const std::string &action, const std::string &arg)>
      menu_action;

  std::function<void(const AppSettingsValues &)> app_settings_changed;
  std::function<void(const std::vector<std::string> &keys,
                     const ItemOverrideValues &values, bool reset)>
      item_override_changed;
  std::function<void(const ItemOptionsValues &)> item_options_changed;
};

class SelectorUi {
public:
  SelectorUi(Rml::Context *context, Store &store, UiCallbacks callbacks,
             std::string assetDir);
  ~SelectorUi();
  SelectorUi(const SelectorUi &) = delete;
  SelectorUi &operator=(const SelectorUi &) = delete;

  /// Initializes the UI and its data model.
  bool Initialize();
  void Update(double dt);
  /// Returns the delay until another update is needed.
  double NextUpdateDelay() const;

  void ShowToast(const std::string &text, double seconds = 2.4);
  void OpenItemDialog();
  void OpenNamespaceDialog();
  void CloseDialog();

  /// Returns true while a text field has focus.
  bool TextInputFocused() const;
  /// Returns visible item keys.
  std::vector<std::string> VisibleItemKeys() const;
  /// Replaces the File menu and status text.
  void SetMenuData(const std::vector<MenuEntry> &fileMenu,
                   const std::string &status);
  /// Returns whether a menu action is enabled.
  bool MenuActionEnabled(const std::string &action,
                         const std::string &arg) const;

  struct VersionChoice {
    std::string id;
    bool on = false;
    bool installed = false;
  };
  void SetSupportedVersions(bool isPackProject,
                            const std::vector<VersionChoice> &versions,
                            const std::string &range = {});
  /// One version row in the download dialog.
  struct RemoteRowChoice {
    std::string id;
    std::string type;
    std::string button = "download";
    bool installed = false;
    std::string status;
    bool showBar = false;
    double fraction = 0.0;
  };
  struct RemoteGroupChoice {
    std::string type;
    std::size_t total = 0;
    std::size_t matches = 0;
    bool open = false;
    bool truncated = false;
    std::vector<RemoteRowChoice> rows;
  };
  void SetRemoteView(const std::vector<RemoteGroupChoice> &groups,
                     const std::string &note, const std::string &query,
                     const std::string &summary, bool includeSnapshots);

  void OpenDownloadDialog();
  void ShowUnsavedPrompt();
  void SetGroupMode(const std::string &mode);
  void SetItemOverrideForm(const std::string &title,
                           const ItemOverrideValues &values);
  void SetItemOverrideValues(const ItemOverrideValues &values);
  void OpenItemOptionsDialog();
  /// Updates whether a project is open.
  void SetProjectState(bool hasProject);
  void ShowMessage(const std::string &title, const std::string &text);
  void SetBusy(bool busy, const std::string &text);
  void SetDebugVisible(bool visible);
  void SetDebugText(const std::string &text);
  struct GridStats {
    std::size_t totalRows = 0; // every row of the grid, in or out of view
    std::size_t drawnRows = 0; // rows in the document right now
    int columns = 0;
  };
  GridStats GridStatistics() const;
  /// Installed game versions shown in the new-project picker.
  struct InstalledChoice {
    std::string id;
    std::string jar;
  };
  void SetInstalledGames(const std::vector<InstalledChoice> &games);
  void OpenPickGameDialog();
  /// Updates settings and applies the theme.
  void SetAppSettings(const AppSettingsValues &values);
  /// Updates the project's item options.
  void SetItemOptions(const ItemOptionsValues &values);
  void OpenAppSettingsDialog();
  void OpenProjectSettingsDialog();

  Rml::ElementDocument *MainDocument() const { return main_doc_; }
  Rml::ElementDocument *ActiveDialog() const { return active_dialog_; }

private:
  struct VersionVm {
    Rml::String label;
    bool on = false;
  };
  struct CardVm {
    Rml::String key, name, id, icon;
    bool selected = false;
    bool visible = true;
  };
  struct SectionVm {
    Rml::String ns_id, label, collapse_key;
    bool open = true;
    bool empty = false;
    bool visible = true;
    Rml::Vector<CardVm> cards;
  };
  // A visible section header, card row, or empty-state notice.
  struct GridRowVm {
    Rml::String key, label, ns_id, collapse_key;
    bool is_header = false;
    bool is_cards = false;
    bool is_empty = false;
    bool open = false;
    Rml::Vector<CardVm> cards;
  };
  struct ChipVm {
    Rml::String key, name;
  };
  struct GroupVm {
    Rml::String ns_id, label, collapse_key;
    bool open = true;
    Rml::Vector<ChipVm> chips;
  };
  struct SortOptionVm {
    Rml::String key, label;
    bool checked = false;
  };
  struct NamespaceOptionVm {
    Rml::String id, label;
  };
  struct ItemFormVm {
    Rml::String name, id, ns, icon_name, error;
  };
  struct NamespaceFormVm {
    Rml::String label, id, error;
  };
  struct MenuItemVm {
    Rml::String label, action, arg, shortcut;
    bool enabled = true;
    bool separator = false;
  };
  struct AppFormVm {
    Rml::String display_mode, error;
    bool dark = false;
  };
  struct ProjectFormVm {
    Rml::String thickness, outline_size, outline_color, error, pack_note,
        pack_range;
    bool unenchanted = false, unshaded = false, merge = false;
  };
  void ToggleAppSetting(const std::string &name);
  void ToggleOverride(const std::string &name);
  void ToggleProjectSetting(const std::string &name);
  void SubmitAppSettings();
  void SubmitProjectSettings();

  Rml::String ResolveIcon(const std::string &icon) const;
  bool RegisterModel();
  void BindEvents(Rml::DataModelConstructor &c);
  void Refresh();
  void Dirty(const char *name) { model_.DirtyVariable(name); }

  void Download();
  void SubmitItem();
  void SubmitNamespace();
  void ShowDialog(Rml::ElementDocument *doc, const char *focus_id);

  struct DialogKeys : Rml::EventListener {
    explicit DialogKeys(SelectorUi &owner) : ui(owner) {}
    void ProcessEvent(Rml::Event &event) override;
    SelectorUi &ui;
  };

  Rml::Context *ctx_;
  Store &store_;
  UiCallbacks cb_;
  std::string asset_dir_;
  Rml::DataModelHandle model_;

  Rml::ElementDocument *pick_game_doc_ = nullptr;
  Rml::ElementDocument *message_doc_ = nullptr;
  Rml::ElementDocument *unsaved_doc_ = nullptr;
  Rml::ElementDocument *item_options_doc_ = nullptr;

  Rml::String message_title_, message_text_;

  struct OverrideFormVm {
    Rml::String title, thickness, outline_size, outline_color, error;
    bool unenchanted = false, unshaded = false, merge = false;
  };
  Rml::String tip_text_, tip_x_ = "0px", tip_y_ = "0px";
  bool tip_visible_ = false;
  static std::string TipText(const std::string &key);
  OverrideFormVm override_form_;
  std::vector<std::string> override_targets_;

  struct CtxItemVm {
    Rml::String label, action;
    bool enabled = true;
  };

  Rml::Vector<CtxItemVm> ctx_items_;
  bool ctx_open_ = false;
  Rml::String ctx_x_ = "0px", ctx_y_ = "0px";

  std::vector<std::string>
      ctx_keys_; // the keys the menu acts on (one item, or a group)

  bool ctx_is_group_ = false;
  std::string ctx_title_; // group name, for the group options dialog
  std::map<std::string, std::vector<std::string>> section_keys_;

  void OpenContextMenu(const std::string &kind, const std::string &key, float x,
                       float y);
  void RunContextAction(const std::string &action);

  struct InstalledVm {
    Rml::String id, jar;
  };
  Rml::Vector<InstalledVm> installed_games_;
  Rml::ElementDocument *main_doc_ = nullptr, *item_doc_ = nullptr,
                       *ns_doc_ = nullptr, *settings_doc_ = nullptr,
                       *project_settings_doc_ = nullptr,
                       *download_doc_ = nullptr, *active_dialog_ = nullptr;

  Rml::Vector<VersionVm> versions_;
  Rml::Vector<GroupVm> created_groups_;
  Rml::Vector<SectionVm> sections_;

  void BuildGridRows();
  void UpdateGridWindow();
  void UpdateGridColumns();

  std::vector<GridRowVm> grid_all_;
  std::vector<double>
      grid_offsets_; // top of each row in dp, plus the total at the end
  Rml::Vector<GridRowVm> grid_window_;

  std::size_t grid_drawn_ = 0;
  std::size_t grid_capacity_ = 16; // rows the document keeps
  Rml::String debug_text_;
  Rml::String busy_text_;

  bool busy_ = false;
  bool debug_visible_ = false;

  Rml::String grid_top_, grid_bottom_;
  Rml::Element *main_el_ = nullptr;

  int grid_cols_ = 0;
  double grid_scroll_ = -1.0, grid_viewport_ = -1.0;

  Rml::Vector<SortOptionVm> sort_options_;
  Rml::Vector<NamespaceOptionVm> namespaces_;

  ItemFormVm item_form_;
  NamespaceFormVm ns_form_;

  bool versions_collapsed_ = false, created_collapsed_ = false;
  bool no_match_ = false, can_download_ = false, sort_open_ = false,
       toast_visible_ = false;

  Rml::String no_match_text_, created_label_, download_hint_, toast_text_;

  struct SupportedVm {
    Rml::String id;
    bool on = false;
    bool installed = false;
  };

  struct RemoteRowVm {
    Rml::String id, label, button, status, bar, bar_text;
    bool installed = false;   // shows the "Installed" badge next to the id
    bool show_bar = false;    // the action slot becomes a progress bar
    bool show_detail = false; // the error, under the id, for a failed download
  };
  struct RemoteGroupVm {
    Rml::String type, header;
    bool open = false;
    Rml::Vector<RemoteRowVm> rows;
  };
  Rml::Vector<MenuItemVm> file_menu_;
  Rml::Vector<SupportedVm> supported_versions_;
  Rml::Vector<RemoteGroupVm> remote_groups_;
  Rml::String remote_query_, remote_summary_, remote_snapshots_;

  bool is_pack_ = false;
  bool has_project_ = false;

  Rml::String menu_open_, status_text_, download_note_;
  Rml::String group_label_;
  std::string group_mode_ = "none";

  bool dark_mode_ = false;
  AppFormVm app_form_;
  ProjectFormVm project_form_;

  DialogKeys dialog_keys_{*this};

  bool id_touched_ = false;
  std::string icon_path_;
  double toast_remaining_ = 0;
};

} // namespace et
