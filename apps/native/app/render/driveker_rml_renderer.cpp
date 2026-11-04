#include "driveker_rml_renderer.h"
#include "RmlUi/Config/Config.h"
#include "RmlUi/Core/Colour.h"
#include "RmlUi/Core/Debug.h"
#include "RmlUi/Core/Math.h"
#include "RmlUi/Core/Span.h"
#include "RmlUi/Core/Types.h"
#include "RmlUi/Core/Vertex.h"
#include "libraries/stb/stb_image.h"
#include <cstddef>
#include <driveker/core/render_state.hpp>
#include <driveker/core/texture.hpp>

#include <driveker/core/render_system.hpp>
#include <driveker/core/render_types.hpp>
#include <driveker/core/vertex_format.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <span>
#include <string>
#include <vector>

namespace {
struct GpuVertex {
  float x, y;
  uint8_t r, g, b, a;
  float u, v;
};

std::string loadShader(const std::filesystem::path &path) {
  std::ifstream file(path, std::ios::binary);
  if (!file)
    throw std::runtime_error("Could not open shader file: " + path.string());

  std::string source((std::istreambuf_iterator<char>(file)),
                     std::istreambuf_iterator<char>{});
  if (file.bad())
    throw std::runtime_error("Could not read shader file: " + path.string());
  if (source.empty())
    throw std::runtime_error("Shader file is empty: " + path.string());
  return source;
}

constexpr static driveker::VertexFormat uiVertexFormat() {
  return driveker::VertexFormat{{
      {.semantic = driveker::VertexSemantic::POSITION,
       .format = driveker::VertexAttributeFormat::FLOAT2},
      {.semantic = driveker::VertexSemantic::COLOR,
       .format = driveker::VertexAttributeFormat::UNORM8X4},
      {.semantic = driveker::VertexSemantic::UV0,
       .format = driveker::VertexAttributeFormat::FLOAT2},
  }};
}

std::vector<Rml::byte> premultiply(Rml::Span<const Rml::byte> rgba) {
  std::vector<Rml::byte> result(rgba.begin(), rgba.end());
  for (size_t i = 0; i + 3 < result.size(); i += 4)
    for (size_t c = 0; c < 3; ++c)
      result[i + c] =
          static_cast<Rml::byte>(int(result[i + c]) * int(result[i + 3]) / 255);
  return result;
}
} // namespace

DrivekerRmlRenderer::DrivekerRmlRenderer(
    int width, int height, const std::filesystem::path &assets_directory)
    : device_(driveker::RenderSystem::getDevice()), width_(width),
      height_(height) {
  const auto vertexShader = assets_directory / "shaders" / "ui.vert";
  const auto fragmentShader = assets_directory / "shaders" / "ui.frag";
  const auto vertexSource = loadShader(vertexShader);
  const auto fragmentSource = loadShader(fragmentShader);
  const auto shader = driveker::ShaderProgramDesc{
      .vertexSource = vertexSource,
      .fragmentSource = fragmentSource,
      .vertexSourceName = vertexShader.string(),
      .fragmentSourceName = fragmentShader.string()};
  pipeline_ = device_.createPipeline(
      driveker::PipelineDesc::builder(shader)
          .vertexFormat(uiVertexFormat())
          .blend(driveker::BlendSourceFactor::ONE,
                 driveker::BlendSourceFactor::ONE,
                 driveker::BlendDestFactor::ONE_MINUS_SRC_ALPHA,
                 driveker::BlendDestFactor::ONE_MINUS_SRC_ALPHA)
          .depthTest(false)
          .depthWrite(false)
          .noCull()
          .build());

  const std::array<Rml::byte, 4> white{255, 255, 255, 255};
  white_texture_ = CreateTexture({white.data(), white.size()}, {1, 1});
}

DrivekerRmlRenderer::~DrivekerRmlRenderer() {
  for (auto &[_, geometry] : geometry_) {
    device_.destroyBuffer(geometry.indices);
    device_.destroyBuffer(geometry.vertices);
  }
  for (auto &[_, texture] : textures_)
    device_.destroyTexture(texture);
  if (white_texture_.valid()) device_.destroyTexture(white_texture_);
  if (pipeline_.valid()) device_.destroyPipeline(pipeline_);
}

void DrivekerRmlRenderer::SetViewport(int width, int height) {
  width_ = width;
  height_ = height;
}
void DrivekerRmlRenderer::BeginFrame(float red, float green, float blue) {
  device_.beginFrame();
  device_.setViewport({.x = 0, .y = 0, .width = width_, .height = height_});
  device_.clear({.red = red, .green = green, .blue = blue, .alpha = 1.f}, true,
                false, false);
}
void DrivekerRmlRenderer::EndFrame() {
  device_.setScissor(false);
  device_.endFrame();
}

