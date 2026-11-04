#pragma once

#include "app_session.hpp"
#include "core/project/game_versions.hpp"
#include "ui/selector_ui.h"

#include <vector>

namespace enchantment_tweaks::native {

/// Builds the File menu for the current application state.
std::vector<et::MenuEntry>
buildFileMenu(const AppSession &session,
              const std::vector<project::InstalledGame> &games);

} // namespace enchantment_tweaks::native
