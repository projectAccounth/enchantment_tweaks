#pragma once

#include "core/util/identifier.hpp"
#include "core/util/type.hpp"
#include <array>
#include <map>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

namespace enchantment_tweaks::mesh {

enum class DisplayTransformTypes : uint8_t {
  THIRD_PERSON_LHAND,
  THIRD_PERSON_RHAND,
  FIRST_PERSON_LHAND,
  FIRST_PERSON_RHAND,
  GROUND,
  GUI,
  HEAD,
  FIXED
};

enum class FaceCardinalDirection : uint8_t {
  NORTH,
  SOUTH,
  UP,
  DOWN,
  WEST,
  EAST
};

inline static std::string faceDirToString(const FaceCardinalDirection &dir) {
  static constexpr const char *kFaceOrder[] = {"north", "south", "up",
                                               "down",  "west",  "east"};
  return kFaceOrder[static_cast<uint8_t>(dir)];
}

inline static FaceCardinalDirection stringToFaceDir(const std::string &str) {
  static const std::map<std::string, FaceCardinalDirection> lookup = {
      {"north", FaceCardinalDirection::NORTH},
      {"south", FaceCardinalDirection::SOUTH},
      {"up", FaceCardinalDirection::UP},
      {"down", FaceCardinalDirection::DOWN},
      {"west", FaceCardinalDirection::WEST},
      {"east", FaceCardinalDirection::EAST},
  };

  return lookup.at(str);
}

inline static DisplayTransformTypes
transformTypeFromString(const std::string &string) {
  static const std::map<std::string, DisplayTransformTypes> lookup = {
      {"thirdperson_lefthand", DisplayTransformTypes::THIRD_PERSON_LHAND},
      {"thirdperson_righthand", DisplayTransformTypes::THIRD_PERSON_RHAND},
      {"firstperson_lefthand", DisplayTransformTypes::FIRST_PERSON_LHAND},
      {"firstperson_righthand", DisplayTransformTypes::FIRST_PERSON_RHAND},
      {"ground", DisplayTransformTypes::GROUND},
      {"gui", DisplayTransformTypes::GUI},
      {"head", DisplayTransformTypes::HEAD},
      {"fixed", DisplayTransformTypes::FIXED},
  };

  return lookup.at(string);
}

inline static std::string
stringFromTransformType(const DisplayTransformTypes &type) {
  static const std::map<DisplayTransformTypes, std::string> lookup = {
      {DisplayTransformTypes::THIRD_PERSON_LHAND, "thirdperson_lefthand"},
      {DisplayTransformTypes::THIRD_PERSON_RHAND, "thirdperson_righthand"},
      {DisplayTransformTypes::FIRST_PERSON_LHAND, "firstperson_lefthand"},
      {DisplayTransformTypes::FIRST_PERSON_RHAND, "firstperson_righthand"},
      {DisplayTransformTypes::GROUND, "ground"},
      {DisplayTransformTypes::GUI, "gui"},
      {DisplayTransformTypes::HEAD, "head"},
      {DisplayTransformTypes::FIXED, "fixed"},
  };

  return lookup.at(type);
}

struct Vec3 : public JsonSerializable {
  Vec3(double x, double y, double z) : x(x), y(y), z(z) {}
  Vec3() : x(0), y(0), z(0) {}
  double x, y, z;

  json serialize() const override { return json::array({x, y, z}); }

  void deserialize(const json &j) override {
    std::array<double, 3> v3;
    j.get_to(v3);
    x = v3[0], y = v3[1], z = v3[2];
  }
};

struct UV : public JsonSerializable {
  UV(double uf, double vf, double ut, double vt)
      : uFrom(uf), vFrom(vf), uTo(ut), vTo(vt) {}
  UV() : uFrom(), vFrom(), uTo(), vTo() {}
  double uFrom, vFrom, uTo, vTo;

  json serialize() const override {
    return json::array({uFrom, vFrom, uTo, vTo});
  }

