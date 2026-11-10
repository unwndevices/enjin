// The palette contract's seal (ADR-0022, Tomodachi #426): indices 8-14 are not
// Palette roles, and every way in refuses them with a clear message.
//
//   SEAL-01  gfx.* raise a Lua error naming the bad colour (9), for each kind
//            of colour argument: a draw colour, a pixel, an optional colour,
//            a UI colour, a style table key, a remap index and a raw remap
//            table entry. 0..7 and 15 still draw.
//   SEAL-02  engine.sprite.load / engine.tilemap.load refuse a .njn sheet with
//            a pixel 9 (one pass at load); the same sheet in roles loads.
//   SEAL-03  LayeredAssetStore refuses a layered .njn with a pixel 9 and names
//            it; the decoder itself stays permissive (Colour replace reads it).
//   SEAL-04  The palette holds the 8 roles only: 8-14 resolve to black (unused
//            LUT slots) and setColor ignores them; isPaletteIndex is the test.
#include <enjin2/graphics/asset_arena.hpp>
#include <enjin2/graphics/blit_rgb565.hpp>
#include <enjin2/graphics/layer_compositor.hpp>
#include <enjin2/graphics/layered_asset.hpp>
#include <enjin2/graphics/njn2.hpp>
#include <enjin2/graphics/palette.hpp>
#include <enjin2/scripting/bindings.hpp>
#include <enjin2/scripting/lua_engine.hpp>

#include <cstdio>
#include <string>
#include <vector>

using namespace enjin2;

static int passes = 0;
static int failures = 0;

#define ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            fprintf(stderr, "FAIL: %s\n", std::string(msg).c_str()); \
            failures++; \
        } else { \
            passes++; \
        } \
    } while (0)

static bool contains(const std::string& s, const char* part) { return s.find(part) != std::string::npos; }

// LuaEngine + LuaBindings drawing onto one 32x32 compositor layer.
struct DrawFixture {
    LayerCompositor<32, 32> compositor;
    LuaEngine engine;
    LuaBindings bindings;
    LuaCanvas layer0;
    LuaCanvas* layerPtrs[1];

    DrawFixture() : bindings(&engine), layer0(&compositor.layers[0]) {
        layerPtrs[0] = &layer0;
        engine.initialize();
        bindings.registerAll();
        bindings.setLayers(layerPtrs, 1, compositor.visible);
        compositor.clearAll();
    }

    LuaResult exec(const std::string& code) { return engine.executeString(code.c_str()); }
    uint8_t px(int16_t x, int16_t y) { return compositor.layers[0].getPixel(x, y).value; }
};

