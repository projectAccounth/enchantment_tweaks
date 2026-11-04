#include "soft_renderer.hpp"

#include "RmlUi/Config/Config.h"
#include "RmlUi/Core/Colour.h"
#include "RmlUi/Core/Math.h"
#include "RmlUi/Core/Span.h"
#include "RmlUi/Core/Types.h"
#include "RmlUi/Core/Vertex.h"
#include "core/util/image.hpp"
#include "libraries/stb/stb_image.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <ios>
#include <string>
#include <utility>
#include <vector>

namespace {

struct Pt {
  double x, y;
};
double edge(const Pt &a, const Pt &b, const Pt &p) {
  return (b.x - a.x) * (p.y - a.y) - (b.y - a.y) * (p.x - a.x);
}

} // namespace

SoftRenderer::SoftRenderer(int width, int height)
    : w_(width), h_(height), fb_(static_cast<size_t>(width) * height * 4, 0.f) {
  sx1_ = w_;
  sy1_ = h_;
}

void SoftRenderer::BeginFrame() { std::fill(fb_.begin(), fb_.end(), 0.f); }

bool SoftRenderer::SavePng(const std::string &path, uint8_t br, uint8_t bg,
                           uint8_t bb) const {
  std::vector<uint8_t> rgba(static_cast<size_t>(w_) * h_ * 4);
  for (size_t i = 0; i < static_cast<size_t>(w_) * h_; ++i) {
    const float a = fb_[i * 4 + 3];
    const float bgv[3] = {br / 255.f, bg / 255.f, bb / 255.f};
    for (int c = 0; c < 3; ++c) {
      const float v = fb_[i * 4 + c] + bgv[c] * (1.f - a);
      rgba[i * 4 + c] =
          static_cast<uint8_t>(std::lround(std::clamp(v, 0.f, 1.f) * 255.f));
    }
    rgba[i * 4 + 3] = 255;
  }
  const auto png = enchantment_tweaks::image::Image(w_, h_, rgba).encodePng();
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  out.write(reinterpret_cast<const char *>(png.data()),
            static_cast<std::streamsize>(png.size()));
  return static_cast<bool>(out);
}

Rml::CompiledGeometryHandle
SoftRenderer::CompileGeometry(Rml::Span<const Rml::Vertex> vertices,
                              Rml::Span<const int> indices) {
  Geometry g;
  g.vertices.assign(vertices.begin(), vertices.end());
  g.indices.assign(indices.begin(), indices.end());
  const uintptr_t id = next_++;
  geometry_[id] = std::move(g);
  return id;
}

void SoftRenderer::ReleaseGeometry(Rml::CompiledGeometryHandle h) {
  geometry_.erase(h);
}

Rml::TextureHandle SoftRenderer::LoadTexture(Rml::Vector2i &dimensions,
                                             const Rml::String &source) {
  int w = 0, h = 0, n = 0;
  unsigned char *px = stbi_load(source.c_str(), &w, &h, &n, 4);
  if (!px) return 0;
  Texture t;
  t.w = w;
  t.h = h;
  t.rgba.assign(px, px + static_cast<size_t>(w) * h * 4);
  stbi_image_free(px);
  for (size_t i = 0; i < t.rgba.size(); i += 4) {
    for (int c = 0; c < 3; ++c)
      t.rgba[i + c] = static_cast<uint8_t>(t.rgba[i + c] * t.rgba[i + 3] / 255);
  }
  dimensions = {w, h};
  const uintptr_t id = next_++;
  textures_[id] = std::move(t);
  return id;
}

Rml::TextureHandle
SoftRenderer::GenerateTexture(Rml::Span<const Rml::byte> data,
                              Rml::Vector2i dimensions) {
  Texture t;
  t.w = dimensions.x;
  t.h = dimensions.y;
  t.rgba.assign(data.begin(), data.end());
  const uintptr_t id = next_++;
  textures_[id] = std::move(t);
  return id;
}

void SoftRenderer::ReleaseTexture(Rml::TextureHandle h) { textures_.erase(h); }

void SoftRenderer::EnableScissorRegion(bool enable) { scissorOn_ = enable; }

void SoftRenderer::SetScissorRegion(Rml::Rectanglei r) {
  sx0_ = std::max(0, r.Left());
  sy0_ = std::max(0, r.Top());
  sx1_ = std::min(w_, r.Right());
  sy1_ = std::min(h_, r.Bottom());
}

