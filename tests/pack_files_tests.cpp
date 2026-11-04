#include "core/api/pack_writer.hpp"
#include "core/util/debug.hpp"
#include "core/util/pack_files.hpp"
#include "core/util/zip_writer.hpp"
#include "test_utils.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>

using namespace enchantment_tweaks;
using namespace enchantment_tweaks::conversion;
namespace fs = std::filesystem;

namespace {

bool hasPath(const std::vector<OutputFile> &files, const std::string &path) {
  return std::any_of(files.begin(), files.end(),
                     [&](const OutputFile &f) { return f.path == path; });
}

std::string textOf(const std::vector<OutputFile> &files,
                   const std::string &path) {
  for (const auto &f : files) {
    if (f.path == path) return {f.bytes.begin(), f.bytes.end()};
  }
  return {};
}

} // namespace

int main() {
  test_util::Suite suite;

  suite.run("a ZIP pack is read file by file, with its paths", [] {
    const fs::path zipPath = fs::temp_directory_path() / "et_pack_files.zip";
    zip::ZipWriter zw;
    zw.addFile("pack.mcmeta",
               std::string(
                   R"({"pack":{"pack_format":46,"description":"base"}})"));
    zw.addFile("assets/minecraft/items/bow.json",
               std::string("{\"model\":{}}"));
    zw.addFile("assets/minecraft/textures/item/bow.png",
               std::string("png-bytes"));
    zw.write(zipPath);
    const auto files = pack_files::readPackFiles(zipPath);
    test_util::check(files.size() == 3,
                     "expected 3 files, got " + std::to_string(files.size()));
    test_util::check(hasPath(files, "assets/minecraft/items/bow.json"),
                     "item path lost");
    test_util::check(textOf(files, "assets/minecraft/textures/item/bow.png") ==
                         "png-bytes",
                     "bytes not read back");
    fs::remove(zipPath);
  });

  suite.run("a folder pack is read with the same relative paths", [] {
    const fs::path root = fs::temp_directory_path() / "et_pack_folder";
    fs::remove_all(root);
    fs::create_directories(root / "assets" / "minecraft" / "items");
    std::ofstream(root / "pack.mcmeta") << "{}";
    std::ofstream(root / "assets" / "minecraft" / "items" / "bow.json") << "{}";
    const auto files = pack_files::readPackFiles(root);
    test_util::check(hasPath(files, "pack.mcmeta") &&
                         hasPath(files, "assets/minecraft/items/bow.json"),
                     "folder paths wrong");
    fs::remove_all(root);
  });

  suite.run("a missing pack is an error, not an empty pack", [] {
    bool threw = false;
    try {
      (void)pack_files::readPackFiles("/no/such/pack.zip");
    } catch (const std::runtime_error &) {
      threw = true;
    }
    test_util::check(threw, "missing pack read without error");
  });

  suite.run("put replaces a base file and keeps one copy", [] {
    PackBuilder pack("test", 46);
    pack.put({"assets/minecraft/items/bow.json", {'o', 'l', 'd'}});
    pack.put({"assets/minecraft/items/bow.json", {'n', 'e', 'w'}});
    const auto files = pack.files();
    int count = 0;
    for (const auto &f : files) {
      if (f.path == "assets/minecraft/items/bow.json") ++count;
    }
    test_util::check(count == 1, "file duplicated");
    test_util::check(textOf(files, "assets/minecraft/items/bow.json") == "new",
                     "file not replaced");
  });

  suite.run("supported formats are written into pack.mcmeta", [] {
    PackBuilder pack("test", 46);
    pack.setSupportedFormats(46, 46);
    const std::string meta = textOf(pack.files(), "pack.mcmeta");
    test_util::check(meta.find("\"pack_format\": 46") != std::string::npos,
                     "pack_format missing");
    test_util::check(meta.find("\"min_inclusive\": 46") != std::string::npos &&
                         meta.find("\"max_inclusive\": 46") !=
                             std::string::npos,
                     "supported_formats missing");
    bool threw = false;
    try {
      pack.setSupportedFormats(50, 40);
    } catch (const std::invalid_argument &) {
      threw = true;
    }
    test_util::check(threw, "inverted range accepted");
  });

  return suite.finish();
}
