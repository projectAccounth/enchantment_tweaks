#include "download_manager.hpp"

#include "app/minecraft_downloader.hpp"
#include "core/project/minecraft_download.hpp"
#include "core/util/debug.hpp"

#include <algorithm>
#include <chrono>
#include <future>
#include <mutex>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace enchantment_tweaks::native {

DownloadManager::DownloadManager(MinecraftDownloader downloader)
    : downloader_(std::move(downloader)), worker_([this] { work(); }) {}

DownloadManager::~DownloadManager() {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    stop_ = true;
    for (const auto &entry : statuses_)
      cancelRequested_.insert(entry.first);
  }
  wake_.notify_all();
  if (worker_.joinable()) worker_.join();
}

void DownloadManager::requestManifest() {
  std::lock_guard<std::mutex> lock(mutex_);
  if (manifestFuture_ && manifestFuture_->valid()) return; // already loading
  manifestFuture_ = std::async(std::launch::async, [this] {
    std::string warning;
    project::VersionManifest manifest = downloader_.manifest(warning);
    return std::make_pair(std::move(manifest), std::move(warning));
  });
}

bool DownloadManager::manifestLoading() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return manifestFuture_ && manifestFuture_->valid() &&
         manifestFuture_->wait_for(std::chrono::seconds(0)) !=
             std::future_status::ready;
}

std::optional<project::VersionManifest> DownloadManager::takeManifest() {
  std::lock_guard<std::mutex> lock(mutex_);

  if (!manifestFuture_ || !manifestFuture_->valid()) return std::nullopt;

  if (manifestFuture_->wait_for(std::chrono::seconds(0)) !=
      std::future_status::ready)
    return std::nullopt;

  auto [manifest, warning] = manifestFuture_->get();
  manifestFuture_.reset();

  if (manifest.versions.empty()) {
    manifest.error = warning.empty() ? "the version list is empty" : warning;
  } else if (!warning.empty()) {
    manifest.error = warning; // shown as a note, the saved list is in use
  }

  return manifest;
}

bool DownloadManager::enqueue(const project::RemoteVersion &version) {
  std::lock_guard<std::mutex> lock(mutex_);
  const auto existing = statuses_.find(version.id);

  if (existing != statuses_.end() &&
      (existing->second.state == DownloadState::Queued ||
       existing->second.state == DownloadState::Running)) {

    return false;
  }
  cancelRequested_.erase(version.id);

  DownloadStatus status;
  status.id = version.id;
  status.state = DownloadState::Queued;
  status.message = "Queued";

  statuses_[version.id] = status;
  queue_.push_back(version);
  wake_.notify_one();

  return true;
}

void DownloadManager::cancel(const std::string &id) {
  std::lock_guard<std::mutex> lock(mutex_);
  const auto it = statuses_.find(id);
  if (it == statuses_.end()) return;
  if (it->second.state == DownloadState::Queued) {
    queue_.erase(std::remove_if(queue_.begin(), queue_.end(),
                                [&](const project::RemoteVersion &v) {
                                  return v.id == id;
                                }),
                 queue_.end());
    it->second.state = DownloadState::Canceled;
    it->second.message = "Canceled";
  } else if (it->second.state == DownloadState::Running) {
    cancelRequested_.insert(id);
  }
}

std::vector<DownloadStatus> DownloadManager::snapshot() const {
  std::lock_guard<std::mutex> lock(mutex_);
  std::vector<DownloadStatus> out;
  out.reserve(statuses_.size());
  for (const auto &entry : statuses_)
    out.push_back(entry.second);
  return out;
}

void DownloadManager::clearFinished() {
  std::lock_guard<std::mutex> lock(mutex_);
  for (auto it = statuses_.begin(); it != statuses_.end();) {
    const auto state = it->second.state;
    if (state == DownloadState::Done || state == DownloadState::Failed ||
        state == DownloadState::Canceled) {
      it = statuses_.erase(it);
    } else {
      ++it;
    }
  }
}

bool DownloadManager::busy() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return !queue_.empty() ||
         std::any_of(statuses_.begin(), statuses_.end(), [](const auto &e) {
           return e.second.state == DownloadState::Running;
         });
}

void DownloadManager::work() {
  for (;;) {
    project::RemoteVersion next;
    {
      std::unique_lock<std::mutex> lock(mutex_);
      wake_.wait(lock, [this] { return stop_ || !queue_.empty(); });
      if (stop_) return;

      next = queue_.front();
      queue_.pop_front();

      auto &status = statuses_[next.id];
      status.state = DownloadState::Running;
      status.fraction = 0.0;
      status.message = "Starting";
    }

    const auto progress = [this, id = next.id](double fraction,
                                               const std::string &stage) {
      std::lock_guard<std::mutex> lock(mutex_);
      auto &status = statuses_[id];

      status.fraction = fraction;
      status.message = stage;

      return cancelRequested_.count(id) == 0 && !stop_;
    };

    const std::string error = downloader_.download(next, progress);

    std::lock_guard<std::mutex> lock(mutex_);
    auto &status = statuses_[next.id];
    cancelRequested_.erase(next.id);
    if (error.empty()) {
      status.state = DownloadState::Done;
      status.fraction = 1.0;
      status.message = "Installed";

    } else if (error == "canceled") {
      status.state = DownloadState::Canceled;
      status.message = "Canceled";

    } else {
      status.state = DownloadState::Failed;
      status.message = error;
      debug::Logger::error()
          << "download of " << next.id << " failed: " << error;
    }
  }
}

} // namespace enchantment_tweaks::native
