#include "core/project/minecraft_download.hpp"

#include "core/types/common.hpp"
#include "libraries/miniz/miniz.h"
#include "libraries/miniz/miniz_common.h"
#include "libraries/miniz/miniz_zip.h"

#include <array>
#include <cstring>
#include <fstream>
#include <iterator>

namespace fs = std::filesystem;

namespace enchantment_tweaks::project {

namespace {

// Minimal SHA-1 (FIPS 180-4). Used only to check downloaded files.
std::string sha1(const std::vector<uint8_t> &message) {
  auto rotl = [](uint32_t x, int n) { return (x << n) | (x >> (32 - n)); };
  std::array<uint32_t, 5> h = {0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0};

  std::vector<uint8_t> data = message;
  const uint64_t bitLength = static_cast<uint64_t>(message.size()) * 8;
  data.push_back(0x80);
  while (data.size() % 64 != 56)
    data.push_back(0);
  for (int i = 7; i >= 0; --i)
    data.push_back(static_cast<uint8_t>(bitLength >> (i * 8)));

  for (size_t chunk = 0; chunk < data.size(); chunk += 64) {
    std::array<uint32_t, 80> w{};
    for (size_t i = 0; i < 16; ++i) {
      const size_t at = chunk + 4 * i;
      w[i] = (uint32_t(data[at]) << 24) | (uint32_t(data[at + 1]) << 16) |
             (uint32_t(data[at + 2]) << 8) | uint32_t(data[at + 3]);
    }
    for (size_t i = 16; i < 80; ++i)
      w[i] = rotl(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);

    uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4];
    for (int i = 0; i < 80; ++i) {
      uint32_t f, k;
      if (i < 20) {
        f = (b & c) | (~b & d);
        k = 0x5A827999;
      } else if (i < 40) {
        f = b ^ c ^ d;
        k = 0x6ED9EBA1;
      } else if (i < 60) {
        f = (b & c) | (b & d) | (c & d);
        k = 0x8F1BBCDC;
      } else {
        f = b ^ c ^ d;
        k = 0xCA62C1D6;
      }
      const uint32_t temp = rotl(a, 5) + f + e + k + w[i];
      e = d;
      d = c;
      c = rotl(b, 30);
      b = a;
      a = temp;
    }
    h[0] += a;
    h[1] += b;
    h[2] += c;
    h[3] += d;
    h[4] += e;
  }

  static const char *hex = "0123456789abcdef";
  std::string out;
  for (uint32_t word : h) {
    for (int shift = 28; shift >= 0; shift -= 4)
      out += hex[(word >> shift) & 0xF];
  }
  return out;
}

} // namespace

VersionManifest parseVersionManifest(const std::string &text) {
  VersionManifest out;
  json j = json::parse(text, nullptr, /*allow_exceptions=*/false);
  if (!j.is_object() || !j.contains("versions") || !j["versions"].is_array()) {
    out.error = "the version list is not in the expected format";
    return out;
  }
  for (const auto &entry : j["versions"]) {
    if (!entry.is_object()) continue;
    RemoteVersion v;
    v.id = entry.value("id", std::string());
    v.type = entry.value("type", std::string());
    v.url = entry.value("url", std::string());
    if (!v.id.empty() && !v.url.empty()) out.versions.push_back(std::move(v));
  }
  if (out.versions.empty()) out.error = "the version list is empty";
  return out;
}

ClientDownloadResult parseClientDownload(const std::string &text) {
  ClientDownloadResult result;
  json j = json::parse(text, nullptr, /*allow_exceptions=*/false);
  if (!j.is_object() || !j.contains("downloads") ||
      !j["downloads"].contains("client")) {
    result.error = "this version has no client download";
    return result;
  }
  const json &client = j["downloads"]["client"];
  ClientDownload d;
  d.url = client.value("url", std::string());
  d.sha1 = client.value("sha1", std::string());
  d.size = client.value("size", uint64_t{0});
  if (d.url.empty() || d.sha1.size() != 40) {
    result.error = "the client download is incomplete";
    return result;
  }
  result.download = std::move(d);
  return result;
}

std::string sha1Hex(const std::vector<uint8_t> &bytes) { return sha1(bytes); }

std::optional<int> resourceFormatOfJar(const fs::path &jar) {
  std::ifstream in(jar, std::ios::binary);
  if (!in) return std::nullopt;
  in.seekg(0, std::ios::end);
  const auto size = static_cast<std::streamsize>(in.tellg());
  in.seekg(0, std::ios::beg);
  std::vector<uint8_t> bytes(static_cast<size_t>(size));
  if (size > 0 && !in.read(reinterpret_cast<char *>(bytes.data()), size))
    return std::nullopt;
  mz_zip_archive zip{};
  if (!mz_zip_reader_init_mem(&zip, bytes.data(), bytes.size(), 0))
    return std::nullopt;

  std::optional<int> format;
  // Look the entry up directly instead of walking every entry of the archive.
  const int located =
      mz_zip_reader_locate_file(&zip, "version.json", nullptr, 0);
  if (located >= 0) {
    const auto index = static_cast<mz_uint>(located);
    size_t size = 0;
    void *data = mz_zip_reader_extract_to_heap(&zip, index, &size, 0);
    if (data) {
      const auto *first = static_cast<const char *>(data);
      const json version =
          json::parse(first, first + size, nullptr, /*allow_exceptions=*/false);
      mz_free(data);
      if (version.is_object() && version.contains("pack_version") &&
          version["pack_version"].is_object()) {
        const json &pv = version["pack_version"];
        if (pv.contains("resource") && pv["resource"].is_number_integer())
          format = pv["resource"].get<int>();
        else if (pv.contains("resource_major") &&
                 pv["resource_major"].is_number_integer())
          format = pv["resource_major"].get<int>();
      }
    }
  }
  mz_zip_reader_end(&zip);
  return format;
}

} // namespace enchantment_tweaks::project
