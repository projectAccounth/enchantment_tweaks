#pragma once
#include "../model/store.h"
#include "../model/types.h"
#include "RmlUi/Config/Config.h"
#include "RmlUi/Core/Context.h"
#include "RmlUi/Core/DataModelHandle.h"
#include "RmlUi/Core/ElementDocument.h"
#include "RmlUi/Core/Event.h"
#include "RmlUi/Core/EventListener.h"
#include <RmlUi/Core.h>
#include <functional>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace et {

struct SavePackRequest {
  std::string suggested_name;
  std::vector<std::string> versions;
  std::vector<Item> items;
  std::vector<PackFile>
      files; // the pack as text files; zip it, or write a folder
};
struct SavePackResult {
  bool saved = false;
  std::string path; // shown in the toast when set
  std::string
      error; // non-empty => failure; empty with saved=false => user cancelled
};

/// True for an absolute POSIX path that exists on disk (Windows drive paths
/// already work in RmlUi). Use it in your SystemInterface so icons picked by
/// the user can be loaded:
///
///   void JoinPath(Rml::String& out, const Rml::String& document, const
///   Rml::String& path) override {
///       if (et::IsExistingAbsoluteFile(path)) { out = path; return; }  //
///       default would strip the leading '/'
///       Rml::SystemInterface::JoinPath(out, document, path);
///   }
bool IsExistingAbsoluteFile(const std::string &path);

/// Every callback is optional.
struct UiCallbacks {
  std::function<SavePackResult(const SavePackRequest &)> save_pack;
  std::function<std::optional<std::string>()>
      pick_icon; // open a native file dialog, return the path
  std::function<void(const PersistedState &)>
      persist; // called on every persistable change; debounce on your side
  std::function<void(const std::vector<std::string> &versions,
                     const std::set<std::string> &created)>
      selection_changed;
  std::function<void(const std::string &link_id)> open_link; // "home" ...
  std::function<void(const Item &)> item_added;
  std::function<void(const Namespace &)> namespace_added;
};

class SelectorUi {
public:
  /// `asset_dir` contains ui/, icons/, sprites/ (this folder's assets/).
  SelectorUi(Rml::Context *context, Store &store, UiCallbacks callbacks,
             std::string asset_dir);
  ~SelectorUi();
  SelectorUi(const SelectorUi &) = delete;
  SelectorUi &operator=(const SelectorUi &) = delete;

  /// Creates the data model and loads the documents. Returns false (and logs)
  /// on failure.
  bool Initialise();

  /// Call once per frame, before Context::Update(), for toast timing.
  void Update(double dt_seconds);
  /// Time until this UI needs another update. Infinity means user input can
  /// wake it.
  double NextUpdateDelay() const;

  void ShowToast(const std::string &text, double seconds = 2.4);
  void OpenItemDialog();
  void OpenNamespaceDialog();
  void CloseDialog();

  Rml::ElementDocument *MainDocument() const { return main_doc_; }
  Rml::ElementDocument *ActiveDialog() const { return active_dialog_; }

private:
  // ---- view-models: plain structs registered with the data model
  // --------------------------
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

  Rml::String ResolveIcon(const std::string &icon) const;
  bool RegisterModel();
  void BindEvents(Rml::DataModelConstructor &c);
  void Refresh(); // store -> view-models -> DirtyVariable
  void Dirty(const char *name) { model_.DirtyVariable(name); }

  void Download();
  void SubmitItem();
  void SubmitNamespace();
  void ShowDialog(Rml::ElementDocument *doc, const char *focus_id);

  /// Escape / Enter for the dialogs. Registered in the capture phase: RmlUi's
  /// text input stops propagation of the keys it handles (including Enter), so
  /// a bubbling handler never sees them.
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
  Rml::ElementDocument *main_doc_ = nullptr, *item_doc_ = nullptr,
                       *ns_doc_ = nullptr, *active_dialog_ = nullptr;

  // bound variables
  Rml::Vector<VersionVm> versions_;
  Rml::Vector<GroupVm> created_groups_;
  Rml::Vector<SectionVm> sections_;
  Rml::Vector<SortOptionVm> sort_options_;
  Rml::Vector<NamespaceOptionVm> namespaces_;
  ItemFormVm item_form_;
  NamespaceFormVm ns_form_;
  bool versions_collapsed_ = false, created_collapsed_ = false;
  bool no_match_ = false, can_download_ = false, sort_open_ = false,
       toast_visible_ = false;
  Rml::String no_match_text_, created_label_, download_hint_, toast_text_;

  DialogKeys dialog_keys_{*this};

  // non-bound state
  bool id_touched_ = false;
  std::string icon_path_;
  double toast_remaining_ = 0;
};

} // namespace et
