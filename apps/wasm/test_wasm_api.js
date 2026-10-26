// node test

const createWasmModule = require('../../build_wasm/bin/wasm_api.js');

async function runTests() {
  const module = await createWasmModule();
  let passed = 0, failed = 0;

  function assert(condition, msg) {
    if (condition) {
      console.log(`  OK ${msg}`);
      passed++;
    } else {
      console.log(`  ER ${msg}`);
      failed++;
    }
  }

  function assertEqual(actual, expected, msg) {
    if (JSON.stringify(actual) === JSON.stringify(expected)) {
      console.log(`  OK ${msg}`);
      passed++;
    } else {
      console.log(`  ER ${msg}`);
      console.log(`    Expected: ${JSON.stringify(expected)}`);
      console.log(`    Actual:   ${JSON.stringify(actual)}`);
      failed++;
    }
  }

  console.log('\nItem Model Tests');

  // Test itemModelSerialize - simple model
  {
    const input = JSON.stringify({
      type: "minecraft:model",
      model: "minecraft:item/diamond_sword"
    });
    const result = JSON.parse(module.itemModelSerialize(input));
    assertEqual(result.type, "minecraft:model", "itemModelSerialize: basic model type");
    assertEqual(result.model, "minecraft:item/diamond_sword", "itemModelSerialize: model path");
  }

  // Test itemModelSerialize - with transformation
  {
    const input = JSON.stringify({
      type: "minecraft:model",
      model: "minecraft:item/diamond_sword",
      transformation: {
        translation: [0, 1, 2],
        scale: [0.5, 0.5, 0.5],
        rotation: [0, 90, 0]
      }
    });
    const result = JSON.parse(module.itemModelSerialize(input));
    assertEqual(result.transformation.translation, [0, 1, 2], "itemModelSerialize: transformation translation");
    assertEqual(result.transformation.scale, [0.5, 0.5, 0.5], "itemModelSerialize: transformation scale");
  }

  // Test itemModelSerialize - composite
  {
    const input = JSON.stringify({
      type: "minecraft:composite",
      models: [
        { type: "minecraft:model", model: "minecraft:item/diamond_sword" },
        { type: "minecraft:model", model: "minecraft:item/shield" }
      ]
    });
    const result = JSON.parse(module.itemModelSerialize(input));
    assertEqual(result.type, "minecraft:composite", "itemModelSerialize: composite type");
    assertEqual(result.models.length, 2, "itemModelSerialize: composite models count");
  }

  // Test itemModelSerialize - condition
  {
    const input = JSON.stringify({
      type: "minecraft:condition",
      property: "minecraft:damaged",
      on_true: { type: "minecraft:model", model: "minecraft:item/damaged_sword" },
      on_false: { type: "minecraft:model", model: "minecraft:item/diamond_sword" }
    });
    const result = JSON.parse(module.itemModelSerialize(input));
    assertEqual(result.type, "minecraft:condition", "itemModelSerialize: condition type");
    assertEqual(result.property, "minecraft:damaged", "itemModelSerialize: condition property");
    assert(result.on_true !== undefined, "itemModelSerialize: on_true present");
    assert(result.on_false !== undefined, "itemModelSerialize: on_false present");
  }

  // Test itemModelSerialize - select
  {
    const input = JSON.stringify({
      type: "minecraft:select",
      property: "minecraft:main_hand",
      cases: [
        { when: "left", model: { type: "minecraft:model", model: "minecraft:item/left_hand" } },
        { when: "right", model: { type: "minecraft:model", model: "minecraft:item/right_hand" } }
      ],
      fallback: { type: "minecraft:model", model: "minecraft:item/default" }
    });
    const result = JSON.parse(module.itemModelSerialize(input));
    assertEqual(result.type, "minecraft:select", "itemModelSerialize: select type");
    assertEqual(result.cases.length, 2, "itemModelSerialize: select cases count");
    assert(result.fallback !== undefined, "itemModelSerialize: select fallback present");
  }

  // Test itemModelSerialize - range_dispatch
  {
    const input = JSON.stringify({
      type: "minecraft:range_dispatch",
      property: "minecraft:damage",
      scale: 1.0,
      entries: [
        { threshold: 0.0, model: { type: "minecraft:model", model: "minecraft:item/full" } },
        { threshold: 0.5, model: { type: "minecraft:model", model: "minecraft:item/half" } }
      ]
    });
    const result = JSON.parse(module.itemModelSerialize(input));
    assertEqual(result.type, "minecraft:range_dispatch", "itemModelSerialize: range_dispatch type");
    assertEqual(result.entries.length, 2, "itemModelSerialize: range_dispatch entries count");
  }

  // Test itemModelDefinitionSerialize
  {
    const input = JSON.stringify({
      model: { type: "minecraft:model", model: "minecraft:item/diamond_sword" },
      hand_animation_on_swap: false,
      oversized_in_gui: true,
      swap_animation_scale: 1.5
    });
    const result = JSON.parse(module.itemModelDefinitionSerialize(input));
    assertEqual(result.model.type, "minecraft:model", "itemModelDefinitionSerialize: model type");
    assertEqual(result.hand_animation_on_swap, false, "itemModelDefinitionSerialize: hand_animation_on_swap");
    assertEqual(result.oversized_in_gui, true, "itemModelDefinitionSerialize: oversized_in_gui");
    assertEqual(result.swap_animation_scale, 1.5, "itemModelDefinitionSerialize: swap_animation_scale");
  }

  console.log('\nSpecial Model Tests');

  // Test specialModelSerialize - banner
  {
    const input = JSON.stringify({
      type: "minecraft:banner",
      color: "red",
      attachment: "ground"
    });
    const result = JSON.parse(module.specialModelSerialize(input));
    assertEqual(result.type, "minecraft:banner", "specialModelSerialize: banner type");
    assertEqual(result.color, "red", "specialModelSerialize: banner color");
    // attachment is only serialized if not default (ground)
    assert(result.attachment === undefined || result.attachment === "ground", "specialModelSerialize: banner attachment (default omitted)");
  }

  // Test specialModelSerialize - chest
  {
    const input = JSON.stringify({
      type: "minecraft:chest",
      texture: "minecraft:entity/chest/normal",
      chest_type: "single",
      openness: 0.5
    });
    const result = JSON.parse(module.specialModelSerialize(input));
    assertEqual(result.type, "minecraft:chest", "specialModelSerialize: chest type");
    assertEqual(result.texture, "minecraft:entity/chest/normal", "specialModelSerialize: chest texture");
    // chest_type is only serialized if not default (single)
    assert(result.chest_type === undefined || result.chest_type === "single", "specialModelSerialize: chest_type (default omitted)");
    assertEqual(result.openness, 0.5, "specialModelSerialize: openness");
  }

  // Test specialModelSerialize - book
  {
    const input = JSON.stringify({
      type: "minecraft:book",
      open_angle: 1.57,
      page1: 0.3,
      page2: 0.7
    });
    const result = JSON.parse(module.specialModelSerialize(input));
    assertEqual(result.type, "minecraft:book", "specialModelSerialize: book type");
    assertEqual(result.open_angle, 1.57, "specialModelSerialize: open_angle");
    assertEqual(result.page1, 0.3, "specialModelSerialize: page1");
    assertEqual(result.page2, 0.7, "specialModelSerialize: page2");
  }

  console.log('\nTint Source Tests');

  // Test tintSourceSerialize - constant
  {
    const input = JSON.stringify({
      type: "minecraft:constant",
      value: 0xFF0000
    });
    const result = JSON.parse(module.tintSourceSerialize(input));
    assertEqual(result.type, "minecraft:constant", "tintSourceSerialize: constant type");
    assertEqual(result.value, 0xFF0000, "tintSourceSerialize: constant value (packed)");
  }

  // Test tintSourceSerialize - dye
  {
    const input = JSON.stringify({
      type: "minecraft:dye",
      default: [1.0, 0.0, 0.0]
    });
    const result = JSON.parse(module.tintSourceSerialize(input));
    assertEqual(result.type, "minecraft:dye", "tintSourceSerialize: dye type");
    assertEqual(result.default, [1.0, 0.0, 0.0], "tintSourceSerialize: dye default (rgb)");
  }

  // Test tintSourceSerialize - grass
  {
    const input = JSON.stringify({
      type: "minecraft:grass",
      temperature: 0.8,
      downfall: 0.4
    });
    const result = JSON.parse(module.tintSourceSerialize(input));
    assertEqual(result.type, "minecraft:grass", "tintSourceSerialize: grass type");
    assertEqual(result.temperature, 0.8, "tintSourceSerialize: temperature");
    assertEqual(result.downfall, 0.4, "tintSourceSerialize: downfall");
  }

  console.log('\nMesh API Tests');

  // Test buildMesh - simple 2x2 opaque image
  {
    // 2x2 RGBA image, all opaque red
    const pixels = new Uint8Array([
      255, 0, 0, 255,  255, 0, 0, 255,
      255, 0, 0, 255,  255, 0, 0, 255
    ]);
    const imageData = { width: 2, height: 2, pixels: Array.from(pixels) };
    const options = { thicknessPx: 1, textureKey: "test" };
    const modelJson = module.buildMesh(JSON.stringify(imageData), JSON.stringify(options));
    const model = JSON.parse(modelJson);
    assert(model.textures !== undefined, "buildMesh: textures present");
    assert(model.textures.particle !== undefined, "buildMesh: particle texture");
    assert(model.elements !== undefined, "buildMesh: elements present");
    assert(model.elements.length > 0, "buildMesh: has elements");
    assert(model.display !== undefined, "buildMesh: display present");
  }

  // Test buildMesh with full options
  {
    const pixels = new Uint8Array(16 * 16 * 4).fill(255);
    const imageData = { width: 16, height: 16, pixels: Array.from(pixels) };
    const options = {
      thicknessPx: 2.0,
      textureNamespace: "minecraft",
      textureKey: "diamond_sword",
      cullTransparent: true,
      mergeColumns: false,
      outlineOnly: false,
      outlineSizePx: 1.0,
      outlineTextureNamespace: "minecraft",
      outlineTextureKey: "outline"
    };
    const modelJson = module.buildMesh(JSON.stringify(imageData), JSON.stringify(options));
    const model = JSON.parse(modelJson);
    assert(model.textures !== undefined, "buildMesh full options: textures present");
    assert(model.textures.particle !== undefined, "buildMesh full options: particle texture");
    assert(model.textures.outline !== undefined, "buildMesh full options: outline texture");
    assert(model.elements.length > 0, "buildMesh full options: has elements");
  }

  // Test buildMesh - with transparency (cullTransparent)
  {
    // 2x2 image: top-left opaque, others transparent
    const pixels = new Uint8Array([
      255, 0, 0, 255,  0, 0, 0, 0,
      0, 0, 0, 0,      0, 0, 0, 0
    ]);
    const imageData = { width: 2, height: 2, pixels: Array.from(pixels) };
    const options = { thicknessPx: 1, cullTransparent: true, textureKey: "test" };
    const modelJson = module.buildMesh(JSON.stringify(imageData), JSON.stringify(options));
    const model = JSON.parse(modelJson);
    // Should only have 1 element (the opaque pixel)
    assertEqual(model.elements.length, 1, "buildMesh: cullTransparent filters transparent");
  }

  // Test buildMesh - outline
  {
    const pixels = new Uint8Array(16 * 16 * 4);
    // Make a simple square in the middle
    for (let y = 4; y < 12; y++) {
      for (let x = 4; x < 12; x++) {
        const idx = (y * 16 + x) * 4;
        pixels[idx] = 255; pixels[idx+1] = 0; pixels[idx+2] = 0; pixels[idx+3] = 255;
      }
    }
    const imageData = { width: 16, height: 16, pixels: Array.from(pixels) };
    const options = { thicknessPx: 1, outlineSizePx: 1, outlineOnly: true, textureKey: "test" };
    const modelJson = module.buildMesh(JSON.stringify(imageData), JSON.stringify(options));
    const model = JSON.parse(modelJson);
    assert(model.textures.outline !== undefined, "buildMesh: outline texture present");
    assert(model.elements.length > 0, "buildMesh: outline has elements");
  }

  // Test modelSerialize/Deserialize roundtrip
  {
    const model = {
      parent: "minecraft:item/generated",
      gui_light: "front",
      textures: { layer0: "minecraft:item/diamond" },
      elements: [
        {
          from: [0, 0, 8], to: [16, 16, 8],
          faces: {
            north: { uv: [0, 0, 16, 16], texture: "#layer0" },
            south: { uv: [0, 0, 16, 16], texture: "#layer0" }
          }
        }
      ],
      display: {
        gui: { rotation: [30, 225, 0], translation: [0, 0, 0], scale: [0.625, 0.625, 0.625] }
      }
    };
    const serialized = module.modelSerialize(JSON.stringify(model));
    const deserialized = JSON.parse(module.modelDeserialize(serialized));
    assertEqual(deserialized.parent, "minecraft:item/generated", "modelDeserialize: parent");
    assertEqual(deserialized.textures.layer0, "minecraft:item/diamond", "modelDeserialize: texture");
    assertEqual(deserialized.elements.length, 1, "modelDeserialize: elements count");
  }

  console.log('\nIdentifier Tests');

  // Test identifierToString
  {
    const result = module.identifierToString("minecraft", "diamond_sword");
    assertEqual(result, "minecraft:diamond_sword", "identifierToString");
  }

  // Test identifierParse
  {
    const result = module.identifierParse("minecraft:diamond_sword");
    assertEqual(result, "minecraft:diamond_sword", "identifierParse");
  }

  console.log('\nVec3 Tests');

  // Test vec3Serialize
  {
    const input = JSON.stringify([1.5, 2.5, 3.5]);
    const result = JSON.parse(module.vec3Serialize(input));
    assertEqual(result, [1.5, 2.5, 3.5], "vec3Serialize");
  }

  console.log('\nTransformation Tests');

  // Test transformationSerialize
  {
    const input = JSON.stringify({
      translation: [1, 2, 3],
      scale: [0.5, 0.5, 0.5],
      rotation: [0, 90, 0]
    });
    const result = JSON.parse(module.transformationSerialize(input));
    assertEqual(result.translation, [1, 2, 3], "transformationSerialize: translation");
    assertEqual(result.scale, [0.5, 0.5, 0.5], "transformationSerialize: scale");
    assertEqual(result.rotation, [0, 90, 0], "transformationSerialize: rotation");
  }

  console.log(`\nResults: ${passed} passed, ${failed} failed`);
  if (failed > 0) process.exit(1);
}

runTests().catch(err => {
  console.error('Test error:', err);
  process.exit(1);
});