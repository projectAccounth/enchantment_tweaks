#include "core/util/image.hpp"
#include "libraries/stb/stb_image.h"
#include "libraries/stb/stb_image_write.h"

// Implemented in src/libraries/stb_image_impl.cpp
extern "C" unsigned char *stbi_write_png_to_mem(const unsigned char *pixels,
                                                int stride_bytes, int x, int y,
                                                int n, int *out_len);

#include <cstdlib>
#include <stdexcept>
#include <vector>

namespace enchantment_tweaks::image {

Image Image::load(const std::string &path) {
  int w = 0, h = 0, sourceChannels = 0;
  unsigned char *data =
      stbi_load(path.c_str(), &w, &h, &sourceChannels, 4 /* force RGBA */);

  if (!data) {
    const char *reason = stbi_failure_reason();
    throw std::runtime_error(
        "enchantment_tweaks::image::Image::load: failed to load '" + path +
        "': " + (reason ? reason : "unknown error"));
  }

  std::vector<uint8_t> pixels(data, data + (static_cast<size_t>(w) * h * 4));
  stbi_image_free(data);

  return {w, h, std::move(pixels)};
}

Pixel Image::at(int x, int y) const {
  if (x < 0 || x >= width_ || y < 0 || y >= height_) {
    throw std::out_of_range("enchantment_tweaks::image::Image::at: (" +
                            std::to_string(x) + ", " + std::to_string(y) +
                            ") out of range");
  }
  const size_t idx = (static_cast<size_t>(y) * width_ + x) * 4;
  return {.r = pixels_[idx + 0],
          .g = pixels_[idx + 1],
          .b = pixels_[idx + 2],
          .a = pixels_[idx + 3]};
}

bool Image::isOpaque(int x, int y) const { return at(x, y).a != 0; }

void Image::setPixel(int x, int y, const Pixel &p) {
  if (x < 0 || x >= width_ || y < 0 || y >= height_) {
    throw std::out_of_range("enchantment_tweaks::image::Image::setPixel: (" +
                            std::to_string(x) + ", " + std::to_string(y) +
                            ") out of range");
  }
  const size_t idx = (static_cast<size_t>(y) * width_ + x) * 4;
  pixels_[idx + 0] = p.r;
  pixels_[idx + 1] = p.g;
  pixels_[idx + 2] = p.b;
  pixels_[idx + 3] = p.a;
}

void Image::save(const std::string &path) const {
  if (width_ == 0 || height_ == 0) {
    throw std::runtime_error(
        "enchantment_tweaks::image::Image::save: empty image");
  }
  int result = stbi_write_png(path.c_str(), width_, height_, 4, pixels_.data(),
                              width_ * 4);
  if (result == 0) {
    throw std::runtime_error(
        "enchantment_tweaks::image::Image::save: failed to write '" + path +
        "'");
  }
}

Image Image::scaledNearest(int factor) const {
  if (factor < 1)
    throw std::invalid_argument("scale factor must be at least 1");
  if (width_ == 0 || height_ == 0)
    throw std::runtime_error("scaledNearest: empty image");
  Image out(width_ * factor, height_ * factor);
  for (int y = 0; y < out.height(); ++y) {
    for (int x = 0; x < out.width(); ++x) {
      out.setPixel(x, y, at(x / factor, y / factor));
    }
  }
  return out;
}

std::vector<uint8_t> Image::encodePng() const {
  if (width_ == 0 || height_ == 0) {
    throw std::runtime_error(
        "enchantment_tweaks::image::Image::encodePng: empty image");
  }
  int length = 0;
  unsigned char *encoded = stbi_write_png_to_mem(pixels_.data(), width_ * 4,
                                                 width_, height_, 4, &length);
  if (!encoded) {
    throw std::runtime_error(
        "enchantment_tweaks::image::Image::encodePng: encoder failed");
  }
  std::vector<uint8_t> out(encoded, encoded + length);
  std::free(encoded);
  return out;
}

} // namespace enchantment_tweaks::image
