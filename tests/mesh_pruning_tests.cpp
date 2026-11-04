#include "core/api/mesh.hpp"
#include "core/util/debug.hpp"
#include "test_utils.hpp"

#include <set>
#include <string>

using namespace enchantment_tweaks;
using namespace enchantment_tweaks::mesh;
using nlohmann::json;

namespace {

// Opaque pixels are 255 alpha, transparent are 0.
image::Image grid(int w, int h, const std::string &rows) {
  std::vector<uint8_t> px(static_cast<size_t>(w) * h * 4, 0);
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      if (rows[static_cast<size_t>(y) * w + x] == '#')
        px[(static_cast<size_t>(y) * w + x) * 4 + 3] = 255;
    }
  }
  return {w, h, px};
}

// Faces of each body element (the ones textured "#texture"), keyed by its
// sprite cell.
std::set<std::string> faces(const Model &model) {
  std::set<std::string> out;
  const json j = model.serialize();
  for (const auto &el : j.at("elements")) {
    const auto &from = el.at("from");
    const std::string cell = std::to_string(from[0].get<double>()) + "," +
                             std::to_string(from[1].get<double>());
    for (const auto &[dir, face] : el.at("faces").items()) {
      if (face.at("texture") == "#texture") out.insert(cell + ":" + dir);
    }
  }
  return out;
}

Model build(const image::Image &img) {
  BuildOptions o;
  o.cullTransparent = true;
  o.outlineOnly = false; // the body, not only the outline
  o.outlineSizePx = 0.0;
  return MeshBuilder(img).build(o);
}

} // namespace

int main() {
  test_util::Suite suite;

  suite.run("a lone pixel keeps all six faces", [] {
    const auto f = faces(build(grid(1, 1, "#")));
    test_util::check(f.size() == 6,
                     "expected 6 faces, got " + std::to_string(f.size()));
  });

  suite.run("two touching pixels: the side between them is not written", [] {
    // "##": the east face of the left pixel and the west face of the right one
    // are inside the solid.
    const auto f = faces(build(grid(2, 1, "##")));
    // Each pixel: 2 sprite faces, top and bottom, and one outer side (the left
    // pixel's west, the right pixel's east). The inner sides are gone: 2*2 +
    // 2*2 + 2 = 10.
    test_util::check(f.size() == 10,
                     "expected 10 faces, got " + std::to_string(f.size()));
    // The two pixels are at x=0 and x=1 in model units; the inner sides would
    // be "east" of x=0 and "west" of x=1. Neither may appear.
    bool interiorSide = false;
    for (const auto &key : f) {
      if ((key.find(":east") != std::string::npos && key.starts_with("0,")) ||
          (key.find(":west") != std::string::npos && key.starts_with("1,")))
        interiorSide = true;
    }
    test_util::check(!interiorSide, "an interior side was written");
  });

  suite.run("a transparent neighbor keeps the side that faces it", [] {
    // "##." : the left pixel's east side is inside; the right pixel's east side
    // faces air.
    const auto model = build(grid(3, 1, "##."));
    const json j = model.serialize();
    int east = 0, west = 0;
    for (const auto &el : j.at("elements")) {
      if (el.at("faces").contains("east")) ++east;
      if (el.at("faces").contains("west")) ++west;
    }
    test_util::check(
        east == 1, "exactly one east side expected (the one facing air), got " +
                       std::to_string(east));
    test_util::check(west == 1,
                     "exactly one west side expected (the left edge), got " +
                         std::to_string(west));
  });

  suite.run("an interior pixel of a ring writes only its sprite faces", [] {
    // 3x3 ring with a transparent center. Only the center is transparent, so
    // the four pixels around it see air on one side each and keep those sides.
    const auto model = build(grid(3, 3,
                                  "###"
                                  "#.#"
                                  "###"));
    const json j = model.serialize();
    test_util::check(j.at("elements").size() == 8,
                     "8 opaque pixels expected, got " +
                         std::to_string(j.at("elements").size()));
    int sideFaces = 0;
    for (const auto &el : j.at("elements")) {
      for (const auto &[dir, face] : el.at("faces").items()) {
        if (dir != "north" && dir != "south") ++sideFaces;
      }
    }
    // Every side that faces the hole or the outside: the 8 pixels have 12 sides
    // on the hole or edge (4 per pixel next to the hole counted once each
    // side), plus the outer edges.
    test_util::check(sideFaces == 16, "side face count " +
                                          std::to_string(sideFaces) +
                                          ", expected 16");
  });

  return suite.finish();
}
