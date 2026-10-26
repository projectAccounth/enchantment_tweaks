#include "core/types/identifier.hpp"

#include "core/util/string.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace enchantment_tweaks {

bool Identifier::verifyNamespace(const std::string &str) {
  return !str.empty() && std::ranges::all_of(str, [](char c) {
    return std::isalnum(static_cast<unsigned char>(c)) || c == '_' ||
           c == '-' || c == '.';
  });
}

bool Identifier::verifyName(const std::string &str) {
  return !str.empty() && std::ranges::all_of(str, [](char c) {
    return std::isalnum(static_cast<unsigned char>(c)) || c == '/' ||
           c == '.' || c == '_' || c == '-';
  });
}

Identifier::Identifier() : _namespace(), _path() {}

Identifier::Identifier(std::string nmsp, std::string path)
    : _namespace(std::move(nmsp)), _path(std::move(path)) {
  if (!verifyNamespace(_namespace) || !verifyName(_path)) {
    throw std::runtime_error(std::string("Error constructing Identifier: ") +
                             _namespace + " " + _path);
  }
}

bool Identifier::operator==(const Identifier &other) const {
  return _namespace == other._namespace && _path == other._path;
}

bool Identifier::operator!=(const Identifier &other) const {
  return !(*this == other);
}

std::string Identifier::getNamespace() const { return _namespace; }
std::string Identifier::getPath() const { return _path; }
std::string Identifier::getString() const { return _namespace + ":" + _path; }
std::string Identifier::toString() const { return getString(); }

fs::path Identifier::getFsPath(const std::string &ext) const {
  return fs::path(_namespace) / (_path + ext);
}

Identifier Identifier::parse(const std::string &str) {
  bool colon = str.find(':') != std::string::npos;
  std::vector<std::string> elems = util::split(str, ":");
  // defaults to Minecraft namespace with 1 token
  if (elems.size() == 1) {
    if (colon) { // should never happen
      throw std::runtime_error("Error parsing Identifier: " + str +
                               ", invalid format with stray colon");
    }
    if (!verifyName(elems[0])) {
      throw std::runtime_error(
          std::string("Error parsing path-only Identifier: ") + elems[0]);
    }

    return {"minecraft", elems[0]};
  }
  if (elems.size() != 2) {
    throw std::runtime_error(std::string("Error parsing Identifier, not "
                                         "separated by exactly one colon: ") +
                             str);
  }

  const std::string &nmsp = elems[0];
  const std::string &path = elems[1];
  if (!verifyNamespace(nmsp) || !verifyName(path)) {
    throw std::runtime_error(std::string("Error parsing Identifier: ") + nmsp +
                             " " + path);
  }
  return {nmsp, path};
}

} // namespace enchantment_tweaks
