#include "core/api/mesh.hpp"
#include "core/types/identifier.hpp"
#include "core/types/mesh/types.hpp"

#include <cmath>
#include <stdexcept>

namespace enchantment_tweaks::mesh {

namespace {

double rnd6(double v) { return std::round(v * 1e6) / 1e6; }

} // namespace

MeshBuilder::MeshBuilder(const image::Image &img) : image_(img) {}

std::vector<std::vector<bool>> MeshBuilder::computeOccupancy() const {
  const int w = image_.width();
  const int h = image_.height();
  std::vector<std::vector<bool>> occupied(h, std::vector<bool>(w, false));
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      occupied[y][x] = image_.isOpaque(x, y);
    }
  }
  return occupied;
}

bool MeshBuilder::isSolid(const std::vector<std::vector<bool>> &occupied, int x,
                          int y) const {
  if (y < 0 || y >= static_cast<int>(occupied.size())) return false;
  if (x < 0 || x >= static_cast<int>(occupied[y].size())) return false;
  return occupied[y][x];
}

std::optional<Element> MeshBuilder::makeElement(
    double x1, double y1, double z1, double x2, double y2, double z2,
    const std::optional<Uv> &uvNorth, const std::optional<Uv> &uvSouth,
    const std::optional<Uv> &uvUp, const std::optional<Uv> &uvDown,
    const std::optional<Uv> &uvWest, const std::optional<Uv> &uvEast,
    const std::string &textureVar) {
  Element el;
  el.from = {x1, y1, z1};
  el.to = {x2, y2, z2};

  auto addFace = [&](const char *name, const std::optional<Uv> &uv) {
    if (uv) el.faces[faceDirectionFromString(name)] = Face{*uv, textureVar};
  };
  addFace("north", uvNorth);
  addFace("south", uvSouth);
  addFace("up", uvUp);
  addFace("down", uvDown);
  addFace("west", uvWest);
  addFace("east", uvEast);

  if (el.faces.empty()) return std::nullopt;
  return el;
}

void MeshBuilder::appendPerPixelElements(const BuildOptions &options, double z1,
                                         double z2,
                                         std::vector<Element> &out) const {
  const int w = image_.width();
  const int h = image_.height();
  const double scaleX = MODEL_UNITS / w;
  const double scaleY = MODEL_UNITS / h;

  auto uvX = [&](int px) { return rnd6(px * scaleX); };
  auto uvY = [&](int py) { return rnd6(py * scaleY); };

  for (int row = 0; row < h; ++row) {
    for (int col = 0; col < w; ++col) {
      if (options.cullTransparent && !image_.isOpaque(col, row)) continue;

      const double mx1 = rnd6(col * scaleX);
      const double mx2 = rnd6((col + 1) * scaleX);
      const double my1 = rnd6((h - row - 1) * scaleY);
      const double my2 = rnd6((h - row) * scaleY);

      const double u1 = uvX(col), u2 = uvX(col + 1);
      const double v1 = uvY(row), v2 = uvY(row + 1);

      const Uv uvN{u2, v1, u1, v2};
      const Uv uvS{u1, v1, u2, v2};
      const Uv uvUp{u1, v1, u2, v2};
      const Uv uvDown{u1, v1, u2, v2};
      const Uv uvWest{u1, v1, u2, v2};
      const Uv uvEast{u1, v1, u2, v2};

      auto el = makeElement(mx1, my1, z1, mx2, my2, z2, uvN, uvS, uvUp, uvDown,
                            uvWest, uvEast, "#texture");
      if (el) out.push_back(std::move(*el));
    }
  }
}

