#include "file_dialogs.hpp"

#include "core/util/debug.hpp"

#include <nfd.h>

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace enchantment_tweaks::native {

namespace {

// joins all the extensions with a comma
std::string filterSpec(const std::vector<std::string> &extensions) {
  std::string spec;
  for (const auto &ext : extensions) {
    if (!spec.empty()) spec += ',';
    spec += ext;
  }
  return spec;
}

std::optional<fs::path> takePath(nfdu8char_t *raw) {
  fs::path path(reinterpret_cast<const char *>(raw));
  NFD_FreePathU8(raw);
  return path;
}

} // namespace

FileDialogs::FileDialogs() : available_(NFD_Init() == NFD_OKAY) {}

FileDialogs::~FileDialogs() {
  if (available_) NFD_Quit();
}

std::optional<fs::path>
FileDialogs::openFile(const std::vector<std::string> &extensions,
                      const fs::path &startDirectory) const {
  if (!available_) {
    debug::Logger::error() << "native file dialogs unavailable: "
                           << NFD_GetError();
    return std::nullopt;
  }
  const std::string spec = filterSpec(extensions);
  const nfdu8filteritem_t filter{"Minecraft files", spec.c_str()};
  const std::string start = startDirectory.string();

  nfdu8char_t *out = nullptr;
  const nfdresult_t result = NFD_OpenDialogU8(
      &out, &filter, 1, start.empty() ? nullptr : start.c_str());
  if (result != NFD_OKAY) {
    if (result == NFD_ERROR)
      debug::Logger::error() << "open dialog failed: " << NFD_GetError();
    return std::nullopt;
  }
  return takePath(out);
}

std::optional<fs::path>
FileDialogs::pickFolder(const fs::path &startDirectory) const {
  if (!available_) {
    debug::Logger::error() << "native file dialogs unavailable: "
                           << NFD_GetError();
    return std::nullopt;
  }
  const std::string start = startDirectory.string();
  nfdu8char_t *out = nullptr;
  const nfdresult_t result =
      NFD_PickFolderU8(&out, start.empty() ? nullptr : start.c_str());
  if (result != NFD_OKAY) {
    if (result == NFD_ERROR)
      debug::Logger::error() << "folder dialog failed: " << NFD_GetError();
    return std::nullopt;
  }
  return takePath(out);
}

std::optional<fs::path>
FileDialogs::saveFile(const std::vector<std::string> &extensions,
                      const std::string &defaultName,
                      const fs::path &startDirectory) const {
  if (!available_) {
    debug::Logger::error() << "native file dialogs unavailable: "
                           << NFD_GetError();
    return std::nullopt;
  }
  const std::string spec = filterSpec(extensions);
  const nfdu8filteritem_t filter{"Resource pack", spec.c_str()};
  const std::string start = startDirectory.string();

  nfdu8char_t *out = nullptr;
  const nfdresult_t result = NFD_SaveDialogU8(
      &out, &filter, 1, start.empty() ? nullptr : start.c_str(),
      defaultName.c_str());
  if (result != NFD_OKAY) {
    if (result == NFD_ERROR)
      debug::Logger::error() << "save dialog failed: " << NFD_GetError();
    return std::nullopt;
  }
  return takePath(out);
}

} // namespace enchantment_tweaks::native
