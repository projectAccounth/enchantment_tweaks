#pragma once

#include "core/types/common.hpp"
#include "core/types/identifier.hpp"
#include "core/util/json/utils.hpp"

#include <map>
#include <optional>
#include <string>
#include <vector>

using namespace util;

namespace enchantment_tweaks::mesh {

enum class DisplayTransformType : uint8_t {
  ThirdPersonLeftHand,
  ThirdPersonRightHand,
  FirstPersonLeftHand,
  FirstPersonRightHand,
  Ground,
  Gui,
  Head,
  Fixed,
};

const EnumTable<DisplayTransformType> &displayTransformTypeTable();
std::string toString(DisplayTransformType t);
DisplayTransformType displayTransformTypeFromString(const std::string &s);

enum class FaceDirection : uint8_t {
  North,
  South,
  Up,
  Down,
  West,
  East,
};

const EnumTable<FaceDirection> &faceDirectionTable();
std::string toString(FaceDirection t);
FaceDirection faceDirectionFromString(const std::string &s);

enum class GuiLight : uint8_t { Front, Side };

const EnumTable<GuiLight> &guiLightTable();
std::string toString(GuiLight t);
GuiLight guiLightFromString(const std::string &s);

// [uFrom, vFrom, uTo, vTo].
struct Uv final : public JsonSerializable {
  Uv() = default;
  Uv(double uFrom, double vFrom, double uTo, double vTo);

  double uFrom = 0.0;
  double vFrom = 0.0;
  double uTo = 0.0;
  double vTo = 0.0;

  json serialize() const override;
  void deserialize(const json &j) override;
};

// One face of a cuboid element.
struct Face final : public JsonSerializable {
  Face() = default;
  Face(Uv uv, std::string texture);

  Uv uv;
  std::string texture; // e.g. "#texture"
  std::optional<FaceDirection> cullface;
  int rotation = 0; // optional, one of 0 / 90 / 180 / 270, default 0

  json serialize() const override;
  void deserialize(const json &j) override;
};

// One cuboid element of the model, from `from` to `to`, with up to six faces.
struct Element final : public JsonSerializable {
  Element() = default;
  Element(Vec3 from, Vec3 to, std::map<FaceDirection, Face> faces);

  Vec3 from;
  Vec3 to;
  std::map<FaceDirection, Face> faces;

  json serialize() const override;
  void deserialize(const json &j) override;
};

struct DisplayTransform final : public JsonSerializable {
  std::optional<Vec3> rotation;
  std::optional<Vec3> translation;
  std::optional<Vec3> scale;

  json serialize() const override;
  void deserialize(const json &j) override;
};

struct Model final : public JsonSerializable {
  std::optional<std::string> parent;
  std::optional<GuiLight> guiLight;
  std::map<std::string, std::string>
      textures; // "texture variable" -> id or ref, e.g. "particle" -> "ns:key"
                // or "#north"
  std::vector<Element> elements;
  std::map<DisplayTransformType, DisplayTransform> display;

  json serialize() const override;
  void deserialize(const json &j) override;
};

} // namespace enchantment_tweaks::mesh
