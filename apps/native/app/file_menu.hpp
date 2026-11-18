#pragma once

#include "app_session.hpp"
#include "core/project/game_versions.hpp"
#include "ui/selector_ui.h"

#include <vector>

namespace enchantment_tweaks::native {

/// Builds the File menu.
std::vector<MenuEntry>
buildFileMenu(const AppSession &session,
              const std::vector<project::InstalledGame> &games);

} // namespace enchantment_tweaks::native
