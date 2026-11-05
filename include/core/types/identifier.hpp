#pragma once

// Mirrors Minecraft's resource-location format.

#include "core/util/string.hpp"

#include <cstdlib>
#include <filesystem>
#include <string>

namespace enchantment_tweaks {

namespace fs = std::filesystem;

class Identifier {
private:
  std::string _namespace;
  std::string _path;

  static bool verifyNamespace(const std::string &str);
  static bool verifyName(const std::string &str);

public:
  Identifier();
  Identifier(std::string nmsp, std::string path);

  bool operator==(const Identifier &other) const;
  bool operator!=(const Identifier &other) const;

  std::string getNamespace() const;
  std::string getPath() const;
  std::string getString() const;
  std::string toString() const;

  /// Gets the FS path with a suffix.
  fs::path getFsPath(const std::string &ext = "") const;

  static Identifier parse(const std::string &str);
};

} // namespace enchantment_tweaks

namespace std {
template <> struct hash<enchantment_tweaks::Identifier> {
  size_t operator()(const enchantment_tweaks::Identifier &id) const noexcept {
    return enchantment_tweaks::util::hashMultiple(id.getNamespace(),
                                                  id.getPath());
  }
};

} // namespace std
