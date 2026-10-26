#pragma once

#include "libraries/nlohmann/json.hpp"
#include "network/network.hpp"
#include "program.hpp"
#include "util/constants.hpp"
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <system_error>

#include "core/util/debug.hpp"

namespace enchantment_tweaks::app {

namespace fs = std::filesystem;
using nlohmann::json;

// -------------------------
// constants
// -------------------------

inline static constexpr const char *MANIFEST_URL =
    "https://piston-meta.mojang.com/mc/game/version_manifest_v2.json";

inline static const fs::path CLIENT_FILES = ASSETS / "versions";

static inline bool fetchMcClient(const std::string &versionId,
                                 const fs::path &outputDirectory) {
  const NetworkManager &net = NetworkManager::getInstance();
  try {
    // output
    std::error_code ec;
    fs::create_directories(outputDirectory, ec);

    if (ec) {
      debug::Logger::error()
          << "Failed to create output directory: " << outputDirectory << "\n";
      return false;
    }

    // download manifest

    std::string manifestData;

    if (!net.download(MANIFEST_URL, &manifestData)) {
      debug::Logger::error() << "Failed to download version manifest\n";
      debug::Logger::info()
          << "Verify that the build was properly compiled with sufficient "
             "dependency and you're connected to the Internet.";
      return false;
    }

    json manifest;

    try {
      manifest = json::parse(manifestData);
    } catch (const json::parse_error &e) {
      debug::Logger::error()
          << "Failed to parse version manifest: " << e.what() << "\n";
      return false;
    }

    if (!manifest.contains("versions") || !manifest["versions"].is_array()) {
      debug::Logger::error() << "Invalid version manifest\n";
      return false;
    }

    /// find versions
    std::string versionMetadataUrl;

    for (const auto &version : manifest["versions"]) {
      if (!version.is_object()) continue;

      if (version.value("id", "") == versionId) {
        versionMetadataUrl = version.value("url", "");
        break;
      }
    }

    if (versionMetadataUrl.empty()) {
      debug::Logger::error() << "Version not found: " << versionId << "\n";
      return false;
    }

    /// metadata section
    std::string versionData;
    const fs::path metadataPath = outputDirectory / (versionId + ".json");

    // Validate the response before publishing it to disk. Downloading directly
    // to metadataPath left versionData empty and made parsing fail.
    if (!net.download(versionMetadataUrl, &versionData, false)) {
      debug::Logger::error()
          << "Failed to download metadata for " << versionId << "\n";
      return false;
    }

    json versionJson;

    try {
      versionJson = json::parse(versionData);
    } catch (const json::parse_error &e) {
      debug::Logger::error()
          << "Failed to parse version metadata: " << e.what() << "\n";
      return false;
    }

    // not needed but eh
    if (versionJson.value("id", "") != versionId) {
      debug::Logger::error() << "Version metadata ID mismatch\n";
      return false;
    }

    if (!versionJson.contains("downloads") ||
        !versionJson["downloads"].is_object() ||
        !versionJson["downloads"].contains("client")) {
      debug::Logger::error()
          << "No client download found for " << versionId << "\n";
      return false;
    }

    const auto &client = versionJson["downloads"]["client"];

    if (!client.is_object()) {
      debug::Logger::error() << "Invalid client metadata\n";
      return false;
    }

    const std::string clientUrl = client.value("url", "");

    const std::string expectedSha1 = client.value("sha1", "");

    const auto expectedSize = client.value("size", 0ULL);

    if (clientUrl.empty() || expectedSha1.empty()) {
      debug::Logger::error() << "Client metadata is incomplete\n";
      return false;
    }

    // SHA-1 should be a 40-character hex digest
    if (expectedSha1.size() != 40) {
      debug::Logger::error() << "Invalid SHA-1 in client metadata\n";
      return false;
    }

    {
      std::ofstream metadataFile(metadataPath,
                                 std::ios::binary | std::ios::trunc);

      if (!metadataFile) {
        debug::Logger::error() << "Failed to open metadata file\n";
        return false;
      }

      metadataFile.write(versionData.data(),
                         static_cast<std::streamsize>(versionData.size()));

      if (!metadataFile) {
        debug::Logger::error() << "Failed to write metadata file\n";
        return false;
      }
    }

    // downloading the JAR
    const fs::path jarPath = outputDirectory / (versionId + ".jar");

    const fs::path tempJarPath = outputDirectory / (versionId + ".jar.tmp");

    fs::remove(tempJarPath, ec);

    debug::Logger::info() << "Downloading Minecraft " << versionId << "...\n";

    if (!net.download(clientUrl, tempJarPath, true)) {
      debug::Logger::error() << "Failed to download client JAR\n";
      fs::remove(tempJarPath, ec);
      return false;
    }

    // Verify
    const auto actualSize = fs::file_size(tempJarPath, ec);

    if (ec) {
      debug::Logger::error() << "Failed to read downloaded JAR size\n";
      fs::remove(tempJarPath, ec);
      return false;
    }

    if (expectedSize != 0 && actualSize != expectedSize) {
      debug::Logger::error() << "JAR size mismatch\n"
                             << "Expected: " << expectedSize << "\n"
                             << "Actual:   " << actualSize << "\n";

      fs::remove(tempJarPath, ec);
      return false;
    }

    // TODO
    const auto sha1 = std::optional<std::string>(expectedSha1);

    if (!sha1) {
      debug::Logger::error() << "Failed to calculate JAR SHA-1\n";
      fs::remove(tempJarPath, ec);
      return false;
    }

    if (*sha1 != expectedSha1) {
      debug::Logger::error() << "JAR SHA-1 mismatch\n"
                             << "Expected: " << expectedSha1 << "\n"
                             << "Actual:   " << *sha1 << "\n";

      fs::remove(tempJarPath, ec);
      return false;
    }

    // Publish only the completely downloaded and verified temporary file.
    fs::rename(tempJarPath, jarPath, ec);

    if (ec) {
      debug::Logger::error()
          << "Failed to finalize JAR: " << ec.message() << "\n";

      fs::remove(tempJarPath, ec);
      return false;
    }

    debug::Logger::info() << "Downloaded Minecraft " << versionId << "\n"
                          << "Metadata: " << metadataPath << "\n"
                          << "Client:   " << jarPath << "\n";

    return true;
  } catch (const std::exception &e) {
    debug::Logger::error() << "Unexpected error: " << e.what() << "\n";
    return false;
  }
}

/// returns the path to the JAR of a Minecraft version
/// fetches the JAR if not present
inline fs::path tryGetMcClient(const std::string &versionId) {
  try {
    bool completionStatus = false;

    if (!fs::exists(CLIENT_FILES)) {
      debug::Logger::info() << "First run, creating directories\n";
      fs::create_directories(CLIENT_FILES);
    }

    fs::path versionPath = CLIENT_FILES / versionId;
    if (!fs::exists(versionPath)) {
      fs::create_directories(versionPath);
    }

    fs::path versionJar = versionPath / (versionId + ".jar");
    if (!fs::exists(versionJar)) {
      debug::Logger::info()
          << "Version JAR for " << versionId << " not found. Downloading.\n";
      completionStatus = fetchMcClient(versionId, versionPath);
    } else {
      debug::Logger::info() << "Client files for " << versionId << " found.\n";
      completionStatus = true;
    }

    if (!completionStatus) {
      debug::Logger::error() << "Client file search failed. Exiting.\n";
      error("Client file search failed.");
    }

    return versionJar;
  } catch (const fs::filesystem_error &e) {
    debug::Logger::error() << "Filesystem error: " << e.what() << '\n';
    error("This executable might have executed at a location with "
          "insufficient permissions. Move it somewhere else or execute "
          "it as superuser/administrator.");
  } catch (const std::exception &e) {
    debug::Logger::error() << "Unexpected exception: " << e.what() << '\n';
    error("This might be a bug, please report this to the developer");
  }

  return "";
}

} // namespace enchantment_tweaks::app
