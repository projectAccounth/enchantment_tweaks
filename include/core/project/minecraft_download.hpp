#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace enchantment_tweaks::project {
namespace fs = std::filesystem;

/// One version entry from the manifest.
struct RemoteVersion {
  std::string id;   // "1.21.4"
  std::string type; // "release", "snapshot", ...
  std::string url;  // version metadata (JSON)
};

struct VersionManifest {
  std::vector<RemoteVersion> versions; // as listed (newest first)
  std::string error;                   // empty on success
};

/// Parses version_manifest_v2.json.
VersionManifest parseVersionManifest(const std::string &json);

struct ClientDownload {
  std::string url;
  std::string sha1;
  uint64_t size = 0;
};

struct ClientDownloadResult {
  std::optional<ClientDownload> download;
  std::string error;
};

ClientDownloadResult parseClientDownload(const std::string &versionJson);

/// Returns the lowercase SHA-1 digest.
std::string sha1Hex(const std::vector<uint8_t> &bytes);

/// Returns the resource pack format targeted by a game JAR.
std::optional<int> resourceFormatOfJar(const fs::path &jar);
} // namespace enchantment_tweaks::project
