#pragma once

#include "core/util/image.hpp"

#include <array>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace enchantment_tweaks::mesh {

constexpr double MODEL_UNITS = 16.0;

using Vec3 = std::array<double, 3>;
using UV = std::array<double, 4>;

struct Face {
  UV uv{};
  std::string texture; // e.g. "#texture"
};

struct Element {
  Vec3 from{};
  Vec3 to{};
  std::map<std::string, Face>
      faces; // keyed by "north", "south", "up", "down", "west", "east"
};

struct DisplayTransform {
  std::optional<Vec3> rotation;
  std::optional<Vec3> translation;
  std::optional<Vec3> scale;
};

struct Model {
  std::map<std::string, std::string> textures; // "texture" -> "ns:key", etc.
  std::vector<Element> elements;
  std::map<std::string, DisplayTransform> display;
};

struct BuildOptions {
  double thicknessPx = 1.0;
  std::string textureNamespace = "minecraft";
  std::string textureKey = "item/generated";
  bool cullTransparent = true;
  bool mergeColumns = false;

  // Optional expanded backface outline shell.
  double outlineSizePx = 0.0;
  std::optional<std::string> outlineTextureNamespace;
  std::optional<std::string> outlineTextureKey;
};

// one per img
class MeshBuilder {
public:
  explicit MeshBuilder(const image::Image &img);

  Model Build(const BuildOptions &options) const;

private:
  const image::Image &image_;

  std::vector<std::vector<bool>> computeOccupancy() const;
  bool isSolid(const std::vector<std::vector<bool>> &occupied, int x,
               int y) const;

  static std::optional<Element>
  makeElement(double x1, double y1, double z1, double x2, double y2, double z2,
              std::optional<UV> uvNorth, std::optional<UV> uvSouth,
              std::optional<UV> uvUp, std::optional<UV> uvDown,
              std::optional<UV> uvWest, std::optional<UV> uvEast,
              const std::string &textureVar);

  void appendPerPixelElements(const BuildOptions &options, double z1, double z2,
                              std::vector<Element> &out) const;
  void appendMergedElements(const BuildOptions &options, double z1, double z2,
                            std::vector<Element> &out) const;
  void appendOutlineElements(const BuildOptions &options, double z1, double z2,
                             const std::vector<std::vector<bool>> &occupied,
                             std::vector<Element> &out) const;

  static std::map<std::string, DisplayTransform> defaultDisplay();
};

} // namespace enchantment_tweaks::mesh