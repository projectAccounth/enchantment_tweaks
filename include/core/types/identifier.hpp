#pragma once

// a mirror of minecraft's resourcelocation impl

#include "core/util/string.hpp"
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

namespace enchantment_tweaks {

namespace fs = std::filesystem;

class Identifier {
private:
  std::string _namespace;
  std::string _path;

  static bool verifyNamespace(const std::string &str) {
    return std::ranges::all_of(str, [](char c) {
      return std::isalnum(static_cast<unsigned char>(c));
    });
  }

  static bool verifyName(const std::string &str) {
    return std::ranges::all_of(str, [](char c) {
      return std::isalnum(static_cast<unsigned char>(c)) || c == '/' ||
             c == '.';
    });
  }

public:
  Identifier(std::string nmsp, std::string path)
      : _namespace(std::move(nmsp)), _path(std::move(path)) {
    if (!verifyNamespace(_namespace) || !verifyName(_path))
      throw std::runtime_error(std::string("Error constructing Identifier: ") +
                               _namespace + " " + _path);
  }

  bool operator==(const Identifier &other) const {
    return _namespace == other._namespace && _path == other._path;
  }

  bool operator!=(const Identifier &other) const { return !(*this == other); }

  std::string getNamespace() const { return _namespace; }
  std::string getPath() const { return _path; }
  std::string getString() const { return _namespace + ":" + _path; }
  std::string toString() const { return getString(); }

  /// Gets the FS path with a suffix for extension or something idk
  fs::path getFsPath(const std::string &suf = "") const {
    return fs::path(_namespace) / (_path + suf);
  }

  static Identifier parse(const std::string &str) {
    std::vector<std::string> elems = util::split(str, ":");

    if (elems.size() != 2) {
      throw std::runtime_error(std::string("Error parsing Identifier, not "
                                           "separated by exactly one colon: ") +
                               str);
    }

    const std::string &nmsp = elems[0];
    const std::string &path = elems[1];

    if (!verifyNamespace(nmsp) || !verifyName(path))
      throw std::runtime_error(std::string("Error parsing Identifier: ") +
                               nmsp + " " + path);

    return {nmsp, path};
  }
};

} // namespace enchantment_tweaks

namespace std {
template <> struct hash<enchantment_tweaks::Identifier> {
  size_t operator()(const enchantment_tweaks::Identifier &id) const noexcept {
    return util::hashMultiple(id.getNamespace(), id.getPath());
  }
};

} // namespace std
