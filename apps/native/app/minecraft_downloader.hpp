#pragma once

#include "core/project/minecraft_download.hpp"

#include <filesystem>
#include <functional>
#include <string>

namespace enchantment_tweaks::native {
namespace fs = std::filesystem;

using DownloadProgressFn =
    std::function<bool(double fraction, const std::string &stage)>;

class MinecraftDownloader {
public:
  explicit MinecraftDownloader(const fs::path &appDirectory);

  /// Initializes the network library.
  static void initialiseNetwork();

  /// Loads the version list, using the cached copy if the network fails.
  [[nodiscard]] project::VersionManifest manifest(std::string &warning) const;

  /// Downloads a client JAR; returns empty on success or an error/cancellation
  /// message.
  std::string download(const project::RemoteVersion &version,
                       const DownloadProgressFn &progress) const;

  /// Returns the app-managed versions directory.
  [[nodiscard]] fs::path versionsRoot() const { return root_ / "versions"; }

private:
  fs::path root_;
};

} // namespace enchantment_tweaks::native
