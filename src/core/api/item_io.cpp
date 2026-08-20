#include "core/api/item_io.hpp"
#include "libraries/nlohmann/json.hpp"

#include <fstream>
#include <stdexcept>

namespace enchantment_tweaks::item {

using nlohmann::json;

namespace {

json ToJson(const mesh::UV& uv) { return json::array({uv[0], uv[1], uv[2], uv[3]}); }

json ToJson(const mesh::Vec3& v) { return json::array({v[0], v[1], v[2]}); }

json ToJson(const mesh::Face& f) {
    json j;
    j["uv"] = ToJson(f.uv);
    j["texture"] = f.texture;
    return j;
}

json ToJson(const mesh::Element& e) {
    json j;
    j["from"] = ToJson(e.from);
    j["to"] = ToJson(e.to);

    // Fixed emission order for stable, human-readable output, matching the
    // order faces are added in the reference script.
    static constexpr const char* kFaceOrder[] = {"north", "south", "up",
                                                   "down",  "west",  "east"};
    json faces = json::object();
    for (const char* name : kFaceOrder) {
        auto it = e.faces.find(name);
        if (it != e.faces.end()) faces[name] = ToJson(it->second);
    }
    j["faces"] = faces;
    return j;
}

json ToJson(const mesh::DisplayTransform& d) {
    json j = json::object();
    if (d.rotation) j["rotation"] = ToJson(*d.rotation);
    if (d.translation) j["translation"] = ToJson(*d.translation);
    if (d.scale) j["scale"] = ToJson(*d.scale);
    return j;
}

json ToJson(const mesh::Model& m) {
    json j;
    j["textures"] = m.textures;

    json elements = json::array();
    for (const auto& e : m.elements) elements.push_back(ToJson(e));
    j["elements"] = elements;

    json display = json::object();
    for (const auto& [name, transform] : m.display) display[name] = ToJson(transform);
    j["display"] = display;

    return j;
}

}  // namespace

std::string serialize(const mesh::Model& model, int indent) {
    const json j = ToJson(model);
    return indent < 0 ? j.dump() : j.dump(indent);
}

void writeFile(const std::string& path, const mesh::Model& model, int indent) {
    std::ofstream out(path, std::ios::binary);
    if (!out) {
        throw std::runtime_error("enchantment_tweaks::item_io::writeFile: cannot open '" + path +
                                  "' for writing");
    }
    out << serialize(model, indent);
    if (!out) {
        throw std::runtime_error("enchantment_tweaks::item_io::writeFile: write failed for '" + path +
                                  "'");
    }
}

}  // namespace enchantment_tweaks::item