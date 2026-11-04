#include "core/util/zip_streaming.hpp"

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

constexpr uint16_t kDosTime = 0;
constexpr uint16_t kDosDate = (1U << 5) | 1U; // 1980-01-01

void put16(std::vector<uint8_t> &out, uint16_t v) {
  out.push_back(static_cast<uint8_t>(v & 0xFF));
  out.push_back(static_cast<uint8_t>(v >> 8));
}

void put32(std::vector<uint8_t> &out, uint32_t v) {
  put16(out, static_cast<uint16_t>(v & 0xFFFF));
  put16(out, static_cast<uint16_t>(v >> 16));
}

void writeBytes(std::ofstream &out, const std::vector<uint8_t> &bytes) {
  out.write(reinterpret_cast<const char *>(bytes.data()),
            static_cast<std::streamsize>(bytes.size()));
}

} // namespace

uint32_t crc32Of(const std::vector<uint8_t> &bytes) {
  static const auto table = [] {
    std::array<uint32_t, 256> t{};
    for (uint32_t n = 0; n < 256; ++n) {
      uint32_t c = n;
      for (int k = 0; k < 8; ++k)
        c = (c & 1U) ? 0xEDB88320U ^ (c >> 1) : c >> 1;
      t[n] = c;
    }
    return t;
  }();
  uint32_t c = 0xFFFFFFFFU;
  for (uint8_t b : bytes)
    c = table[(c ^ b) & 0xFFU] ^ (c >> 8);
  return c ^ 0xFFFFFFFFU;
}

StreamingZipWriter::StreamingZipWriter(const fs::path &destination)
    : destination_(destination) {
  if (destination.has_parent_path())
    fs::create_directories(destination.parent_path());
  out_.open(destination, std::ios::binary | std::ios::trunc);
  if (!out_) throw std::runtime_error("cannot create " + destination.string());
}

void StreamingZipWriter::add(const std::string &path,
                             const std::vector<uint8_t> &bytes) {
  if (closed_) throw std::logic_error("zip already closed");
  if (path.empty())
    throw std::invalid_argument("zip entry path cannot be empty");
  if (!names_.insert(path).second)
    throw std::invalid_argument("duplicate zip entry: " + path);
  if (bytes.size() > 0xFFFFFFFFULL || offset_ > 0xFFFFFFFFULL) {
    throw std::runtime_error("archive too large for the ZIP format");
  }
  const auto size = static_cast<uint32_t>(bytes.size());
  const uint32_t crc = crc32Of(bytes);
  const auto nameLength = static_cast<uint16_t>(path.size());

  std::vector<uint8_t> header;
  put32(header, 0x04034B50);
  put16(header, 20);
  put16(header, 0x0800);
  put16(header, 0);
  put16(header, kDosTime);
  put16(header, kDosDate);
  put32(header, crc);
  put32(header, size);
  put32(header, size);
  put16(header, nameLength);
  put16(header, 0);
  writeBytes(out_, header);
  out_.write(path.data(), static_cast<std::streamsize>(path.size()));
  writeBytes(out_, bytes);
  if (!out_)
    throw std::runtime_error("failed writing " + destination_.string());

  entries_.push_back({path, crc, size, static_cast<uint32_t>(offset_)});
  offset_ += header.size() + path.size() + bytes.size();
}

void StreamingZipWriter::close() {
  if (closed_) return;
  closed_ = true;
  const uint64_t centralStart = offset_;
  std::vector<uint8_t> central;
  for (const auto &e : entries_) {
    put32(central, 0x02014B50);
    put16(central, 20);
    put16(central, 20);
    put16(central, 0x0800);
    put16(central, 0);
    put16(central, kDosTime);
    put16(central, kDosDate);
    put32(central, e.crc);
    put32(central, e.size);
    put32(central, e.size);
    put16(central, static_cast<uint16_t>(e.path.size()));
    put16(central, 0);
    put16(central, 0);
    put16(central, 0);
    put16(central, 0);
    put32(central, 0);
    put32(central, e.offset);
    central.insert(central.end(), e.path.begin(), e.path.end());
  }
  writeBytes(out_, central);
  std::vector<uint8_t> end;
  put32(end, 0x06054B50);
  put16(end, 0);
  put16(end, 0);
  put16(end, static_cast<uint16_t>(entries_.size()));
  put16(end, static_cast<uint16_t>(entries_.size()));
  put32(end, static_cast<uint32_t>(central.size()));
  put32(end, static_cast<uint32_t>(centralStart));
  put16(end, 0);
  writeBytes(out_, end);
  out_.close();
  if (!out_)
    throw std::runtime_error("failed finishing " + destination_.string());
}

} // namespace enchantment_tweaks::zip
