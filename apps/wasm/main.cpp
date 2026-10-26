#include <emscripten/bind.h>
#include <emscripten/emscripten.h>
#include <emscripten/val.h>

#include "core/api/mesh.hpp"
#include "core/types/common.hpp"
#include "core/types/identifier.hpp"
#include "core/types/items/item_model.hpp"
#include "core/types/items/item_model_definition.hpp"
#include "core/types/items/special_model.hpp"
#include "core/types/items/tint_source.hpp"
#include "core/types/items/types.hpp"
#include "core/types/mesh/types.hpp"
#include "core/util/image.hpp"

#include <memory>
#include <string>
#include <vector>

using namespace enchantment_tweaks;
using namespace enchantment_tweaks::item;
using namespace enchantment_tweaks::mesh;
using namespace enchantment_tweaks::image;
using emscripten::val;

namespace {

// Item model functions
std::string itemModelSerialize(const std::string &jsonStr) {
  json j = json::parse(jsonStr);
  std::unique_ptr<ItemModel> model = ItemModel::create(j);
  return model->serialize().dump();
}

std::string itemModelDeserialize(const std::string &jsonStr) {
  json j = json::parse(jsonStr);
  std::unique_ptr<ItemModel> model = ItemModel::create(j);
  return model->serialize().dump();
}

std::string itemModelDefinitionSerialize(const std::string &jsonStr) {
  json j = json::parse(jsonStr);
  ItemModelDefinition def;
  def.deserialize(j);
  return def.serialize().dump();
}

std::string itemModelDefinitionDeserialize(const std::string &jsonStr) {
  json j = json::parse(jsonStr);
  ItemModelDefinition def;
  def.deserialize(j);
  return def.serialize().dump();
}

std::string specialModelSerialize(const std::string &jsonStr) {
  json j = json::parse(jsonStr);
  std::unique_ptr<SpecialModel> model = SpecialModel::create(j);
  return model->serialize().dump();
}

std::string tintSourceSerialize(const std::string &jsonStr) {
  json j = json::parse(jsonStr);
  std::unique_ptr<TintSource> source = TintSource::create(j);
  return source->serialize().dump();
}

// Mesh functions
std::string buildOptionsToJson(const BuildOptions &options) {
  json j = json::object();
  j["thicknessPx"] = options.thicknessPx;
  j["textureNamespace"] = options.textureNamespace;
  j["textureKey"] = options.textureKey;
  j["cullTransparent"] = options.cullTransparent;
  j["mergeColumns"] = options.mergeColumns;
  j["outlineOnly"] = options.outlineOnly;
  j["outlineSizePx"] = options.outlineSizePx;
  if (options.outlineTextureNamespace.has_value()) {
    j["outlineTextureNamespace"] = *options.outlineTextureNamespace;
  }
  if (options.outlineTextureKey.has_value()) {
    j["outlineTextureKey"] = *options.outlineTextureKey;
  }
  json overrideJson = json::object();
  for (const auto &[k, v] : options.overrideTransforms) {
    overrideJson[toString(k)] = v.serialize();
  }
  j["overrideTransforms"] = std::move(overrideJson);
  return j.dump();
}

// Simplified BuildOptions for JS (no optional/map complexity)
struct JsBuildOptions {
  double thicknessPx = 1.0;
  std::string textureNamespace = "minecraft";
  std::string textureKey = "missingno";
  bool cullTransparent = true;
  bool mergeColumns = false;
  bool outlineOnly = false;
  double outlineSizePx = 0.0;
  std::string outlineTextureNamespace = "";
  std::string outlineTextureKey = "";
  std::vector<std::pair<std::string, DisplayTransform>> overrideTransforms;
};

JsBuildOptions jsBuildOptionsFromJson(const std::string &jsonStr) {
  json j = json::parse(jsonStr);
  JsBuildOptions options;
  options.thicknessPx = j.value("thicknessPx", 1.0);
  options.textureNamespace = j.value("textureNamespace", "minecraft");
  options.textureKey = j.value("textureKey", "missingno");
  options.cullTransparent = j.value("cullTransparent", true);
  options.mergeColumns = j.value("mergeColumns", false);
  options.outlineOnly = j.value("outlineOnly", false);
  options.outlineSizePx = j.value("outlineSizePx", 0.0);
  options.outlineTextureNamespace = j.value("outlineTextureNamespace", "");
  options.outlineTextureKey = j.value("outlineTextureKey", "");
  if (j.contains("overrideTransforms")) {
    for (const auto &entry : j.at("overrideTransforms").items()) {
      DisplayTransform transform;
      transform.deserialize(entry.value());
      options.overrideTransforms.emplace_back(entry.key(), std::move(transform));
    }
  }
  return options;
}

BuildOptions jsToBuildOptions(const JsBuildOptions &js) {
  BuildOptions options;
  options.thicknessPx = js.thicknessPx;
  options.textureNamespace = js.textureNamespace;
  options.textureKey = js.textureKey;
  options.cullTransparent = js.cullTransparent;
  options.mergeColumns = js.mergeColumns;
  options.outlineOnly = js.outlineOnly;
  options.outlineSizePx = js.outlineSizePx;
  if (!js.outlineTextureNamespace.empty()) {
    options.outlineTextureNamespace = js.outlineTextureNamespace;
  }
  if (!js.outlineTextureKey.empty()) {
    options.outlineTextureKey = js.outlineTextureKey;
  }
  for (const auto &[k, v] : js.overrideTransforms) {
    options.overrideTransforms[displayTransformTypeFromString(k)] = v;
  }
  return options;
}

std::string buildMesh(const std::string &imageDataJson,
                      const std::string &optionsJson) {
  json imgJson = json::parse(imageDataJson);
  int width = imgJson.at("width").get<int>();
  int height = imgJson.at("height").get<int>();
  std::vector<uint8_t> pixels =
      imgJson.at("pixels").get<std::vector<uint8_t>>();

  Image img(width, height, std::move(pixels));
  MeshBuilder builder(img);

  JsBuildOptions jsOptions = jsBuildOptionsFromJson(optionsJson);
  BuildOptions options = jsToBuildOptions(jsOptions);
  Model model = builder.build(options);

  return model.serialize().dump();
}

std::string modelSerialize(const std::string &jsonStr) {
  json j = json::parse(jsonStr);
  Model model;
  model.deserialize(j);
  return model.serialize().dump();
}

std::string modelDeserialize(const std::string &jsonStr) {
  json j = json::parse(jsonStr);
  Model model;
  model.deserialize(j);
  return model.serialize().dump();
}

std::string identifierToString(const std::string &ns, const std::string &path) {
  Identifier id(ns, path);
  return id.toString();
}

std::string identifierParse(const std::string &str) {
  Identifier id = Identifier::parse(str);
  return id.toString();
}

std::string vec3Serialize(const std::string &jsonStr) {
  json j = json::parse(jsonStr);
  Vec3 v;
  v.deserialize(j);
  return v.serialize().dump();
}

std::string transformationSerialize(const std::string &jsonStr) {
  json j = json::parse(jsonStr);
  Transformation t;
  t.deserialize(j);
  return t.serialize().dump();
}

} // namespace

