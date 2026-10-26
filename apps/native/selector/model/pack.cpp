#include "pack.h"
#include <libraries/nlohmann/json.hpp>

namespace et {
using nlohmann::json;

std::vector<PackFile> buildPackFiles(const std::vector<std::string> &versions,
                                     const std::vector<Item> &items) {
  std::vector<PackFile> files;
  files.push_back(
      {"pack.mcmeta", json{{"pack",
                            {{"pack_format", kPackFormat},
                             {"description", "Enchantment Tweaks pack"}}}}
                          .dump(2)});

  json listed = json::array();
  for (const auto &i : items)
    listed.push_back({{"namespace", i.ns}, {"id", i.id}, {"name", i.name}});
  files.push_back({"enchantment_tweaks.json",
                   json{{"versions", versions}, {"items", listed}}.dump(2)});

  for (const auto &i : items)
    files.push_back({"data/" + i.ns + "/enchantment_tweaks/" + i.id + ".json",
                     json{{"item", i.Key()}, {"versions", versions}}.dump(2)});
  return files;
}

} // namespace et