Rml::CompiledGeometryHandle
DrivekerRmlRenderer::CompileGeometry(Rml::Span<const Rml::Vertex> vertices,
                                     Rml::Span<const int> indices) {
  if (vertices.empty() || indices.empty()) return 0;
  std::vector<GpuVertex> packed;
  packed.reserve(vertices.size());
  for (const auto &v : vertices)
    packed.push_back({.x = v.position.x,
                      .y = v.position.y,
                      .r = v.colour.red,
                      .g = v.colour.green,
                      .b = v.colour.blue,
                      .a = v.colour.alpha,
                      .u = v.tex_coord.x,
                      .v = v.tex_coord.y});

  std::vector<uint32_t> packedIndices;
  packedIndices.reserve(indices.size());
  for (int i : indices) {
    if (i < 0 || static_cast<size_t>(i) >= vertices.size()) return 0;
    packedIndices.push_back(static_cast<uint32_t>(i));
  }

  const uintptr_t handle = next_handle_++;
  geometry_.emplace(
      handle,
      Geometry{.vertices = device_.createBuffer(
                   {.size = sizeof(GpuVertex) * packed.size(),
                    .usage = driveker::BufferUsage::VERTEX},
                   std::as_bytes(std::span{packed})),

               .indices = device_.createBuffer(
                   {.size = sizeof(uint32_t) * packedIndices.size(),
                    .usage = driveker::BufferUsage::INDEX},
                   std::as_bytes(std::span{packedIndices})),

               .index_count = static_cast<uint32_t>(packedIndices.size())});
  return handle;
}

void DrivekerRmlRenderer::RenderGeometry(Rml::CompiledGeometryHandle handle,
                                         Rml::Vector2f translation,
                                         Rml::TextureHandle texture) {
  const auto geometry = geometry_.find(handle);
  if (geometry == geometry_.end()) return;
  const auto image = textures_.find(texture);
  const driveker::Texture &activeTex =
      image == textures_.end() ? white_texture_ : image->second;
  if (scissor_enabled_) {
    const int x = std::clamp(scissor_.Left(), 0, width_),
              y = std::clamp(scissor_.Top(), 0, height_);
    const int right = std::clamp(scissor_.Right(), x, width_),
              bottom = std::clamp(scissor_.Bottom(), y, height_);
    device_.setScissor(true, {.x = x,
                              .y = height_ - bottom,
                              .width = right - x,
                              .height = bottom - y});
  }

  // batch the entire Rml pass into one render pass call
  auto pass = device_.createRenderPass();
  auto &draw = pass.emplaceDraw();
  draw.pipeline = pipeline_.handle();
  draw.indexed = true;
  draw.indexBuffer = geometry->second.indices;
  draw.count = geometry->second.index_count;
  draw.topology = driveker::PrimitiveTopology::TRIANGLES;
  draw.vertexBuffers.push_back(
      {.slot = 0, .buffer = geometry->second.vertices, .offset = 0});
  pass.setUniform2f("u_translation", translation.x, translation.y)
      .setUniform2f("u_viewport", static_cast<float>(width_),
                    static_cast<float>(height_))
      .bindTexture("u_texture", activeTex, 0);
  device_.submit(pass);
}

void DrivekerRmlRenderer::ReleaseGeometry(Rml::CompiledGeometryHandle handle) {
  if (auto it = geometry_.find(handle); it != geometry_.end()) {
    device_.destroyBuffer(it->second.indices);
    device_.destroyBuffer(it->second.vertices);
    geometry_.erase(it);
  }
}
driveker::Texture
DrivekerRmlRenderer::CreateTexture(Rml::Span<const Rml::byte> rgba,
                                   Rml::Vector2i size) {
  return device_.createTexture(
      {.width = size.x,
       .height = size.y,
       .colorFormat = driveker::ColorFormat::RGBA8,
       .hasColor = true,
       .hasDepth = false},
      {reinterpret_cast<const std::byte *>(rgba.data()), rgba.size()});
}
Rml::TextureHandle DrivekerRmlRenderer::LoadTexture(Rml::Vector2i &dimensions,
                                                    const Rml::String &source) {
  int width = 0, height = 0, channels = 0;
  stbi_uc *pixels = stbi_load(source.c_str(), &width, &height, &channels, 4);
  if (!pixels) return 0;
  const auto premultiplied =
      premultiply({reinterpret_cast<const Rml::byte *>(pixels),
                   static_cast<size_t>(width * height * 4)});
  stbi_image_free(pixels);
  dimensions = {width, height};
  const uintptr_t handle = next_handle_++;
  textures_.emplace(handle, CreateTexture(premultiplied, dimensions));
  return handle;
}
Rml::TextureHandle
DrivekerRmlRenderer::GenerateTexture(Rml::Span<const Rml::byte> data,
                                     Rml::Vector2i dimensions) {
  if (data.empty() || dimensions.x <= 0 || dimensions.y <= 0) return 0;
  const uintptr_t handle = next_handle_++;
  textures_.emplace(handle, CreateTexture(data, dimensions));
  return handle;
}
void DrivekerRmlRenderer::ReleaseTexture(Rml::TextureHandle texture) {
  if (auto it = textures_.find(texture); it != textures_.end()) {
    device_.destroyTexture(it->second);
    textures_.erase(it);
  }
}
void DrivekerRmlRenderer::EnableScissorRegion(bool enable) {
  scissor_enabled_ = enable;
  if (!enable) device_.setScissor(false);
}
void DrivekerRmlRenderer::SetScissorRegion(Rml::Rectanglei region) {
  scissor_ = region;
}
