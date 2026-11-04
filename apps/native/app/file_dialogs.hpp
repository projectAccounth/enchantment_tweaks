#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace enchantment_tweaks::native {
namespace fs = std::filesystem;

class FileDialogs {
public:
  FileDialogs();
  ~FileDialogs();
  FileDialogs(const FileDialogs &) = delete;
  FileDialogs &operator=(const FileDialogs &) = delete;

  [[nodiscard]] bool available() const noexcept { return available_; }

  [[nodiscard]] std::optional<fs::path>
  openFile(const std::vector<std::string> &extensions,
           const fs::path &startDirectory = {}) const;

  [[nodiscard]] std::optional<fs::path>
  pickFolder(const fs::path &startDirectory = {}) const;

  [[nodiscard]] std::optional<fs::path>
  saveFile(const std::vector<std::string> &extensions,
           const std::string &defaultName,
           const fs::path &startDirectory = {}) const;

private:
  bool available_ = false;
};

} // namespace enchantment_tweaks::native
