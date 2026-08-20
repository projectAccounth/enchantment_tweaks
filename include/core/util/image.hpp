#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace enchantment_tweaks::image {

struct Pixel {
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
    uint8_t a = 0;
};

class Image {
public:
    Image() = default;

    static Image Load(const std::string& path);

    int width() const noexcept { return width_; }
    int height() const noexcept { return height_; }
    bool empty() const noexcept { return width_ == 0 || height_ == 0; }

    Pixel at(int x, int y) const;

    bool isOpaque(int x, int y) const;

private:
    Image(int w, int h, std::vector<uint8_t> pixels)
        : width_(w), height_(h), pixels_(std::move(pixels)) {}

    int width_ = 0;
    int height_ = 0;
    std::vector<uint8_t> pixels_;  // RGBA8, row-major, top-to-bottom
};

}  // namespace enchantment_tweaks::image