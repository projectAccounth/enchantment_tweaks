#pragma once

// a mirror of minecraft's resourcelocation impl

#include "core/util/string.hpp"
#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <string>
#include <vector>

namespace enchantment_tweaks {

class Identifier {
private:
  std::string _namespace;
  std::string _path;

  inline static bool verifyNamespace(const std::string &str) {
    return std::all_of(str.begin(), str.end(), [](char c) {
      return std::isalnum(static_cast<unsigned char>(c));
    });
  }

  inline static bool verifyName(const std::string &str) {
    return std::all_of(str.begin(), str.end(), [](char c) {
      return std::isalnum(static_cast<unsigned char>(c)) || c == '/' ||
             c == '.';
    });
  }

public:
  Identifier(std::string _namespace, std::string _path)
      : _namespace(std::move(_namespace)), _path(std::move(_path)) {
    if (!verifyNamespace(_namespace) || !verifyName(_path))
      throw std::runtime_error(std::string("Error constructing Identifier: ") +
                               _namespace + " " + _path);
  }

  bool operator==(const Identifier &other) const {
    return _namespace == other._namespace && _path == other._path;
  }

  bool operator!=(const Identifier &other) const { return !(*this == other); }

  inline static Identifier parse(const std::string &str) {
    std::vector<std::string> elems = util::split(str, ":");

    if (elems.size() != 2) {
      throw std::runtime_error(std::string("Error parsing Identifier, not "
                                           "separated by exactly one colon: ") +
                               str);
    }

    const std::string &_namespace = elems[0];
    const std::string &_path = elems[1];

    if (!verifyNamespace(_namespace) || !verifyName(_path))
      throw std::runtime_error(std::string("Error parsing Identifier: ") +
                               _namespace + " " + _path);

    return Identifier(_namespace, _path);
  }
};

} // namespace enchantment_tweaks