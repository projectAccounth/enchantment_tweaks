#pragma once

#include "core/types/common.hpp"
#include "core/types/identifier.hpp"
#include "core/util/json/utils.hpp"

#include <map>
#include <optional>
#include <string>
#include <utility>
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

inline const EnumTable<DisplayTransformType> &displayTransformTypeTable() {
  static const EnumTable<DisplayTransformType> table = {
      {DisplayTransformType::ThirdPersonLeftHand, "thirdperson_lefthand"},
      {DisplayTransformType::ThirdPersonRightHand, "thirdperson_righthand"},
      {DisplayTransformType::FirstPersonLeftHand, "firstperson_lefthand"},
      {DisplayTransformType::FirstPersonRightHand, "firstperson_righthand"},
      {DisplayTransformType::Ground, "ground"},
      {DisplayTransformType::Gui, "gui"},
      {DisplayTransformType::Head, "head"},
      {DisplayTransformType::Fixed, "fixed"},
  };
  return table;
}
inline std::string toString(DisplayTransformType t) {
  return enumToString(t, displayTransformTypeTable());
}
inline DisplayTransformType
displayTransformTypeFromString(const std::string &s) {
  return enumFromString(s, displayTransformTypeTable());
}

enum class FaceDirection : uint8_t {
  North,
  South,
  Up,
  Down,
  West,
  East,
};

inline const EnumTable<FaceDirection> &faceDirectionTable() {
  static const EnumTable<FaceDirection> table = {
      {FaceDirection::North, "north"}, {FaceDirection::South, "south"},
      {FaceDirection::Up, "up"},       {FaceDirection::Down, "down"},
      {FaceDirection::West, "west"},   {FaceDirection::East, "east"},
  };
  return table;
}
inline std::string toString(FaceDirection t) {
  return enumToString(t, faceDirectionTable());
}
inline FaceDirection faceDirectionFromString(const std::string &s) {
  return enumFromString(s, faceDirectionTable());
}

enum class GuiLight : uint8_t { Front, Side };

inline const EnumTable<GuiLight> &guiLightTable() {
  static const EnumTable<GuiLight> table = {
      {GuiLight::Front, "front"},
      {GuiLight::Side, "side"},
  };
  return table;
}
inline std::string toString(GuiLight t) {
  return enumToString(t, guiLightTable());
}
inline GuiLight guiLightFromString(const std::string &s) {
  return enumFromString(s, guiLightTable());
}

// [uFrom, vFrom, uTo, vTo].
struct Uv final : public JsonSerializable {
  Uv() = default;
  Uv(double uFrom, double vFrom, double uTo, double vTo)
      : uFrom(uFrom), vFrom(vFrom), uTo(uTo), vTo(vTo) {}

  double uFrom = 0.0;
  double vFrom = 0.0;
  double uTo = 0.0;
  double vTo = 0.0;

  json serialize() const override {
    json arr = json::array();
    arr.push_back(uFrom);
    arr.push_back(vFrom);
    arr.push_back(uTo);
    arr.push_back(vTo);
    return arr;
  }

  void deserialize(const json &j) override {
    uFrom = j.at(0).get<double>();
    vFrom = j.at(1).get<double>();
    uTo = j.at(2).get<double>();
    vTo = j.at(3).get<double>();
  }
};

// One face of a cuboid element.
struct Face final : public JsonSerializable {
  Face() = default;
  Face(Uv uv, std::string texture)
      : uv(std::move(uv)), texture(std::move(texture)) {}

  Uv uv;
  std::string texture; // e.g. "#texture"
  std::optional<FaceDirection> cullface;
  int rotation = 0; // optional, one of 0 / 90 / 180 / 270, default 0

  json serialize() const override {
    json j = json::object();
    j["uv"] = uv.serialize();
    j["texture"] = texture;
    if (cullface.has_value()) {
      j["cullface"] = toString(*cullface);
    }
    if (rotation != 0) {
      j["rotation"] = rotation;
    }
    return j;
  }

  void deserialize(const json &j) override {
    if (j.contains("uv")) {
      uv.deserialize(j.at("uv"));
    }
    texture = readOptional<std::string>(j, "texture").value_or(texture);
    if (const auto s = readOptional<std::string>(j, "cullface")) {
      cullface = faceDirectionFromString(*s);
    } else {
      cullface.reset();
    }
    rotation = readOptional<int>(j, "rotation").value_or(0);
  }
};