void SoftRenderer::RenderGeometry(Rml::CompiledGeometryHandle handle,
                                  Rml::Vector2f translation,
                                  Rml::TextureHandle texture) {
  auto git = geometry_.find(handle);
  if (git == geometry_.end()) return;
  const Geometry &g = git->second;
  const Texture *tex = nullptr;
  if (texture) {
    auto tit = textures_.find(texture);
    if (tit != textures_.end()) tex = &tit->second;
  }
  const int cx0 = scissorOn_ ? sx0_ : 0, cy0 = scissorOn_ ? sy0_ : 0;
  const int cx1 = scissorOn_ ? sx1_ : w_, cy1 = scissorOn_ ? sy1_ : h_;

  auto sample = [&](float u, float v, float out[4]) {
    const float fx = std::clamp(u, 0.f, 1.f) * tex->w - 0.5f;
    const float fy = std::clamp(v, 0.f, 1.f) * tex->h - 0.5f;
    const int x0 = static_cast<int>(std::floor(fx)),
              y0 = static_cast<int>(std::floor(fy));
    const float tx = fx - x0, ty = fy - y0;
    auto at = [&](int x, int y, int c) {
      x = std::clamp(x, 0, tex->w - 1);
      y = std::clamp(y, 0, tex->h - 1);
      return tex->rgba[(static_cast<size_t>(y) * tex->w + x) * 4 + c] / 255.f;
    };
    for (int c = 0; c < 4; ++c) {
      out[c] = (at(x0, y0, c) * (1 - tx) + at(x0 + 1, y0, c) * tx) * (1 - ty) +
               (at(x0, y0 + 1, c) * (1 - tx) + at(x0 + 1, y0 + 1, c) * tx) * ty;
    }
  };

  for (size_t i = 0; i + 2 < g.indices.size(); i += 3) {
    const Rml::Vertex *va = &g.vertices[g.indices[i]];
    const Rml::Vertex *vb = &g.vertices[g.indices[i + 1]];
    const Rml::Vertex *vc = &g.vertices[g.indices[i + 2]];
    Pt a{va->position.x + translation.x, va->position.y + translation.y};
    Pt b{vb->position.x + translation.x, vb->position.y + translation.y};
    Pt c{vc->position.x + translation.x, vc->position.y + translation.y};
    double area = edge(a, b, c);
    if (std::fabs(area) < 1e-9) continue;
    if (area < 0) {
      std::swap(b, c);
      std::swap(vb, vc);
      area = -area;
    }
    const int x0 =
        std::max(cx0, static_cast<int>(std::floor(std::min({a.x, b.x, c.x}))));
    const int x1 =
        std::min(cx1, static_cast<int>(std::ceil(std::max({a.x, b.x, c.x}))));
    const int y0 =
        std::max(cy0, static_cast<int>(std::floor(std::min({a.y, b.y, c.y}))));
    const int y1 =
        std::min(cy1, static_cast<int>(std::ceil(std::max({a.y, b.y, c.y}))));
    for (int y = y0; y < y1; ++y) {
      for (int x = x0; x < x1; ++x) {
        const Pt p{x + 0.5, y + 0.5};
        const double e0 = edge(b, c, p), e1 = edge(c, a, p), e2 = edge(a, b, p);
        if (e0 < 0 || e1 < 0 || e2 < 0) continue;
        const float l0 = static_cast<float>(e0 / area),
                    l1 = static_cast<float>(e1 / area),
                    l2 = static_cast<float>(e2 / area);
        float col[4];
        col[0] =
            (va->colour.red * l0 + vb->colour.red * l1 + vc->colour.red * l2) /
            255.f;
        col[1] = (va->colour.green * l0 + vb->colour.green * l1 +
                  vc->colour.green * l2) /
                 255.f;
        col[2] = (va->colour.blue * l0 + vb->colour.blue * l1 +
                  vc->colour.blue * l2) /
                 255.f;
        col[3] = (va->colour.alpha * l0 + vb->colour.alpha * l1 +
                  vc->colour.alpha * l2) /
                 255.f;
        if (tex) {
          float t[4];
          sample(va->tex_coord.x * l0 + vb->tex_coord.x * l1 +
                     vc->tex_coord.x * l2,
                 va->tex_coord.y * l0 + vb->tex_coord.y * l1 +
                     vc->tex_coord.y * l2,
                 t);
          for (int k = 0; k < 4; ++k)
            col[k] *= t[k];
        }
        float *d = &fb_[(static_cast<size_t>(y) * w_ + x) * 4];
        const float inv = 1.f - col[3];
        for (int k = 0; k < 4; ++k)
          d[k] = col[k] + d[k] * inv;
      }
    }
  }
}
