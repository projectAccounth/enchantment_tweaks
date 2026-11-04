#pragma once

#include <cstdint>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

namespace enchantment_tweaks::image {

struct Pixel {
  uint8_t r = 0;
  uint8_t g = 0;
  uint8_t b = 0;
  uint8_t a = 0;

  bool operator==(const Pixel &other) const {
    return r == other.r && g == other.g && b == other.b && a == other.a;
  }
};

class Image {
public:
  Image() = default;

  Image(int w, int h)
      : width_(w), height_(h), pixels_(static_cast<size_t>(w) * h * 4, 0) {}

  Image(int w, int h, std::vector<uint8_t> pixels)
      : width_(w), height_(h), pixels_(std::move(pixels)) {}

  [[nodiscard]] static Image load(const std::string &path);

  [[nodiscard]] int width() const noexcept { return width_; }
  [[nodiscard]] int height() const noexcept { return height_; }
  [[nodiscard]] bool empty() const noexcept {
    return width_ == 0 || height_ == 0;
  }

  [[nodiscard]] Pixel at(int x, int y) const;

  [[nodiscard]] bool isOpaque(int x, int y) const;

  [[nodiscard]] const std::vector<uint8_t> &pixels() const noexcept {
    return pixels_;
  }
  [[nodiscard]] std::vector<uint8_t> &pixels() noexcept { return pixels_; }

  void setPixel(int x, int y, const Pixel &p);
  void save(const std::string &path) const;

  [[nodiscard]] Image scaledNearest(int factor) const;
  [[nodiscard]] std::vector<uint8_t> encodePng() const;

private:
  int width_ = 0;
  int height_ = 0;
  std::vector<uint8_t> pixels_; // RGBA8, row-major, top-to-bottom
};

} // namespace enchantment_tweaks::image