// One cuboid element of the model, from `from` to `to`, with up to six faces.
struct Element final : public JsonSerializable {
  Element() = default;
  Element(Vec3 from, Vec3 to, std::map<FaceDirection, Face> faces)
      : from(std::move(from)), to(std::move(to)), faces(std::move(faces)) {}

  Vec3 from;
  Vec3 to;
  std::map<FaceDirection, Face> faces;

  json serialize() const override {
    json j = json::object();
    j["from"] = from.serialize();
    j["to"] = to.serialize();

    json facesJson = json::object();
    for (const auto &entry : faces) {
      facesJson[toString(entry.first)] = entry.second.serialize();
    }
    j["faces"] = std::move(facesJson);
    return j;
  }

  void deserialize(const json &j) override {
    from.deserialize(j.at("from"));
    to.deserialize(j.at("to"));

    faces.clear();
    if (j.contains("faces")) {
      for (const auto &entry : j.at("faces").items()) {
        Face face;
        face.deserialize(entry.value());
        faces.emplace(faceDirectionFromString(entry.key()), std::move(face));
      }
    }
  }
};

struct DisplayTransform final : public JsonSerializable {
  std::optional<Vec3> rotation;
  std::optional<Vec3> translation;
  std::optional<Vec3> scale;

  json serialize() const override {
    json j = json::object();
    if (rotation.has_value()) {
      j["rotation"] = rotation->serialize();
    }
    if (translation.has_value()) {
      j["translation"] = translation->serialize();
    }
    if (scale.has_value()) {
      j["scale"] = scale->serialize();
    }
    return j;
  }

  void deserialize(const json &j) override {
    rotation.reset();
    translation.reset();
    scale.reset();
    if (j.contains("rotation")) {
      Vec3 v;
      v.deserialize(j.at("rotation"));
      rotation = v;
    }
    if (j.contains("translation")) {
      Vec3 v;
      v.deserialize(j.at("translation"));
      translation = v;
    }
    if (j.contains("scale")) {
      Vec3 v;
      v.deserialize(j.at("scale"));
      scale = v;
    }
  }
};

struct Model final : public JsonSerializable {
  std::optional<std::string> parent;
  std::optional<GuiLight> guiLight;
  std::map<std::string, Identifier>
      textures; // "texture variable" -> id, e.g. "particle" -> "ns:key"
  std::vector<Element> elements;
  std::map<DisplayTransformType, DisplayTransform> display;

  json serialize() const override {
    json j = json::object();
    writeOptional(j, "parent", parent);
    if (guiLight.has_value()) {
      j["gui_light"] = toString(*guiLight);
    }

    if (!textures.empty()) {
      json texturesJson = json::object();
      for (const auto &entry : textures) {
        texturesJson[entry.first] = entry.second.toString();
      }
      j["textures"] = std::move(texturesJson);
    }

    if (!elements.empty()) {
      json elementsJson = json::array();
      for (const auto &element : elements) {
        elementsJson.push_back(element.serialize());
      }
      j["elements"] = std::move(elementsJson);
    }

    if (!display.empty()) {
      json displayJson = json::object();
      for (const auto &entry : display) {
        displayJson[toString(entry.first)] = entry.second.serialize();
      }
      j["display"] = std::move(displayJson);
    }

    return j;
  }

  void deserialize(const json &j) override {
    parent = readOptional<std::string>(j, "parent");
    if (const auto s = readOptional<std::string>(j, "gui_light")) {
      guiLight = guiLightFromString(*s);
    } else {
      guiLight.reset();
    }

    textures.clear();
    if (j.contains("textures")) {
      for (const auto &entry : j.at("textures").items()) {
        textures[entry.key()] =
            Identifier::parse(entry.value().get<std::string>());
      }
    }

    elements.clear();
    if (j.contains("elements")) {
      for (const auto &elementJson : j.at("elements")) {
        Element element;
        element.deserialize(elementJson);
        elements.push_back(std::move(element));
      }
    }

    display.clear();
    if (j.contains("display")) {
      for (const auto &entry : j.at("display").items()) {
        DisplayTransform transform;
        transform.deserialize(entry.value());
        display.emplace(displayTransformTypeFromString(entry.key()),
                        std::move(transform));
      }
    }
  }
};

} // namespace enchantment_tweaks::mesh
