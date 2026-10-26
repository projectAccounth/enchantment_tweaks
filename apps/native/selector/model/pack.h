#pragma once
#include "types.h"
#include <vector>

namespace et {

// TODO: swap for your real pack format; pack_format changes between Minecraft
// versions.
inline constexpr int kPackFormat = 0;

/// Same files as src/data/pack.ts in the web app.
std::vector<PackFile> buildPackFiles(const std::vector<std::string> &versions,
                                     const std::vector<Item> &items);

} // namespace et