  void deserialize(const json &j) override {
    std::array<double, 4> uv;
    j.get_to(uv);
    uFrom = uv[0], vFrom = uv[1], uTo = uv[2], vTo = uv[3];
  }
};

class Face : public JsonSerializable {
public:
  Face(const UV &uv, const std::string &texture) : uv(uv), texture(texture) {}

  UV uv{};
  std::string texture; // e.g. "#texture"
  std::unordered_set<FaceCardinalDirection> cullface;
  int rotation;

  json serialize() const override {
    std::unordered_set<std::string> converted;
    for (const auto &f : cullface)
      converted.insert(faceDirToString(f));
    return {
        {"uv", uv.serialize()},
        {"texture", texture},
        {"cullface", converted},
        {"rotation", rotation},
    };
  }

  void deserialize(const json &json) override {
    if (json.contains("uv")) uv.deserialize(json["uv"]);
    if (json.contains("texture")) texture = json["texture"];
    if (json.contains("cullface")) {
      std::unordered_set<std::string> deserialized;
      json["cullface"].get_to(deserialized);

      for (const auto &str : deserialized)
        cullface.insert(stringToFaceDir(str));
    }
    if (json.contains("rotation")) rotation = json["rotation"];
  }
};

struct Element : public JsonSerializable {
public:
  Element() = default;
  Element(const Vec3 &from, const Vec3 &to,
          const std::map<FaceCardinalDirection, Face> &faces)
      : from(from), to(to), faces(faces) {}
  Vec3 from{};
  Vec3 to{};
  std::map<FaceCardinalDirection, Face> faces;

  json serialize() const override {
    json j;
    j["from"] = from.serialize();
    j["to"] = to.serialize();

    static constexpr const char *kFaceOrder[] = {"north", "south", "up",
                                                 "down",  "west",  "east"};
    json serializedFaces = json::object();
    for (const char *name : kFaceOrder) {
      auto it = faces.find(stringToFaceDir(name));
      if (it != faces.end()) serializedFaces[name] = it->second.serialize();
    }
    j["faces"] = serializedFaces;
    return j;
  }

  void deserialize(const json &j) override {
    from.deserialize(j["from"]);
    to.deserialize(j["to"]);

    for (auto &e : j["faces"].items()) {
      faces[stringToFaceDir(e.key())].deserialize(e.value());
    }
  }
};

struct DisplayTransform : public JsonSerializable {
public:
  std::optional<Vec3> rotation;
  std::optional<Vec3> translation;
  std::optional<Vec3> scale;

  json serialize() const override {
    json j;
    if (rotation) j["rotation"] = rotation->serialize();
    if (translation) j["translation"] = translation->serialize();
    if (scale) j["scale"] = scale->serialize();
    return j;
  }

  void deserialize(const json &j) override {
    if (j.contains("rotation")) {
      rotation->deserialize(j["rotation"]);
    }
    if (j.contains("translation")) {
      translation->deserialize(j["translation"]);
    }
    if (j.contains("scale")) {
      scale->deserialize(j["scale"]);
    }
  }
};

struct Model : public JsonSerializable {
  std::optional<std::string> parent;
  std::map<std::string, Identifier> textures; // "texture" -> "ns:key", etc.
  std::vector<Element> elements;
  std::map<DisplayTransformTypes, DisplayTransform> display;

  json serialize() const override {
    json j;
    for (const auto &[name, id] : textures) {
      j["textures"][name] = id.getString();
    }

    json serializedElems = json::array();
    for (const auto &e : elements)
      serializedElems.push_back(e.serialize());
    j["elements"] = serializedElems;

    json serializedDisplay = json::object();
    for (const auto &[name, transform] : display)
      serializedDisplay[mesh::stringFromTransformType(name)] =
          transform.serialize();
    j["display"] = serializedDisplay;

    return j;
  }

  void deserialize(const json &j) override {
    for (const auto &[key, texPath] : j["textures"].items()) {
      textures[key] = Identifier::parse(texPath);
    }
    for (const auto &elem : j["elements"]) {
      Element e;
      e.deserialize(elem);
      elements.emplace_back(e);
    }
  }
};

} // namespace enchantment_tweaks::mesh