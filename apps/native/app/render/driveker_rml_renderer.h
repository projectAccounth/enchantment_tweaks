#pragma once

#include "RmlUi/Config/Config.h"
#include "RmlUi/Core/Colour.h"
#include "RmlUi/Core/Debug.h"
#include "RmlUi/Core/Math.h"
#include "RmlUi/Core/Span.h"
#include "RmlUi/Core/Types.h"
#include "RmlUi/Core/Vertex.h"
#include <RmlUi/Core/RenderInterface.h>
#include <driveker/core/pipeline_object.hpp>
#include <driveker/core/render_device.hpp>
#include <driveker/core/render_types.hpp>
#include <driveker/core/texture.hpp>

#include <cstdint>
#include <filesystem>
#include <unordered_map>

class DrivekerRmlRenderer final : public Rml::RenderInterface {
public:
  DrivekerRmlRenderer(int width, int height,
                      const std::filesystem::path &assets_directory);
  ~DrivekerRmlRenderer() override;

  void SetViewport(int width, int height);
  void BeginFrame(float red = 0.055f, float green = 0.063f,
                  float blue = 0.082f);
  void EndFrame();

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
    driveker::BufferHandle vertices, indices;
    std::uint32_t index_count = 0;
  };
  driveker::Texture CreateTexture(Rml::Span<const Rml::byte> rgba,
                                  Rml::Vector2i dimensions);

  driveker::RenderDevice &device_;
  driveker::PipelineObject pipeline_;
  driveker::Texture white_texture_;
  int width_, height_;
  bool scissor_enabled_ = false;
  Rml::Rectanglei scissor_{};
  std::unordered_map<uintptr_t, Geometry> geometry_;
  std::unordered_map<uintptr_t, driveker::Texture> textures_;
  uintptr_t next_handle_ = 1;
};
