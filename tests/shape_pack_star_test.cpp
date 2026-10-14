// The tracer join (Tomodachi #331): the stand-in Star Kit, bundled by the
// converter (#330), built into a Shape group from a full Look and drawn.
//
// The Star's Kit coordinates (80×80 artboard, origin at its centre) put the
// Body's lowest closed-ring points at y 68 → +28 and its x span 4..76 → ±36,
// so its feet line is (0, 28) and a pet placed at (80, 150) spans x 44..115
// and draws nothing at or below y 150.

#include <enjin2/graphics/asset_arena.hpp>
#include <enjin2/graphics/canvas.hpp>
#include <enjin2/graphics/shape_pack.hpp>
#include <enjin2/import/shape_bundle.hpp>
#include <enjin2/shape/shape_pack_group.hpp>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

using namespace enjin2;
using namespace enjin2::shape;

static int passes = 0;
static int failures = 0;

#define ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            fprintf(stderr, "FAIL: %s (line %d)\n", msg, __LINE__); \
            failures++; \
        } else { \
            passes++; \
        } \
    } while (0)

static std::vector<uint8_t> readFile(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

int main() {
    const std::string dir = std::string(ENJIN2_SOURCE_DIR) + "/tools/testdata/shapes/";
    const ShapeBundleResult r = bundleShapes(
        {{"star.svg", readFile(dir + "star.svg"), readFile(dir + "star.svg.shape.json")}},
        std::string(reinterpret_cast<const char*>(readFile(dir + "ledger.json").data()), readFile(dir + "ledger.json").size()));
    ASSERT(r.ok(), "the Star bundles");
    if (!r.ok()) {
        for (const auto& i : r.issues) fprintf(stderr, "  %s: %s\n", i.code.c_str(), i.message.c_str());
        return 1;
    }

    ShapePackMemory src(r.pack.data(), r.pack.size());
    ShapePack pack;
    ShapeError e;
    ASSERT(pack.open(src, &e), e.text);
    AssetArena arena;
    arena.allocateBacking(64 * 1024);

    ShapeLook look;
    for (uint8_t s = 0; s < SHAPE_SLOT_COUNT; ++s) look.set(ShapeSlot(s), 1);
    look.colour[0] = 9;
    look.colour[1] = 4;
    look.colour[2] = 0;

    ShapePackGroup pet;
    ASSERT(pet.build(pack, look, arena, &e), e.text);
    ASSERT(arena.used() <= pack.lookBufferBytes() + AssetArena::ALIGNMENT, "one Look buffer from the arena");
    for (uint8_t s = 0; s < SHAPE_SLOT_COUNT; ++s)
        ASSERT(pet.poseName(ShapeSlot(s)) != nullptr, "every slot of the full Look is loaded");

    static Canvas4<160, 160> layer;
    layer.clear(Pixel4(15));
    static ShapeDrawScratch scratch;
    pet.setPosition({Fixed::fromInt(80), Fixed::fromInt(150)});
    ASSERT(pet.redraw(layer, scratch), "the Star draws without refusals");

    int minX = 160, maxX = -1, maxY = -1, colours[16] = {};
    for (int y = 0; y < 160; ++y)
        for (int x = 0; x < 160; ++x) {
            const uint8_t v = layer.getPixel(int16_t(x), int16_t(y)).value;
            if (v == 15) continue;
            ++colours[v];
            if (x < minX) minX = x;
            if (x > maxX) maxX = x;
            if (y > maxY) maxY = y;
        }
    ASSERT(colours[9] > 1500, "the Body is painted in the body colour");
    ASSERT(colours[4] > 20, "cheeks and arms in the accent colour");
    ASSERT(colours[0] > 10, "eyes and mouth in ink");
    ASSERT(maxY < 150 && maxY >= 146, "the pet stands on its feet line (legs reach just above it)");
    // Arms reach past the Body: x 4 and 76 (Kit) → the arm tips at 44.. and ..115.
    ASSERT(minX >= 42 && minX <= 46 && maxX >= 113 && maxX <= 117, "symmetric about x 80");

    if (getenv("SHAPE_DUMP")) {  // SHAPE_DUMP=1 shape_pack_star_test: the pet as text
        for (int y = 100; y < 152; ++y) {
            for (int x = 40; x < 120; ++x) {
                const uint8_t v = layer.getPixel(int16_t(x), int16_t(y)).value;
                putchar(v == 15 ? '.' : "0123456789abcdef"[v]);
            }
            putchar('\n');
        }
    }

    printf("shape_pack_star_test: %d passed, %d failed\n", passes, failures);
    return failures == 0 ? 0 : 1;
}
