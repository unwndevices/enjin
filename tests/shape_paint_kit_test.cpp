// Paint goldens (Tomodachi #332): the stand-in Star and Chestnut Kits, bundled
// by the CLI (ctest shape_paint_cli_bundle), drawn by a Shape group, each paint
// feature compared with its golden picture in tests/testdata/shape/paint/.
//
//   shape_paint_kit_test <pack.njn>            compare
//   shape_paint_kit_test <pack.njn> --update   rewrite the goldens (review the diff!)
//
// A golden is the layer over one window as rows of palette indices ('.' = 15,
// transparent). Look colours: body 9, accent 4, ink 0. The Star stands at
// (80, 150): Kit (artboard) point (x, y) lands on screen (x + 40, y + 82).

#include <enjin2/graphics/asset_arena.hpp>
#include <enjin2/graphics/canvas.hpp>
#include <enjin2/graphics/shape_pack.hpp>
#include <enjin2/shape/shape_pack_group.hpp>

#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

using namespace enjin2;
using namespace enjin2::shape;

static int passes = 0;
static int failures = 0;
static bool update = false;

#define ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            fprintf(stderr, "FAIL: %s (line %d)\n", msg, __LINE__); \
            failures++; \
        } else { \
            passes++; \
        } \
    } while (0)

using Layer = Canvas4<160, 160>;

static std::string picture(const Layer& c, int x0, int y0, int w, int h) {
    std::string s;
    for (int y = y0; y < y0 + h; ++y) {
        for (int x = x0; x < x0 + w; ++x) {
            const uint8_t v = c.getPixel(static_cast<int16_t>(x), static_cast<int16_t>(y)).value;
            s += v == 15 ? '.' : "0123456789abcdef"[v];
        }
        s += '\n';
    }
    return s;
}

static std::string goldenPath(const char* name) {
    return std::string(ENJIN2_SOURCE_DIR) + "/tests/testdata/shape/paint/" + name + ".txt";
}

// Compare (or with --update, write) golden `name` = the layer over the window.
static bool golden(const Layer& c, const char* name, int x0, int y0, int w, int h) {
    char head[96];
    std::snprintf(head, sizeof head, "# %s: x %d, y %d, %d x %d\n", name, x0, y0, w, h);
    const std::string got = std::string(head) + picture(c, x0, y0, w, h);
    if (update) {
        std::ofstream(goldenPath(name), std::ios::binary) << got;
        return true;
    }
    std::ifstream in(goldenPath(name), std::ios::binary);
    const std::string want((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (got == want) return true;
    fprintf(stderr, "  golden %s differs\n  want:\n%s\n  got:\n%s\n", name, want.c_str(), got.c_str());
    return false;
}

static ShapeLook starLook() {
    ShapeLook l;
    for (uint8_t s = 0; s < SHAPE_SLOT_COUNT; ++s) l.set(ShapeSlot(s), 1);
    l.colour[0] = 9;
    l.colour[1] = 4;
    l.colour[2] = 0;
    return l;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: shape_paint_kit_test <pack.njn> [--update]\n");
        return 2;
    }
    update = argc > 2 && std::strcmp(argv[2], "--update") == 0;
    std::ifstream in(argv[1], std::ios::binary);
    const std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    ShapePackMemory src(bytes.data(), bytes.size());
    ShapePack pack;
    ShapeError e;
    ASSERT(pack.open(src, &e), e.text);
    if (failures) return 1;
    AssetArena arena;
    arena.allocateBacking(64 * 1024);
    static ShapeDrawScratch scratch;
    static Layer layer;

    // The Star in its default poses.
    ShapePackGroup star;
    ASSERT(star.build(pack, starLook(), arena, &e), e.text);
    layer.clear(Pixel4(15));
    star.setPosition({Fixed::fromInt(80), Fixed::fromInt(150)});
    ASSERT(star.redraw(layer, scratch), "the Star draws");
    ASSERT(golden(layer, "antenna-stroke", 72, 80, 18, 26), "antenna: a 1 px stem (open path) under the bulb");
    ASSERT(golden(layer, "cheeks-dither", 58, 106, 44, 14), "cheeks: 50 % Bayer, the Body seen through");
    ASSERT(golden(layer, "mouth-neutral-line", 73, 111, 14, 12), "neutral mouth: a pixel-perfect 1 px curve");

    // The mouth's other poses.
    struct Pose { const char* pose; const char* name; const char* what; };
    const Pose poses[] = {
        {"happy", "mouth-happy-clip", "happy mouth: the tongue clipped inside the mouth"},
        {"open", "mouth-open-hole", "open mouth: an even-odd ring with its hole"},
        {"surprised", "mouth-surprised-stroke", "surprised mouth: accent with a 1 px ink inner stroke"},
    };
    for (const Pose& p : poses) {
        ASSERT(star.setPose(ShapeSlot::Mouth, p.pose, &e), e.text);
        star.redraw(layer, scratch);
        ASSERT(golden(layer, p.name, 73, 111, 14, 12), p.what);
    }

    // The Chestnut's cap on the Star's Body: clipped to the live Body (the Star's
    // outline, not the Chestnut's), and it follows the Body when the group moves.
    {
        ShapeLook l = starLook();
        l.set(ShapeSlot::Top, 2);
        ShapePackGroup pet;
        ASSERT(pet.build(pack, l, arena, &e), e.text);
        layer.clear(Pixel4(15));
        pet.setPosition({Fixed::fromInt(80), Fixed::fromInt(150)});
        ASSERT(pet.redraw(layer, scratch), "the capped Star draws");
        ASSERT(golden(layer, "cap-live-body", 40, 80, 80, 46), "cap: cut to the Star's Body");
        const std::string at = picture(layer, 40, 80, 80, 46);
        pet.setPosition({Fixed::fromInt(83), Fixed::fromInt(152)});
        ASSERT(pet.redraw(layer, scratch), "the moved capped Star draws");
        ASSERT(picture(layer, 43, 82, 80, 46) == at, "moved by (3, 2): the clipped cap moves with the Body, pixel for pixel");
    }

    // The whole Chestnut: its cap clipped to its own Body.
    {
        ShapeLook l;
        l.set(ShapeSlot::Body, 2);
        l.set(ShapeSlot::Eyes, 2);
        l.set(ShapeSlot::Mouth, 2);
        l.set(ShapeSlot::Top, 2);
        l.colour[0] = 9;
        l.colour[1] = 4;
        l.colour[2] = 0;
        ShapePackGroup pet;
        ASSERT(pet.build(pack, l, arena, &e), e.text);
        layer.clear(Pixel4(15));
        pet.setPosition({Fixed::fromInt(80), Fixed::fromInt(150)});
        ASSERT(pet.redraw(layer, scratch), "the Chestnut draws");
        ASSERT(golden(layer, "chestnut", 50, 92, 60, 60), "Chestnut: cap on its own live Body, stroked mouth");
    }

    if (update) printf("shape_paint_kit_test: goldens rewritten\n");
    printf("shape_paint_kit_test: %d passed, %d failed\n", passes, failures);
    return failures == 0 ? 0 : 1;
}
