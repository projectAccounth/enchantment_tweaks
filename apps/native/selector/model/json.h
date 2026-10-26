#pragma once
// JSON <-> model, using the same shapes as the web app (src/data/parse.ts,
// state/hydrate.ts). Everything read from disk is treated as untrusted: bad
// entries are dropped, never thrown on.

#include "types.h"
#include <filesystem>
#include <optional>
#include <string>

namespace et {

/// nullopt if the document is unusable (no versions or no namespaces).
std::optional<Catalog> parseCatalog(const std::string &json_text);
std::optional<PersistedState> parsePersisted(const std::string &json_text);
std::string toJson(const PersistedState &state);

// Small file helpers (UTF-8, whole file). readFile returns nullopt if
// missing/unreadable.
std::optional<std::string> readFile(const std::filesystem::path &path);
bool writeFile(const std::filesystem::path &path,
               const std::string &text); // creates parent folders

} // namespace et
