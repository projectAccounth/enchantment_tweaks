#pragma once

#include "libraries/nlohmann/json.hpp"
#include "network/network.hpp"
#include "program.hpp"
#include "util/constants.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>

namespace enchantment_tweaks::app {

namespace fs = std::filesystem;
using json = nlohmann::json;

// -------------------------
// constants
// -------------------------

inline static constexpr const char *MANIFEST_URL =
    "https://piston-meta.mojang.com/mc/game/version_manifest_v2.json";

inline static const fs::path CLIENT_FILES = ASSETS / "versions";

static inline bool _fetchMcClient(const std::string &versionId,
                                  const fs::path &outputDirectory) {
  const NetworkManager &net = NetworkManager::getInstance();
  try {
    // output
    std::error_code ec;
    fs::create_directories(outputDirectory, ec);

    if (ec) {
      std::cerr << "Failed to create output directory: " << outputDirectory
                << "\n";
      return false;
    }

    // download manifest

    std::string manifestData;

    if (!net.download(MANIFEST_URL, &manifestData)) {
      std::cerr << "Failed to download version manifest\n";
      return false;
    }

    json manifest;

    try {
      manifest = json::parse(manifestData);
    } catch (const json::parse_error &e) {
      std::cerr << "Failed to parse version manifest: " << e.what() << "\n";
      return false;
    }

    if (!manifest.contains("versions") || !manifest["versions"].is_array()) {
      std::cerr << "Invalid version manifest\n";
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
      std::cerr << "Version not found: " << versionId << "\n";
      return false;
    }

    /// metadata section
    std::string versionData;

    if (!net.download(versionMetadataUrl, &versionData)) {
      std::cerr << "Failed to download metadata for " << versionId << "\n";
      return false;
    }

    json versionJson;

    try {
      versionJson = json::parse(versionData);
    } catch (const json::parse_error &e) {
      std::cerr << "Failed to parse version metadata: " << e.what() << "\n";
      return false;
    }

    // not needed but eh
    if (versionJson.value("id", "") != versionId) {
      std::cerr << "Version metadata ID mismatch\n";
      return false;
    }

    if (!versionJson.contains("downloads") ||
        !versionJson["downloads"].is_object() ||
        !versionJson["downloads"].contains("client")) {
      std::cerr << "No client download found for " << versionId << "\n";
      return false;
    }

    const auto &client = versionJson["downloads"]["client"];

    if (!client.is_object()) {
      std::cerr << "Invalid client metadata\n";
      return false;
    }

    const std::string clientUrl = client.value("url", "");

    const std::string expectedSha1 = client.value("sha1", "");

    const auto expectedSize = client.value("size", 0ULL);

    if (clientUrl.empty() || expectedSha1.empty()) {
      std::cerr << "Client metadata is incomplete\n";
      return false;
    }

    // SHA-1 should be a 40-character hex digest
    if (expectedSha1.size() != 40) {
      std::cerr << "Invalid SHA-1 in client metadata\n";
      return false;
    }

    const fs::path metadataPath = outputDirectory / (versionId + ".json");

    {
      std::ofstream metadataFile(metadataPath,
                                 std::ios::binary | std::ios::trunc);

      if (!metadataFile) {
        std::cerr << "Failed to open metadata file\n";
        return false;
      }

      metadataFile.write(versionData.data(),
                         static_cast<std::streamsize>(versionData.size()));

      if (!metadataFile) {
        std::cerr << "Failed to write metadata file\n";
        return false;
      }
    }

    // downloading the JAR
    const fs::path jarPath = outputDirectory / (versionId + ".jar");

    const fs::path tempJarPath = outputDirectory / (versionId + ".jar.tmp");

    fs::remove(tempJarPath, ec);

    std::cout << "Downloading Minecraft " << versionId << "...\n";

    if (!net.download(clientUrl, tempJarPath, true)) {
      std::cerr << "Failed to download client JAR\n";
      fs::remove(tempJarPath, ec);
      return false;
    }

    // Verify
    const auto actualSize = fs::file_size(tempJarPath, ec);

    if (ec) {
      std::cerr << "Failed to read downloaded JAR size\n";
      fs::remove(tempJarPath, ec);
      return false;
    }

    if (expectedSize != 0 && actualSize != expectedSize) {
      std::cerr << "JAR size mismatch\n"
                << "Expected: " << expectedSize << "\n"
                << "Actual:   " << actualSize << "\n";

      fs::remove(tempJarPath, ec);
      return false;
    }

    // TODO
    const auto sha1 = std::optional<std::string>(expectedSha1);

    if (!sha1) {
      std::cerr << "Failed to calculate JAR SHA-1\n";
      fs::remove(tempJarPath, ec);
      return false;
    }

    if (*sha1 != expectedSha1) {
      std::cerr << "JAR SHA-1 mismatch\n"
                << "Expected: " << expectedSha1 << "\n"
                << "Actual:   " << *sha1 << "\n";

      fs::remove(tempJarPath, ec);
      return false;
    }

    // Override

    fs::remove(jarPath, ec);

    fs::rename(tempJarPath, jarPath, ec);

    if (ec) {
      std::cerr << "Failed to finalize JAR: " << ec.message() << "\n";

      fs::remove(tempJarPath, ec);
      return false;
    }

    std::cout << "Downloaded Minecraft " << versionId << "\n"
              << "Metadata: " << metadataPath << "\n"
              << "Client:   " << jarPath << "\n";

    return true;
  } catch (const std::exception &e) {
    std::cerr << "Unexpected error: " << e.what() << "\n";
    return false;
  }
}

/// returns the path to the JAR of a Minecraft version
/// fetches the JAR if not present
inline fs::path tryGetMcClient(const std::string &versionId) {
  try {
    bool completionStatus = false;

    if (!fs::exists(CLIENT_FILES)) {
      std::cout << "First run, creating directories";
      fs::create_directory(CLIENT_FILES);
    }

    fs::path versionPath = CLIENT_FILES / versionId;
    if (!fs::exists(versionPath)) {
      fs::create_directory(versionPath);
    }

    fs::path versionJar = versionPath / (versionId + ".jar");
    if (!fs::exists(versionJar)) {
      std::cout << "Version JAR for " << versionId
                << " not found. Downloading.\n";
      completionStatus = _fetchMcClient(versionId, versionPath);
    } else {
      completionStatus = true;
    }

    if (!completionStatus) {
      std::cerr << "Client file search failed. Exiting.\n";
      exitProgram(ExitStatus::ERROR);
    }

    return versionJar;
  } catch (const fs::filesystem_error &e) {
    std::cerr << "Filesystem error: " << e.what() << '\n';
    exitProgram(ExitStatus::ERROR,
                "This executable might have executed at a location with "
                "insufficient permissions. Move it somewhere else or execute "
                "it as superuser/administrator.");
  } catch (const std::exception &e) {
    std::cerr << "Unexpected exception: " << e.what() << '\n';
    exitProgram(ExitStatus::ERROR,
                "This might be a bug, please report this to the developer");
  }
}

} // namespace enchantment_tweaks::app