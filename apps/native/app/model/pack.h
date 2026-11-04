#pragma once
#include "types.h"
#include <string>
#include <vector>

namespace et {

inline constexpr int kPackFormat = 0;

std::vector<PackFile> buildPackFiles(const std::vector<std::string> &versions,
                                     const std::vector<Item> &items);

} // namespace et
