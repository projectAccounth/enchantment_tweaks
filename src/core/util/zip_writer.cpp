#include "core/util/zip_writer.hpp"

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace enchantment_tweaks::zip {

namespace {

uint32_t crc32(const std::vector<uint8_t> &data) {
  static const auto table = [] {
    std::array<uint32_t, 256> t{};
    for (uint32_t n = 0; n < 256; ++n) {
      uint32_t c = n;
      for (int k = 0; k < 8; ++k) {
        c = (c & 1U) ? 0xEDB88320U ^ (c >> 1) : c >> 1;
      }
      t[n] = c;
    }
    return t;
  }();
  uint32_t c = 0xFFFFFFFFU;
  for (uint8_t b : data) {
    c = table[(c ^ b) & 0xFFU] ^ (c >> 8);
  }
  return c ^ 0xFFFFFFFFU;
}

void put16(std::vector<uint8_t> &out, uint16_t v) {
  out.push_back(static_cast<uint8_t>(v & 0xFF));
  out.push_back(static_cast<uint8_t>(v >> 8));
}

void put32(std::vector<uint8_t> &out, uint32_t v) {
  put16(out, static_cast<uint16_t>(v & 0xFFFF));
  put16(out, static_cast<uint16_t>(v >> 16));
}

void putBytes(std::vector<uint8_t> &out, const std::string &s) {
  out.insert(out.end(), s.begin(), s.end());
}

// DOS date 1980-01-01 and time 00:00:00: fixed, so the output is reproducible.
constexpr uint16_t kDosTime = 0;
constexpr uint16_t kDosDate = (1U << 5) | 1U;

} // namespace

void ZipWriter::addFile(const std::string &path,
                        const std::vector<uint8_t> &bytes) {
  if (path.empty()) {
    throw std::invalid_argument("zip entry path cannot be empty");
  }
  for (const auto &e : entries_) {
    if (e.path == path) {
      throw std::invalid_argument("duplicate zip entry: " + path);
    }
  }
  entries_.push_back({path, bytes});
}

void ZipWriter::addFile(const std::string &path, const std::string &text) {
  addFile(path, std::vector<uint8_t>(text.begin(), text.end()));
}

std::vector<uint8_t> ZipWriter::finish() const {
  std::vector<uint8_t> out;
  std::vector<uint8_t> central;
  std::vector<uint32_t> offsets;

  for (const auto &e : entries_) {
    const auto offset = static_cast<uint32_t>(out.size());
    offsets.push_back(offset);
    const auto size = static_cast<uint32_t>(e.data.size());
    const uint32_t crc = crc32(e.data);
    const auto nameLen = static_cast<uint16_t>(e.path.size());

    // local file header
    put32(out, 0x04034B50);
    put16(out, 20);     // version needed
    put16(out, 0x0800); // flags: UTF-8 names
    put16(out, 0);      // method: stored
    put16(out, kDosTime);
    put16(out, kDosDate);
    put32(out, crc);
    put32(out, size);
    put32(out, size);
    put16(out, nameLen);
    put16(out, 0); // extra length
    putBytes(out, e.path);
    out.insert(out.end(), e.data.begin(), e.data.end());

    // central directory header
    put32(central, 0x02014B50);
    put16(central, 20); // version made by
    put16(central, 20); // version needed
    put16(central, 0x0800);
    put16(central, 0);
    put16(central, kDosTime);
    put16(central, kDosDate);
    put32(central, crc);
    put32(central, size);
    put32(central, size);
    put16(central, nameLen);
    put16(central, 0); // extra
    put16(central, 0); // comment
    put16(central, 0); // disk number
    put16(central, 0); // internal attributes
    put32(central, 0); // external attributes
    put32(central, offset);
    putBytes(central, e.path);
  }

  const auto centralOffset = static_cast<uint32_t>(out.size());
  out.insert(out.end(), central.begin(), central.end());

  const auto count = static_cast<uint16_t>(entries_.size());
  put32(out, 0x06054B50);
  put16(out, 0);     // this disk
  put16(out, 0);     // disk with central directory
  put16(out, count); // entries on this disk
  put16(out, count); // total entries
  put32(out, static_cast<uint32_t>(central.size()));
  put32(out, centralOffset);
  put16(out, 0); // comment length
  return out;
}

void ZipWriter::write(const fs::path &outPath) const {
  const auto bytes = finish();
  if (outPath.has_parent_path()) {
    fs::create_directories(outPath.parent_path());
  }
  std::ofstream out(outPath, std::ios::binary | std::ios::trunc);
  if (!out) {
    throw std::runtime_error("cannot open zip for writing: " +
                             outPath.string());
  }
  out.write(reinterpret_cast<const char *>(bytes.data()),
            static_cast<std::streamsize>(bytes.size()));
  if (!out) {
    throw std::runtime_error("failed writing zip: " + outPath.string());
  }
}

} // namespace enchantment_tweaks::zip
