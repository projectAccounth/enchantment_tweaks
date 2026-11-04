#include "file_menu.hpp"

namespace enchantment_tweaks::native {

namespace {

et::MenuEntry item(std::string label, std::string action, std::string arg = {},
                   bool enabled = true) {
  // Keep these shortcuts in sync with HandleShortcut().
  static const std::pair<const char *, const char *> shortcuts[] = {
      {"open_pick_game", "Ctrl+N"}, {"open_project", "Ctrl+O"},
      {"save_project", "Ctrl+S"},   {"save_project_as", "Ctrl+Shift+S"},
      {"export_pack", "Ctrl+E"},    {"close_pack", "Ctrl+W"},
      {"quit", "Ctrl+Q"},           {"open_download", "Ctrl+D"},
  };
  std::string shortcut;
  for (const auto &[name, keys] : shortcuts) {
    if (action == name) shortcut = keys;
  }
  return et::MenuEntry{
      std::move(label),   std::move(action), std::move(arg), enabled, false,
      std::move(shortcut)};
}

et::MenuEntry separator() { return et::MenuEntry{{}, {}, {}, true, true}; }

} // namespace

std::vector<et::MenuEntry>
buildFileMenu(const AppSession &session,
              const std::vector<project::InstalledGame> &games) {
  std::vector<et::MenuEntry> file;
  // Use one menu entry; the dialog lists installed versions.
  file.push_back(item("New game project from installed version...",
                      "open_pick_game", {}, !games.empty()));
  file.push_back(item("New game project from JAR...", "new_game_browse"));
  file.push_back(item("New pack project from ZIP...", "new_pack_zip"));
  file.push_back(item("New pack project from folder...", "new_pack_folder"));
  file.push_back(item("Download Minecraft version...", "open_download"));
  file.push_back(separator());

  file.push_back(item("Open project...", "open_project"));
  file.push_back(
      item("Save project", "save_project", {}, session.hasProject()));
  file.push_back(
      item("Save project as...", "save_project_as", {}, session.hasProject()));
  file.push_back(separator());

  file.push_back(item("Open pack over this game...", "open_pack", {},
                      session.hasProject() && !session.isPackProject()));
  if (session.isPackProject()) {
    file.push_back(
        item("Close pack project", "close_pack", {}, session.hasProject()));
  } else if (session.hasPack()) {
    file.push_back(item("Close pack", "close_pack", {}, true));
  } else {
    file.push_back(
        item("Close project", "close_pack", {}, session.hasProject()));
  }
  file.push_back(separator());

  file.push_back(item("Export pack...", "export_pack", {},
                      session.hasProject() && (session.isPackProject() ||
                                               !session.selection().empty())));
  file.push_back(separator());
  file.push_back(item("Quit", "quit"));
  return file;
}

} // namespace enchantment_tweaks::native