EMSCRIPTEN_BINDINGS(enchantment_tweaks) {
  // Enums
  emscripten::enum_<ItemModelType>("ItemModelType")
      .value("Model", ItemModelType::Model)
      .value("Composite", ItemModelType::Composite)
      .value("Condition", ItemModelType::Condition)
      .value("Select", ItemModelType::Select)
      .value("RangeDispatch", ItemModelType::RangeDispatch)
      .value("Empty", ItemModelType::Empty)
      .value("BundleSelectedItem", ItemModelType::BundleSelectedItem)
      .value("Special", ItemModelType::Special);

  emscripten::enum_<BooleanPropertyType>("BooleanPropertyType")
      .value("Broken", BooleanPropertyType::Broken)
      .value("BundleHasSelectedItem",
             BooleanPropertyType::BundleHasSelectedItem)
      .value("Carried", BooleanPropertyType::Carried)
      .value("Component", BooleanPropertyType::Component)
      .value("Damaged", BooleanPropertyType::Damaged)
      .value("ExtendedView", BooleanPropertyType::ExtendedView)
      .value("FishingRodCast", BooleanPropertyType::FishingRodCast)
      .value("HasComponent", BooleanPropertyType::HasComponent)
      .value("KeybindDown", BooleanPropertyType::KeybindDown)
      .value("Selected", BooleanPropertyType::Selected)
      .value("UsingItem", BooleanPropertyType::UsingItem)
      .value("ViewEntity", BooleanPropertyType::ViewEntity)
      .value("CustomModelData", BooleanPropertyType::CustomModelData);

  emscripten::enum_<SelectPropertyType>("SelectPropertyType")
      .value("BlockState", SelectPropertyType::BlockState)
      .value("ChargeType", SelectPropertyType::ChargeType)
      .value("Component", SelectPropertyType::Component)
      .value("ContextDimension", SelectPropertyType::ContextDimension)
      .value("ContextEntityType", SelectPropertyType::ContextEntityType)
      .value("DisplayContext", SelectPropertyType::DisplayContext)
      .value("LocalTime", SelectPropertyType::LocalTime)
      .value("MainHand", SelectPropertyType::MainHand)
      .value("TrimMaterial", SelectPropertyType::TrimMaterial)
      .value("CustomModelData", SelectPropertyType::CustomModelData);

  emscripten::enum_<NumericPropertyType>("NumericPropertyType")
      .value("BundleFullness", NumericPropertyType::BundleFullness)
      .value("Compass", NumericPropertyType::Compass)
      .value("Cooldown", NumericPropertyType::Cooldown)
      .value("Count", NumericPropertyType::Count)
      .value("CrossbowPull", NumericPropertyType::CrossbowPull)
      .value("Damage", NumericPropertyType::Damage)
      .value("Time", NumericPropertyType::Time)
      .value("UseCycle", NumericPropertyType::UseCycle)
      .value("UseDuration", NumericPropertyType::UseDuration)
      .value("CustomModelData", NumericPropertyType::CustomModelData);

  emscripten::enum_<TimeSource>("TimeSource")
      .value("Daytime", TimeSource::Daytime)
      .value("MoonPhase", TimeSource::MoonPhase)
      .value("Random", TimeSource::Random);

  emscripten::enum_<CompassTarget>("CompassTarget")
      .value("Spawn", CompassTarget::Spawn)
      .value("Lodestone", CompassTarget::Lodestone)
      .value("Recovery", CompassTarget::Recovery)
      .value("None", CompassTarget::None);

  emscripten::enum_<SpecialModelType>("SpecialModelType")
      .value("Banner", SpecialModelType::Banner)
      .value("Bell", SpecialModelType::Bell)
      .value("Book", SpecialModelType::Book)
      .value("Chest", SpecialModelType::Chest)
      .value("Conduit", SpecialModelType::Conduit)
      .value("CopperGolemStatue", SpecialModelType::CopperGolemStatue)
      .value("DecoratedPot", SpecialModelType::DecoratedPot)
      .value("EndCube", SpecialModelType::EndCube)
      .value("Head", SpecialModelType::Head)
      .value("PlayerHead", SpecialModelType::PlayerHead)
      .value("Shield", SpecialModelType::Shield)
      .value("ShulkerBox", SpecialModelType::ShulkerBox)
      .value("Trident", SpecialModelType::Trident);

  emscripten::enum_<Attachment>("Attachment")
      .value("Ground", Attachment::Ground)
      .value("Wall", Attachment::Wall);

  emscripten::enum_<DyeColor>("DyeColor")
      .value("White", DyeColor::White)
      .value("Orange", DyeColor::Orange)
      .value("Magenta", DyeColor::Magenta)
      .value("LightBlue", DyeColor::LightBlue)
      .value("Yellow", DyeColor::Yellow)
      .value("Lime", DyeColor::Lime)
      .value("Pink", DyeColor::Pink)
      .value("Gray", DyeColor::Gray)
      .value("LightGray", DyeColor::LightGray)
      .value("Cyan", DyeColor::Cyan)
      .value("Purple", DyeColor::Purple)
      .value("Blue", DyeColor::Blue)
      .value("Brown", DyeColor::Brown)
      .value("Green", DyeColor::Green)
      .value("Red", DyeColor::Red)
      .value("Black", DyeColor::Black);

  emscripten::enum_<HeadKind>("HeadKind")
      .value("Skeleton", HeadKind::Skeleton)
      .value("WitherSkeleton", HeadKind::WitherSkeleton)
      .value("Player", HeadKind::Player)
      .value("Zombie", HeadKind::Zombie)
      .value("Creeper", HeadKind::Creeper)
      .value("Piglin", HeadKind::Piglin)
      .value("Dragon", HeadKind::Dragon);

  emscripten::enum_<ChestType>("ChestType")
      .value("Single", ChestType::Single)
      .value("Left", ChestType::Left)
      .value("Right", ChestType::Right);

  emscripten::enum_<CopperGolemPose>("CopperGolemPose")
      .value("Sitting", CopperGolemPose::Sitting)
      .value("Running", CopperGolemPose::Running)
      .value("Star", CopperGolemPose::Star)
      .value("Standing", CopperGolemPose::Standing);

  emscripten::enum_<EndCubeEffect>("EndCubeEffect")
      .value("Gateway", EndCubeEffect::Gateway)
      .value("Portal", EndCubeEffect::Portal);

  emscripten::enum_<TintSourceType>("TintSourceType")
      .value("Constant", TintSourceType::Constant)
      .value("Dye", TintSourceType::Dye)
      .value("Firework", TintSourceType::Firework)
      .value("Grass", TintSourceType::Grass)
      .value("MapColor", TintSourceType::MapColor)
      .value("Potion", TintSourceType::Potion)
      .value("Team", TintSourceType::Team)
      .value("CustomModelData", TintSourceType::CustomModelData);

  emscripten::enum_<DisplayTransformType>("DisplayTransformType")
      .value("ThirdPersonLeftHand", DisplayTransformType::ThirdPersonLeftHand)
      .value("ThirdPersonRightHand", DisplayTransformType::ThirdPersonRightHand)
      .value("FirstPersonLeftHand", DisplayTransformType::FirstPersonLeftHand)
      .value("FirstPersonRightHand", DisplayTransformType::FirstPersonRightHand)
      .value("Ground", DisplayTransformType::Ground)
      .value("Gui", DisplayTransformType::Gui)
      .value("Head", DisplayTransformType::Head)
      .value("Fixed", DisplayTransformType::Fixed);

  emscripten::enum_<FaceDirection>("FaceDirection")
      .value("North", FaceDirection::North)
      .value("South", FaceDirection::South)
      .value("Up", FaceDirection::Up)
      .value("Down", FaceDirection::Down)
      .value("West", FaceDirection::West)
      .value("East", FaceDirection::East);

  emscripten::enum_<GuiLight>("GuiLight")
      .value("Front", GuiLight::Front)
      .value("Side", GuiLight::Side);

  // Core types
  emscripten::value_object<Vec3>("Vec3")
      .field("x", &Vec3::x)
      .field("y", &Vec3::y)
      .field("z", &Vec3::z);

  emscripten::value_object<RgbColor>("RgbColor")
      .field("packed", &RgbColor::packed_)
      .field("rgb", &RgbColor::rgb_);

  emscripten::value_object<Transformation>("Transformation")
      .field("matrix", &Transformation::matrix)
      .field("translation", &Transformation::translation)
      .field("scale", &Transformation::scale)
      .field("rotation", &Transformation::rotation)
      .field("rightRotation", &Transformation::rightRotation);

  // Identifier
  emscripten::class_<Identifier>("Identifier")
      .constructor<>()
      .constructor<std::string, std::string>()
      .function("getNamespace", &Identifier::getNamespace)
      .function("getPath", &Identifier::getPath)
      .function("toString", &Identifier::toString)
      .class_function("parse", &Identifier::parse);

  // Mesh types
  emscripten::value_object<Uv>("Uv")
      .field("uFrom", &Uv::uFrom)
      .field("vFrom", &Uv::vFrom)
      .field("uTo", &Uv::uTo)
      .field("vTo", &Uv::vTo);

  emscripten::value_object<Face>("Face")
      .field("uv", &Face::uv)
      .field("texture", &Face::texture)
      .field("cullface", &Face::cullface)
      .field("rotation", &Face::rotation);

  emscripten::value_object<Element>("Element")
      .field("from", &Element::from)
      .field("to", &Element::to)
      .field("faces", &Element::faces);

  emscripten::value_object<DisplayTransform>("DisplayTransform")
      .field("rotation", &DisplayTransform::rotation)
      .field("translation", &DisplayTransform::translation)
      .field("scale", &DisplayTransform::scale);

  emscripten::value_object<Model>("Model")
      .field("parent", &Model::parent)
      .field("guiLight", &Model::guiLight)
      .field("textures", &Model::textures)
      .field("elements", &Model::elements)
      .field("display", &Model::display);

  // Image
  emscripten::value_object<Pixel>("Pixel")
      .field("r", &Pixel::r)
      .field("g", &Pixel::g)
      .field("b", &Pixel::b)
      .field("a", &Pixel::a);

  emscripten::class_<Image>("Image")
      .constructor<>()
      .constructor<int, int, std::vector<uint8_t>>()
      .function("width", &Image::width)
      .function("height", &Image::height)
      .function("empty", &Image::empty)
      .function("at", &Image::at)
      .function("isOpaque", &Image::isOpaque);

  // MeshBuilder
  emscripten::class_<MeshBuilder>("MeshBuilder")
      .constructor<const Image &>()
      .function("build", &MeshBuilder::build);

  // Functions
  emscripten::function("itemModelSerialize", &itemModelSerialize);
  emscripten::function("itemModelDeserialize", &itemModelDeserialize);
  emscripten::function("itemModelDefinitionSerialize",
                       &itemModelDefinitionSerialize);
  emscripten::function("itemModelDefinitionDeserialize",
                       &itemModelDefinitionDeserialize);
  emscripten::function("specialModelSerialize", &specialModelSerialize);
  emscripten::function("tintSourceSerialize", &tintSourceSerialize);

  emscripten::function("buildMesh", &buildMesh);
  emscripten::function("modelSerialize", &modelSerialize);
  emscripten::function("modelDeserialize", &modelDeserialize);

  emscripten::function("identifierToString", &identifierToString);
  emscripten::function("identifierParse", &identifierParse);

  emscripten::function("vec3Serialize", &vec3Serialize);
  emscripten::function("transformationSerialize", &transformationSerialize);
}