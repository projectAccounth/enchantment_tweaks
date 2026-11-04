#include "minecraft_downloader.hpp"

#include "core/project/minecraft_download.hpp"
#include "core/util/file_io.hpp"

#include <cstdint>
#include <curl/curl.h>

#include <cstdlib>
#include <curl/easy.h>
#include <curl/system.h>
#include <filesystem>
#include <fstream>
#include <ios>
#include <string>
#include <system_error>
#include <vector>

namespace enchantment_tweaks::native {

namespace fs = std::filesystem;

namespace {

constexpr const char *kManifestUrl =
    "https://piston-meta.mojang.com/mc/game/version_manifest_v2.json";
constexpr long kTimeoutSeconds = 120;

size_t writeToString(char *data, size_t size, size_t count, void *out) {
  static_cast<std::string *>(out)->append(data, size * count);
  return size * count;
}

size_t writeToFile(char *data, size_t size, size_t count, void *out) {
  auto *file = static_cast<std::ofstream *>(out);
  file->write(data, static_cast<std::streamsize>(size * count));
  return file->good() ? size * count : 0;
}

struct ProgressSink {
  const DownloadProgressFn *progress = nullptr;
  bool canceled = false;
};

int transferProgress(void *clientp, curl_off_t dltotal, curl_off_t dlnow,
                     curl_off_t, curl_off_t) {
  auto *sink = static_cast<ProgressSink *>(clientp);
  if (!sink->progress || !*sink->progress || dltotal <= 0) return 0;
  const double fraction =
      static_cast<double>(dlnow) / static_cast<double>(dltotal);
  if (!(*sink->progress)(fraction, "Downloading")) {
    sink->canceled = true;
    return 1; // abort the transfer
  }
  return 0;
}

// Runs one GET. Writes into `sink` through `writer`. Returns "" on success.
std::string httpGet(const std::string &url, curl_write_callback writer,
                    void *sink, const DownloadProgressFn *progress = nullptr) {
  CURL *curl = curl_easy_init();
  if (!curl) return "could not start the network client";
  char errorText[CURL_ERROR_SIZE] = {0};
  ProgressSink progressSink{.progress = progress, .canceled = false};
  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, kTimeoutSeconds);
  curl_easy_setopt(curl, CURLOPT_USERAGENT, "EnchantmentTweaks");
  curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errorText);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writer);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, sink);
  if (progress) {
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, transferProgress);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &progressSink);
  }
  const CURLcode code = curl_easy_perform(curl);
  curl_easy_cleanup(curl);
  if (progressSink.canceled) return "canceled";
  if (code != CURLE_OK) {
    return std::string("download failed: ") +
           (errorText[0] ? errorText : curl_easy_strerror(code));
  }
  return {};
}

} // namespace

MinecraftDownloader::MinecraftDownloader(const fs::path &appDirectory)
    : root_(appDirectory / "minecraft") {}

project::VersionManifest
MinecraftDownloader::manifest(std::string &warning) const {
  const fs::path cache = root_ / "version_manifest_v2.json";

  if (const char *local = std::getenv("ET_VERSION_MANIFEST")) {
    if (const auto text = file_io::readText(local)) {
      auto parsed = project::parseVersionManifest(*text);
      if (parsed.error.empty()) return parsed;
      warning = "local manifest is not valid: " + parsed.error;
    } else {
      warning = std::string("local manifest not found: ") + local;
    }
    project::VersionManifest none;
    none.error = warning;
    return none;
  }

  std::string body;
  const std::string error = httpGet(kManifestUrl, writeToString, &body);
  if (error.empty()) {
    auto parsed = project::parseVersionManifest(body);
    if (parsed.error.empty()) {
      std::error_code ec;
      fs::create_directories(root_, ec);
      std::ofstream(cache, std::ios::binary | std::ios::trunc) << body;
      return parsed;
    }
    warning = parsed.error;
  } else {
    warning = error;
  }
  // last good copy.
  if (const auto cached = file_io::readText(cache)) {
    auto parsed = project::parseVersionManifest(*cached);
    if (parsed.error.empty()) {
      warning += " (showing the saved list)";
      return parsed;
    }
  }
  project::VersionManifest none;
  none.error = warning;
  return none;
}

void MinecraftDownloader::initialiseNetwork() {
  static const CURLcode code = curl_global_init(CURL_GLOBAL_DEFAULT);
  (void)code;
}

std::string
MinecraftDownloader::download(const project::RemoteVersion &version,
                              const DownloadProgressFn &progress) const {
  std::string metadata;
  if (auto error = httpGet(version.url, writeToString, &metadata);
      !error.empty())
    return error;
  const auto client = project::parseClientDownload(metadata);
  if (!client.download) return client.error;

  const fs::path folder = versionsRoot() / version.id;
  const fs::path jar = folder / (version.id + ".jar");
  const fs::path partial = folder / (version.id + ".jar.part");
  std::error_code ec;
  fs::create_directories(folder, ec);
  if (ec) return "cannot create " + folder.string() + ": " + ec.message();

  {
    std::ofstream out(partial, std::ios::binary | std::ios::trunc);
    if (!out) return "cannot write " + partial.string();
    if (auto error =
            httpGet(client.download->url, writeToFile, &out, &progress);
        !error.empty()) {
      out.close();
      fs::remove(partial, ec);
      return error;
    }
  }

  if (progress) progress(1.0, "Checking");
  const auto bytes = file_io::readText(partial);
  if (!bytes) return "could not read the download";
  const std::vector<uint8_t> data(bytes->begin(), bytes->end());
  if (project::sha1Hex(data) != client.download->sha1) {
    fs::remove(partial, ec);
    return "the download of " + version.id +
           " did not match its checksum; it was discarded";
  }

  fs::rename(partial, jar, ec);
  if (ec) return "could not place " + jar.string() + ": " + ec.message();
  std::ofstream(folder / (version.id + ".json"),
                std::ios::binary | std::ios::trunc)
      << metadata;
  return {};
}

} // namespace enchantment_tweaks::native