// ---------------------------------------------------------------------------
// SEAL-01: gfx.* refuse 8-14, naming the value
// ---------------------------------------------------------------------------
static void test_SEAL_01_lua_colours() {
    printf("--- SEAL-01: gfx.* raise on 8-14 ---\n");
    DrawFixture f;
    const char* calls[] = {
        "gfx.setColor(9)",
        "gfx.clear(9)",
        "gfx.setPixel(1, 1, 9)",
        "gfx.fastFillRect(0, 0, 2, 2, 9)",
        "gfx.getPaletteColor(9)",
        "gfx.setTextOutline(true, 9)",
        "engine.ui.panel(0, 0, 4, 4, 9, 2)",
        "engine.ui.setStyle('panel', { fill = 9 })",
        "gfx.remap('solid', 9)",
        "gfx.remap('recolor', 1, 9)",
        "gfx.remap({0, 1, 2, 3, 4, 5, 6, 9, 8, 9, 10, 11, 12, 13, 14, 15})",
    };
    for (const char* call : calls) {
        const LuaResult r = f.exec(call);
        ASSERT(!r.success, std::string("SEAL-01 refused: ") + call);
        ASSERT(contains(r.error, "9 is not a Palette role (0..7) or transparent (15)"),
               std::string("SEAL-01 the message names 9 and the roles: ") + call + " -> " + r.error);
    }
    // 14 too (the old ramp's lightest), and 16 (not an index at all).
    ASSERT(!f.exec("gfx.setColor(14)").success, "SEAL-01 14 refused");
    ASSERT(!f.exec("gfx.setPixel(0, 0, 16)").success, "SEAL-01 16 refused");

    // A refused colour changes nothing: the draw colour is what it was.
    ASSERT(f.exec("gfx.setColor(gfx.COLOR.ACCENT) pcall(gfx.setColor, 12) c = gfx.getColor()").success,
           "SEAL-01 a refused setColor runs under pcall");
    ASSERT(f.exec("assert(c == gfx.COLOR.ACCENT)").success, "SEAL-01 a refused setColor keeps the draw colour");

    // The roles and transparent still draw.
    ASSERT(f.exec("gfx.setPixel(2, 2, 7) gfx.setPixel(3, 3, 15) gfx.setPixel(4, 4, 0)").success,
           "SEAL-01 roles 0..7 and 15 draw");
    ASSERT(f.px(2, 2) == 7 && f.px(3, 3) == 15 && f.px(4, 4) == 0, "SEAL-01 role pixels land");
    // A raw remap table's entries for 8..14 are not read (no role maps there).
    ASSERT(f.exec("gfx.remap({0, 1, 2, 3, 4, 5, 6, 7, 'x', nil, nil, nil, nil, nil, nil, 15})").success,
           "SEAL-01 remap table: the 8..14 entries are ignored");
}

// ---------------------------------------------------------------------------
// SEAL-02: the sheet loader refuses a pixel 9
// ---------------------------------------------------------------------------
static std::vector<uint8_t> sheetNjn(uint8_t pixel) {
    const uint8_t pixels[4] = {0, 3, pixel, 15};
    NjnV2Writer w;
    njn2WriteMeta(w, 2, 2, 1, 1);
    njn2WritePixl(w, pixels, sizeof(pixels));
    std::vector<uint8_t> out;
    w.finalise(out);
    return out;
}

static void writeFile(const std::string& path, const std::vector<uint8_t>& bytes) {
    FILE* fp = fopen(path.c_str(), "wb");
    if (!fp) return;
    fwrite(bytes.data(), 1, bytes.size(), fp);
    fclose(fp);
}

static void test_SEAL_02_sheet_loader() {
    printf("--- SEAL-02: engine.sprite.load / tilemap.load refuse 8-14 ---\n");
    writeFile("seal_nine.njn", sheetNjn(9));
    writeFile("seal_roles.njn", sheetNjn(7));
    DrawFixture f;
    f.bindings.setAssetPath(".");
    const uint32_t before = f.bindings.spriteLoadFailures();
    ASSERT(f.exec("bad = engine.sprite.load('seal_nine')").success, "SEAL-02 load runs");
    ASSERT(f.exec("assert(bad == -1)").success, "SEAL-02 a sheet with pixel 9 is refused (-1)");
    ASSERT(f.bindings.spriteLoadFailures() == before + 1, "SEAL-02 the refusal is counted");
    ASSERT(f.exec("ok = engine.sprite.load('seal_roles') assert(ok >= 0)").success,
           "SEAL-02 the same sheet in roles loads");
    const LuaResult t = f.exec("engine.tilemap.load('seal_nine')");
    ASSERT(!t.success && contains(t.error, "pixel index 9 is not a Palette role"),
           "SEAL-02 a tileset with pixel 9 is refused, named: " + t.error);
}

// ---------------------------------------------------------------------------
// SEAL-03: LayeredAssetStore refuses a pixel 9
// ---------------------------------------------------------------------------
static NjnLayered layered(uint8_t pixel) {
    NjnLayered a;
    a.canvasW = 2;
    a.canvasH = 2;
    NjnPartImage img;
    img.w = 2;
    img.h = 2;
    img.pixels = {3, pixel, 15, 0};
    a.images.push_back(img);
    NjnPart part{};
    std::snprintf(part.name, sizeof(part.name), "body");
    a.parts.push_back(part);
    a.refs.push_back(NjnFramePartRef{0, 0, 0});
    a.durations.push_back(100);
    return a;
}

