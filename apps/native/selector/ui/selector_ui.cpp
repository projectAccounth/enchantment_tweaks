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
#include "selector/model/store.h"
#include "selector/model/types.h"
#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <functional>
#include <limits>
#include <string>
#include <system_error>
#include <utility>

namespace et {

namespace {
const char *kQuote = "\xE2\x80\x9C";   // “
const char *kUnquote = "\xE2\x80\x9D"; // ”

Rml::String Arg0(const Rml::VariantList &args) {
  return args.empty() ? Rml::String() : args[0].Get<Rml::String>();
}

} // namespace

bool IsExistingAbsoluteFile(const std::string &path) {
  const std::filesystem::path p(path);
  std::error_code ec;
  return p.is_absolute() && !p.has_root_name() &&
         std::filesystem::is_regular_file(p, ec);
}

Rml::String SelectorUi::ResolveIcon(const std::string &icon) const {
  if (icon.empty() || icon.starts_with("data:"))
    return "../sprites/placeholder.png";
  if (icon.find_first_of("/\\.") == std::string::npos)
    return "../sprites/" + icon + ".png"; // sprite name

  // A file the user picked: returned untouched. RmlUi would strip a leading '/'
  // while joining it with the document path, so the engine's
  // SystemInterface::JoinPath must let existing absolute files through; see
  // IsExistingAbsoluteFile() in SelectorUi.h.
  return icon;
}

SelectorUi::SelectorUi(Rml::Context *context, Store &store,
                       UiCallbacks callbacks, std::string asset_dir)
    : ctx_(context), store_(store), cb_(std::move(callbacks)),
      asset_dir_(std::move(asset_dir)) {}

SelectorUi::~SelectorUi() {
  if (!ctx_) return;
  for (auto *doc : {main_doc_, item_doc_, ns_doc_})
    if (doc) doc->Close();
  ctx_->RemoveDataModel("selector");
}

bool SelectorUi::Initialise() {
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
  if (!main_doc_ || !item_doc_ || !ns_doc_)
    return false; // RmlUi has logged why

  item_doc_->Hide();
  ns_doc_->Hide();
  item_doc_->AddEventListener("keydown", &dialog_keys_,
                              /*in_capture_phase=*/true);
  ns_doc_->AddEventListener("keydown", &dialog_keys_,
                            /*in_capture_phase=*/true);
  main_doc_->Show();
  return true;
}

bool SelectorUi::RegisterModel() {
  Rml::DataModelConstructor c = ctx_->CreateDataModel("selector");
  if (!c) return false;

  // Order matters (fuck you my sleepy self)
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

  c.Bind("versions", &versions_);
  c.Bind("versions_collapsed", &versions_collapsed_);
  c.Bind("created_groups", &created_groups_);
  c.Bind("created_collapsed", &created_collapsed_);
  c.Bind("created_label", &created_label_);
  c.Bind("can_download", &can_download_);
  c.Bind("download_hint", &download_hint_);
  c.Bind("sections", &sections_);
  c.Bind("no_match", &no_match_);
  c.Bind("no_match_text", &no_match_text_);
  c.Bind("sort_open", &sort_open_);
  c.Bind("sort_options", &sort_options_);
  c.Bind("namespaces", &namespaces_);
  c.Bind("item_form", &item_form_);
  c.Bind("ns_form", &ns_form_);
  c.Bind("toast_visible", &toast_visible_);
  c.Bind("toast_text", &toast_text_);

  // The search box edits the store directly (two-way)
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

  // main scr
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
  on("download", [this](Rml::Event &, Args) { Download(); });

  on("toggle_sort_menu", [this](Rml::Event &, Args) {
    sort_open_ = !sort_open_;
    Dirty("sort_open");
  });
  on("set_sort", [this](Rml::Event &, Args a) {
    if (auto key = sortKeyFromString(Arg0(a))) store_.setSort(*key);
    sort_open_ = false;
    Dirty("sort_open");
  });
  // Click anywhere else closes the sort menu (RmlUi has no "outside click"
  // event).
  on("body_click", [this](Rml::Event &ev, Args) {
    if (!sort_open_) return;
    Rml::Element *target = ev.GetTargetElement();
    if (target && target->Closest(".sortrow")) return;
    sort_open_ = false;
    Dirty("sort_open");
  });

  on("open_item_dialog", [this](Rml::Event &, Args) { OpenItemDialog(); });
  on("open_ns_dialog", [this](Rml::Event &, Args) { OpenNamespaceDialog(); });

  // ---- dialogs
  on("close_dialog", [this](Rml::Event &, Args) { CloseDialog(); });
  on("dialog_backdrop", [this](Rml::Event &ev, Args) {
    if (ev.GetTargetElement() == ev.GetCurrentElement())
      CloseDialog(); // only clicks on the dimmed area
  });
  // The ID follows the name until the user edits it by hand.
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
      item_form_.icon_name = std::filesystem::path(*path).filename().string();
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

  // Sections and cards are never removed while searching, only hidden
  // (`visible` + data-if). RmlUi re-evaluates the bindings of nested data-for
  // elements before it drops the ones whose list shrank, which logs an "array
  // index out of bounds" warning. Stable sizes avoid that.
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
    sections_.push_back(std::move(vm));
  }
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

  // `query` is deliberately not dirtied: the search box is its source, and
  // rewriting it while the user types would fight the caret.
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
    // Enter submits from a text field only; on a button it keeps its normal
    // meaning (click).
    Rml::Element *target = event.GetTargetElement();
    if (!target || target->GetTagName() != "input") return;
    if (ui.active_dialog_ == ui.item_doc_)
      ui.SubmitItem();
    else if (ui.active_dialog_ == ui.ns_doc_)
      ui.SubmitNamespace();
    event.StopPropagation();
  }
}

void SelectorUi::Update(double dt) {
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
  // else: the user cancelled; say nothing
}

} // namespace et
