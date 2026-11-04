#pragma once

#include "core/project/minecraft_download.hpp"
#include "minecraft_downloader.hpp"

#include <condition_variable>
#include <cstdint>
#include <deque>
#include <future>
#include <map>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace enchantment_tweaks::native {

enum class DownloadState : uint8_t { Queued, Running, Done, Failed, Canceled };

struct DownloadStatus {
  std::string id;
  DownloadState state = DownloadState::Queued;
  double fraction = 0.0;
  std::string message; // stage, or the error for Failed
};

class DownloadManager {
public:
  explicit DownloadManager(MinecraftDownloader downloader);
  ~DownloadManager();
  DownloadManager(const DownloadManager &) = delete;
  DownloadManager &operator=(const DownloadManager &) = delete;

  void requestManifest();
  std::optional<project::VersionManifest> takeManifest();
  [[nodiscard]] bool manifestLoading() const;

  /// Queues a version unless it is already queued or downloading.
  bool enqueue(const project::RemoteVersion &version);

  /// Cancels a queued or running download and removes its temporary files.
  void cancel(const std::string &id);

  /// Returns the state of current downloads.
  [[nodiscard]] std::vector<DownloadStatus> snapshot() const;
  /// Removes completed entries after they have been shown in the UI.
  void clearFinished();
  [[nodiscard]] bool busy() const;

  [[nodiscard]] const MinecraftDownloader &downloader() const noexcept {
    return downloader_;
  }

private:
  void work();

  MinecraftDownloader downloader_;
  mutable std::mutex mutex_;
  std::condition_variable wake_;
  std::deque<project::RemoteVersion> queue_;
  std::map<std::string, DownloadStatus> statuses_;
  std::set<std::string> cancelRequested_;
  std::optional<std::future<std::pair<project::VersionManifest, std::string>>>
      manifestFuture_;
  bool stop_ = false;
  std::thread worker_;
};

} // namespace enchantment_tweaks::native