void MeshBuilder::appendMergedElements(const BuildOptions &options, double z1,
                                       double z2,
                                       std::vector<Element> &out) const {
  const int w = image_.width();
  const int h = image_.height();
  const double scaleX = MODEL_UNITS / w;
  const double scaleY = MODEL_UNITS / h;

  auto uvX = [&](int px) { return rnd6(px * scaleX); };
  auto uvY = [&](int py) { return rnd6(py * scaleY); };

  for (int row = 0; row < h; ++row) {
    int col = 0;
    while (col < w) {
      if (options.cullTransparent && !image_.isOpaque(col, row)) {
        ++col;
        continue;
      }

      const int runStart = col;
      ++col;
      while (col < w) {
        if (options.cullTransparent && !image_.isOpaque(col, row)) break;
        ++col;
      }
      const int runEnd = col; // exclusive

      const double mx1 = rnd6(runStart * scaleX);
      const double mx2 = rnd6(runEnd * scaleX);
      const double my1 = rnd6((h - row - 1) * scaleY);
      const double my2 = rnd6((h - row) * scaleY);

      const double u1 = uvX(runStart), u2 = uvX(runEnd);
      const double v1 = uvY(row), v2 = uvY(row + 1);

      const Uv uvN{u2, v1, u1, v2};
      const Uv uvS{u1, v1, u2, v2};
      const Uv uvUp{u1, v1, u2, v2};
      const Uv uvDown{u1, v1, u2, v2};

      const double uWest1 = uvX(runStart), uWest2 = uvX(runStart + 1);
      const double uEast1 = uvX(runEnd - 1), uEast2 = uvX(runEnd);
      const Uv uvWest{uWest1, v1, uWest2, v2};
      const Uv uvEast{uEast1, v1, uEast2, v2};

      auto el = makeElement(mx1, my1, z1, mx2, my2, z2, uvN, uvS, uvUp, uvDown,
                            uvWest, uvEast, "#texture");
      if (el) out.push_back(std::move(*el));
    }
  }
}

void MeshBuilder::appendOutlineElements(
    const BuildOptions &options, double z1, double z2,
    const std::vector<std::vector<bool>> &occupied,
    std::vector<Element> &out) const {
  const int w = image_.width();
  const int h = image_.height();
  const double scaleX = MODEL_UNITS / w;
  const double scaleY = MODEL_UNITS / h;

  const double expandX = options.outlineSizePx * MODEL_UNITS / w;
  const double expandY = options.outlineSizePx * MODEL_UNITS / h;
  const double expandZ = options.outlineSizePx * MODEL_UNITS / w;

  const Uv fulluvN{MODEL_UNITS, 0.0, 0.0, MODEL_UNITS};
  const Uv fulluvS{0.0, 0.0, MODEL_UNITS, MODEL_UNITS};
  const Uv fullUvLR{0.0, 0.0, MODEL_UNITS, MODEL_UNITS};

  for (int row = 0; row < h; ++row) {
    for (int col = 0; col < w; ++col) {
      if (!isSolid(occupied, col, row)) continue;

      const double mx1 = rnd6(col * scaleX);
      const double mx2 = rnd6((col + 1) * scaleX);
      const double my1 = rnd6((h - row - 1) * scaleY);
      const double my2 = rnd6((h - row) * scaleY);

      const bool showWest = !isSolid(occupied, col - 1, row);
      const bool showEast = !isSolid(occupied, col + 1, row);
      const bool showUp = !isSolid(occupied, col, row - 1);
      const bool showDown = !isSolid(occupied, col, row + 1);

      const double ox1 = mx1 - (showWest ? expandX : 0.0);
      const double ox2 = mx2 + (showEast ? expandX : 0.0);
      const double oy1 = my1 - (showDown ? expandY : 0.0);
      const double oy2 = my2 + (showUp ? expandY : 0.0);
      const double oz1 = z1 - expandZ;
      const double oz2 = z2 + expandZ;

      // reversed
      auto el = makeElement(
          ox2, oy2, oz2, ox1, oy1, oz1, std::optional<Uv>(fulluvN),
          std::optional<Uv>(fulluvS),
          showDown ? std::optional<Uv>(fullUvLR) : std::nullopt,
          showUp ? std::optional<Uv>(fullUvLR) : std::nullopt,
          showEast ? std::optional<Uv>(fullUvLR) : std::nullopt,
          showWest ? std::optional<Uv>(fullUvLR) : std::nullopt, "#outline");
      if (el) out.push_back(std::move(*el));
    }
  }
}

