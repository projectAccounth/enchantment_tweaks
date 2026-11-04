#pragma once

#include <RmlUi/Core/RenderInterface.h>

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

class SoftRenderer final : public Rml::RenderInterface {
public:
  SoftRenderer(int width, int height);

  void BeginFrame();
  bool SavePng(const std::string &path, uint8_t br, uint8_t bg,
               uint8_t bb) const;

  Rml::CompiledGeometryHandle
  CompileGeometry(Rml::Span<const Rml::Vertex> vertices,
                  Rml::Span<const int> indices) override;
  void RenderGeometry(Rml::CompiledGeometryHandle geometry,
                      Rml::Vector2f translation,
                      Rml::TextureHandle texture) override;
  void ReleaseGeometry(Rml::CompiledGeometryHandle geometry) override;
  Rml::TextureHandle LoadTexture(Rml::Vector2i &dimensions,
                                 const Rml::String &source) override;
  Rml::TextureHandle GenerateTexture(Rml::Span<const Rml::byte> data,
                                     Rml::Vector2i dimensions) override;
  void ReleaseTexture(Rml::TextureHandle texture) override;
  void EnableScissorRegion(bool enable) override;
  void SetScissorRegion(Rml::Rectanglei region) override;

private:
  struct Geometry {
    std::vector<Rml::Vertex> vertices;
    std::vector<int> indices;
  };
  struct Texture {
    int w = 0, h = 0;
    std::vector<uint8_t> rgba; // premultiplied
  };

  int w_, h_;
  std::vector<float> fb_; // premultiplied RGBA
  bool scissorOn_ = false;
  int sx0_ = 0, sy0_ = 0, sx1_ = 0, sy1_ = 0;
  std::unordered_map<uintptr_t, Geometry> geometry_;
  std::unordered_map<uintptr_t, Texture> textures_;
  uintptr_t next_ = 1;
};
