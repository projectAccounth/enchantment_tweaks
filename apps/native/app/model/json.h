#pragma once

#include "types.h"
#include <filesystem>
#include <optional>
#include <string>

namespace fs = std::filesystem;

namespace et {

std::optional<Catalog> parseCatalog(const std::string &json_text);
std::optional<PersistedState> parsePersisted(const std::string &json_text);
std::string toJson(const PersistedState &state);

std::optional<std::string> readFile(const fs::path &path);
bool writeFile(const fs::path &path, const std::string &text);

} // namespace et