std::map<DisplayTransformType, DisplayTransform> MeshBuilder::defaultDisplay() {
  std::map<DisplayTransformType, DisplayTransform> d;

  DisplayTransform thirdR;
  thirdR.rotation = Vec3{0, -90, 55};
  thirdR.translation = Vec3{0, 4, 0.5};
  thirdR.scale = Vec3{0.85, 0.85, 0.85};
  d[DisplayTransformType::ThirdPersonRightHand] = thirdR;

  DisplayTransform thirdL;
  thirdL.rotation = Vec3{0, 90, -55};
  thirdL.translation = Vec3{0, 4, 0.5};
  thirdL.scale = Vec3{0.85, 0.85, 0.85};
  d[DisplayTransformType::ThirdPersonLeftHand] = thirdL;

  DisplayTransform firstR;
  firstR.rotation = Vec3{0, -90, 25};
  firstR.translation = Vec3{1.13, 3.2, 1.13};
  firstR.scale = Vec3{0.68, 0.68, 0.68};
  d[DisplayTransformType::FirstPersonRightHand] = firstR;

  DisplayTransform firstL;
  firstL.rotation = Vec3{0, 90, -25};
  firstL.translation = Vec3{1.13, 3.2, 1.13};
  firstL.scale = Vec3{0.68, 0.68, 0.68};
  d[DisplayTransformType::FirstPersonLeftHand] = firstL;

  DisplayTransform ground;
  ground.translation = Vec3{0, 2, 0};
  ground.scale = Vec3{0.5, 0.5, 0.5};
  d[DisplayTransformType::Ground] = ground;

  d[DisplayTransformType::Gui] = DisplayTransform{}; // default

  DisplayTransform head;
  head.rotation = Vec3{0, 180, 0};
  head.translation = Vec3{0, 13, 7};
  d[DisplayTransformType::Head] = head;

  DisplayTransform fixed;
  fixed.rotation = Vec3{0, 180, 0};
  fixed.translation = Vec3{0, 0, -2.5};
  fixed.scale = Vec3{0.5, 0.5, 0.5};
  d[DisplayTransformType::Fixed] = fixed;

  return d;
}

Model MeshBuilder::build(const BuildOptions &options) const {
  if (options.outlineSizePx <= 0 && options.outlineOnly) {
    throw std::invalid_argument(
        "No outline present while specifying outline only mode, aborting");
  }

  const int w = image_.width();

  const double zHalf = (options.thicknessPx * MODEL_UNITS / w) / 2.0;
  double z1 = rnd6(MODEL_UNITS / 2 - zHalf);
  double z2 = rnd6(MODEL_UNITS / 2 + zHalf);
  if (std::abs(z2 - z1) < 0.001) {
    z2 = z1 + rnd6(MODEL_UNITS / w);
  }

  Model model;
  const Identifier textureRef =
      Identifier(options.textureNamespace, options.textureKey);
  model.textures["particle"] = textureRef;
  if (!options.outlineOnly) {

    model.textures["texture"] = textureRef;

    if (options.mergeColumns) {
      appendMergedElements(options, z1, z2, model.elements);
    } else {
      appendPerPixelElements(options, z1, z2, model.elements);
    }
  }

  // TODO: do item-specific display later
  model.display = defaultDisplay();

  for (const auto &[k, v] : options.overrideTransforms) {
    model.display[k] = v;
  }

  if (options.outlineSizePx > 0.0) {
    const std::string oNamespace =
        options.outlineTextureNamespace.value_or(options.textureNamespace);
    const std::string oKey = options.outlineTextureKey.value_or("outline");
    model.textures["outline"] = Identifier(oNamespace, oKey);

    const auto occupied = computeOccupancy();
    appendOutlineElements(options, z1, z2, occupied, model.elements);
  }

  return model;
}

} // namespace enchantment_tweaks::mesh
