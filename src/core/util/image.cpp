#include "core/util/image.hpp"
#include "libraries/stb/stb_image.h"

#include <stdexcept>

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

} // namespace enchantment_tweaks::image
