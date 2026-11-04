#include "core/types/mesh/types.hpp"
#include "core/types/common.hpp"
#include "core/util/debug.hpp"
#include "core/util/json/utils.hpp"
#include <map>
#include <string>
#include <utility>

namespace enchantment_tweaks::mesh {

const EnumTable<DisplayTransformType> &displayTransformTypeTable() {
  static const EnumTable<DisplayTransformType> table = {
      {DisplayTransformType::ThirdPersonLeftHand, "thirdperson_lefthand"},
      {DisplayTransformType::ThirdPersonRightHand, "thirdperson_righthand"},
      {DisplayTransformType::FirstPersonLeftHand, "firstperson_lefthand"},
      {DisplayTransformType::FirstPersonRightHand, "firstperson_righthand"},
      {DisplayTransformType::Ground, "ground"},
      {DisplayTransformType::Gui, "gui"},
      {DisplayTransformType::Head, "head"},
      {DisplayTransformType::Fixed, "fixed"},
      {DisplayTransformType::OnShelf, "on_shelf"},
  };
  return table;
}
std::string toString(DisplayTransformType t) {
  return enumToString(t, displayTransformTypeTable());
}
DisplayTransformType displayTransformTypeFromString(const std::string &s) {
  return enumFromString(s, displayTransformTypeTable());
}

const EnumTable<FaceDirection> &faceDirectionTable() {
  static const EnumTable<FaceDirection> table = {
      {FaceDirection::North, "north"}, {FaceDirection::South, "south"},
      {FaceDirection::Up, "up"},       {FaceDirection::Down, "down"},
      {FaceDirection::West, "west"},   {FaceDirection::East, "east"},
  };
  return table;
}
std::string toString(FaceDirection t) {
  return enumToString(t, faceDirectionTable());
}
FaceDirection faceDirectionFromString(const std::string &s) {
  return enumFromString(s, faceDirectionTable());
}

const EnumTable<GuiLight> &guiLightTable() {
  static const EnumTable<GuiLight> table = {
      {GuiLight::Front, "front"},
      {GuiLight::Side, "side"},
  };
  return table;
}
std::string toString(GuiLight t) { return enumToString(t, guiLightTable()); }
GuiLight guiLightFromString(const std::string &s) {
  return enumFromString(s, guiLightTable());
}

Uv::Uv(double uFrom, double vFrom, double uTo, double vTo)
    : uFrom(uFrom), vFrom(vFrom), uTo(uTo), vTo(vTo) {}

json Uv::serialize() const {
  json arr = json::array();
  arr.push_back(uFrom);
  arr.push_back(vFrom);
  arr.push_back(uTo);
  arr.push_back(vTo);
  return arr;
}
void Uv::deserialize(const json &j) {
  uFrom = j.at(0).get<double>();
  vFrom = j.at(1).get<double>();
  uTo = j.at(2).get<double>();
  vTo = j.at(3).get<double>();
}

Face::Face(Uv uv, std::string texture)
    : uv(std::move(uv)), texture(std::move(texture)) {}

json Face::serialize() const {
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
void Face::deserialize(const json &j) {
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

Element::Element(Vec3 from, Vec3 to, std::map<FaceDirection, Face> faces)
    : from(std::move(from)), to(std::move(to)), faces(std::move(faces)) {}

json Element::serialize() const {
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
void Element::deserialize(const json &j) {
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

json DisplayTransform::serialize() const {
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
void DisplayTransform::deserialize(const json &j) {
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

json Model::serialize() const {
  json j = json::object();
  writeOptional(j, "parent", parent);
  if (guiLight.has_value()) {
    j["gui_light"] = toString(*guiLight);
  }
  if (!textures.empty()) {
    json texturesJson = json::object();
    for (const auto &entry : textures) {
      const auto flag = forceTranslucent.find(entry.first);
      if (flag != forceTranslucent.end() && flag->second) {
        texturesJson[entry.first] = {{"sprite", entry.second},
                                     {"force_translucent", true}};
      } else {
        texturesJson[entry.first] = entry.second;
      }
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

void Model::deserialize(const json &j) {
  parent = readOptional<std::string>(j, "parent");
  if (const auto s = readOptional<std::string>(j, "gui_light")) {
    guiLight = guiLightFromString(*s);
  } else {
    guiLight.reset();
  }
  textures.clear();
  forceTranslucent.clear();
  if (j.contains("textures")) {
    for (const auto &entry : j.at("textures").items()) {
      const auto &value = entry.value();
      if (value.is_object()) {
        textures[entry.key()] = value.at("sprite").get<std::string>();
        if (value.value("force_translucent", false)) {
          forceTranslucent[entry.key()] = true;
        }
      } else {
        textures[entry.key()] = value.get<std::string>();
      }
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
      try {
        const auto type = displayTransformTypeFromString(entry.key());
        DisplayTransform transform;
        transform.deserialize(entry.value());
        display.emplace(type, std::move(transform));
      } catch (const std::invalid_argument &) {
        // Mojang adds display contexts over time; one unknown key must not
        // invalidate the whole model.
        debug::Logger::warning()
            << "skipping unknown display context '" << entry.key() << "'";
      }
    }
  }
}

} // namespace enchantment_tweaks::mesh