static std::vector<uint8_t> layeredNjn(uint8_t pixel) {
    NjnV2Writer w;
    njn2WriteLayered(w, layered(pixel));
    std::vector<uint8_t> out;
    w.finalise(out);
    return out;
}

static void test_SEAL_03_layered_loader() {
    printf("--- SEAL-03: LayeredAssetStore refuses 8-14 ---\n");
    AssetArena arena;
    ASSERT(arena.allocateBacking(16 * 1024), "SEAL-03 backing allocated");
    LayeredAssetStore store(arena);

    const std::vector<uint8_t> nine = layeredNjn(9);
    std::string err;
    const auto h = store.loadFromMemory(nine.data(), nine.size(), &err);
    ASSERT(h == LayeredAssetStore::INVALID_HANDLE, "SEAL-03 a layered asset with pixel 9 is refused");
    ASSERT(err == "pixel index 9 is not a Palette role (0..7) or transparent (15)",
           "SEAL-03 the error names pixel index 9: " + err);
    ASSERT(store.assetCount() == 0 && arena.used() == 0, "SEAL-03 nothing committed");

    NjnV2Reader r;
    NjnLayered decoded;
    ASSERT(r.open(nine.data(), nine.size()) && njn2DecodeLayered(r, decoded),
           "SEAL-03 the decoder still reads it (Colour replace moves old art onto roles)");

    const std::vector<uint8_t> roles = layeredNjn(7);
    err.clear();
    ASSERT(store.loadFromMemory(roles.data(), roles.size(), &err) != LayeredAssetStore::INVALID_HANDLE,
           "SEAL-03 the same asset in roles loads: " + err);
}

// ---------------------------------------------------------------------------
// SEAL-04: the palette is the 8 roles
// ---------------------------------------------------------------------------
static void test_SEAL_04_palette() {
    printf("--- SEAL-04: the palette holds the 8 roles ---\n");
    for (unsigned v = 0; v < 256; ++v) {
        const bool want = v < ROLE_COUNT || v == PALETTE_TRANSPARENT;
        if (isPaletteIndex(v) != want) ASSERT(false, "SEAL-04 isPaletteIndex(" + std::to_string(v) + ")");
    }
    ASSERT(sizeof(Palette::colors) / sizeof(RGB) == ROLE_COUNT, "SEAL-04 the palette stores 8 colours");
    Palette p;
    const RGB text = p.getColor(ROLE_TEXT);
    p.setColor(9, 1, 2, 3);
    ASSERT(p.getColor(ROLE_TEXT) == text, "SEAL-04 setColor(9) touches no role");
    for (uint8_t i = ROLE_COUNT; i < PALETTE_TRANSPARENT; ++i) {
        ASSERT(p.getColor(i) == RGB(0, 0, 0) && p.resolve(i) == RGB(0, 0, 0), "SEAL-04 8-14 resolve to black");
    }
    uint16_t lut[16];
    makeRgb565Lut(p, lut, false);
    // On screen 15 and the unused 8-14 show bg, on the device as in the
    // Simulator (owner, #416): a layer left transparent is the empty screen.
    const RGB bg = p.getColor(ROLE_BG);
    for (uint8_t i = ROLE_COUNT; i < 16; ++i)
        ASSERT(lut[i] == packRgb565(bg.r, bg.g, bg.b), "SEAL-04 LUT slots 8-15 show bg");
    ASSERT(lut[ROLE_TEXT] == packRgb565(text.r, text.g, text.b), "SEAL-04 LUT roles keep their colours");
}

int main() {
    test_SEAL_01_lua_colours();
    test_SEAL_02_sheet_loader();
    test_SEAL_03_layered_loader();
    test_SEAL_04_palette();
    printf("\npalette_seal_test: %d passed, %d failed\n", passes, failures);
    return failures == 0 ? 0 : 1;
}
