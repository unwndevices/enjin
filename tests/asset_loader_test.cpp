/**
 * @file asset_loader_test.cpp
 * @brief Runtime .njn v2 / .njm loaders (ADR-0003 §6, Tomodachi #91).
 *
 * Proves the two #91 acceptance slices through the real Lua surface:
 *
 *   1. engine.tilemap.load(name) → map handle: parses a `.njm` map + pulls the
 *      tileset `.njn` v2 ATTR chunk inline, so the map's cells AND per-tile
 *      attributes are queryable. The handle is Lua-owned and needs no scene
 *      (Tomodachi #256): hosts draw it through a compositor restore source.
 *   2. engine.sprite.load(name) → handle over a `.njn` v2 container: the CLIP
 *      table decodes so C_Sprite:setClips(handle) + play(name) work, and the v1
 *      flat-header path still loads (back-compat kept).
 *
 * Fixtures are written to a private temp dir with NjnV2Writer (the same writer
 * the format's Python emitter mirrors), so the test needs no external assets.
 */
#include <enjin2/scripting/bindings.hpp>
#include <enjin2/scripting/lua_engine.hpp>
#include <enjin2/core/scene.hpp>
#include <enjin2/core/object.hpp>
#include <enjin2/graphics/njn2.hpp>
#include <enjin2/graphics/tilemap_asset.hpp>
#include <enjin2/graphics/sprite_asset.hpp>
#include <enjin2/graphics/canvas.hpp>
#include <enjin2/components/tilemap.hpp>
#include <enjin2/scripting/tilemap_lua.hpp>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

using namespace enjin2;

static int passes = 0;
static int failures = 0;

#define ASSERT(cond, msg)                                       \
    do {                                                        \
        if (!(cond)) {                                          \
            fprintf(stderr, "FAIL [line %d]: %s\n", __LINE__, (msg)); \
            failures++;                                         \
        } else {                                                \
            passes++;                                           \
        }                                                       \
    } while (0)

// ---------------------------------------------------------------------------
// Fixtures
// ---------------------------------------------------------------------------

struct MinimalScene : Scene {
    explicit MinimalScene(uint32_t id) : Scene(id) {}
};

struct Fixture {
    LuaEngine engine;
    LuaBindings bindings;
    MinimalScene scene;

    Fixture() : bindings(&engine), scene(1u) {
        engine.initialize();
        bindings.setFeatures(LuaFeatures::all());  // scene/camera/debug/raycast/proxies
        bindings.registerAll();
        scene.initialize();
        bindings.setActiveScene(&scene);
    }

    LuaResult exec(const char* code) { return engine.executeString(code); }
    double num(const char* name) { return engine.getGlobalNumber(name); }
};

// No scene at all: the #256 map handle must not need one.
struct SceneFreeFixture {
    LuaEngine engine;
    LuaBindings bindings;

    SceneFreeFixture() : bindings(&engine) {
        engine.initialize();
        bindings.registerAll();
    }

    LuaResult exec(const char* code) { return engine.executeString(code); }
    double num(const char* name) { return engine.getGlobalNumber(name); }
};

static void writeFile(const std::string& path, const std::vector<uint8_t>& bytes) {
    FILE* fp = fopen(path.c_str(), "wb");
    if (!fp) return;
    if (!bytes.empty()) fwrite(bytes.data(), 1, bytes.size(), fp);
    fclose(fp);
}

// A 4-tile 8x8 v2 tileset with an ATTR chunk (tile 1 SOLID kind 1, tile 2 ONEWAY).
static std::vector<uint8_t> makeTilesetNjn() {
    const uint8_t cw = 8, ch = 8, cols = 4, rows = 1;
    std::vector<uint8_t> pix(static_cast<size_t>(cw) * ch * cols * rows);
    for (size_t i = 0; i < pix.size(); ++i) pix[i] = static_cast<uint8_t>(i % 15);

    NjnTileAttr attrs[4] = {};
    attrs[1].flags = TileAttr::FLAG_SOLID;  attrs[1].kind = 1;
    attrs[2].flags = TileAttr::FLAG_ONEWAY; attrs[2].kind = 2;

    NjnV2Writer w;
    njn2WriteMeta(w, cw, ch, cols, rows);
    njn2WritePixl(w, pix.data(), static_cast<uint32_t>(pix.size()));
    njn2WriteAttr(w, attrs, 4);
    std::vector<uint8_t> out;
    w.finalise(out);
    return out;
}

// A 3x2 .njm map referencing tile ids 0..3.
static std::vector<uint8_t> makeMapNjm() {
    const uint8_t mw = 3, mh = 2;
    const uint16_t cells[6] = {
        tmPackCell(0, 0, 0, false, false), tmPackCell(1, 0, 0, false, false), tmPackCell(2, 0, 0, false, false),
        tmPackCell(3, 0, 0, false, false), tmPackCell(0, 0, 0, false, false), tmPackCell(1, 0, 0, false, false),
    };
    std::vector<uint8_t> out;
    out.push_back(NJM_MAGIC_0); out.push_back(NJM_MAGIC_1);
    out.push_back(NJM_VERSION); out.push_back(0);
    out.push_back(mw); out.push_back(mh);
    out.push_back(0); out.push_back(0);
    for (uint16_t c : cells) {
        out.push_back(static_cast<uint8_t>(c & 0xFF));
        out.push_back(static_cast<uint8_t>((c >> 8) & 0xFF));
    }
    return out;
}

// A 3-tile 16x16 tileset (one tile = one compositor tile): tile 1 is solid
// colour 5 (SOLID kind 3), tile 2 is solid colour 9 (no attrs).
static std::vector<uint8_t> makeRoomNjn() {
    const uint8_t cw = 16, ch = 16, cols = 3, rows = 1;
    std::vector<uint8_t> pix(static_cast<size_t>(cw) * ch * cols * rows);
    const uint8_t colours[3] = {15, 5, 9};
    for (size_t y = 0; y < ch; ++y) {
        for (size_t x = 0; x < static_cast<size_t>(cw) * cols; ++x) {
            pix[y * cw * cols + x] = colours[x / cw];
        }
    }

    NjnTileAttr attrs[3] = {};
    attrs[1].flags = TileAttr::FLAG_SOLID; attrs[1].kind = 3;

    NjnV2Writer w;
    njn2WriteMeta(w, cw, ch, cols, rows);
    njn2WritePixl(w, pix.data(), static_cast<uint32_t>(pix.size()));
    njn2WriteAttr(w, attrs, 3);
    std::vector<uint8_t> out;
    w.finalise(out);
    return out;
}

// A 4x2 room: a solid wall (tile 1) at column 2 of row 0, floor (tile 2)
// along row 1, everything else empty.
static std::vector<uint8_t> makeRoomNjm() {
    const uint8_t mw = 4, mh = 2;
    const uint16_t cells[8] = {
        0, 0, tmPackCell(1, 0, 0, false, false), 0,
        tmPackCell(2, 0, 0, false, false), tmPackCell(2, 0, 0, false, false),
        tmPackCell(2, 0, 0, false, false), tmPackCell(2, 0, 0, false, false),
    };
    std::vector<uint8_t> out;
    out.push_back(NJM_MAGIC_0); out.push_back(NJM_MAGIC_1);
    out.push_back(NJM_VERSION); out.push_back(0);
    out.push_back(mw); out.push_back(mh);
    out.push_back(0); out.push_back(0);
    for (uint16_t c : cells) {
        out.push_back(static_cast<uint8_t>(c & 0xFF));
        out.push_back(static_cast<uint8_t>((c >> 8) & 0xFF));
    }
    return out;
}

// A 3-frame 8x8 v2 animation sheet with a "walk" loop clip.
static std::vector<uint8_t> makeAnimNjn() {
    const uint8_t cw = 8, ch = 8, cols = 3, rows = 1;
    std::vector<uint8_t> pix(static_cast<size_t>(cw) * ch * cols * rows);
    for (size_t i = 0; i < pix.size(); ++i) pix[i] = static_cast<uint8_t>(i % 15);

    NjnClip clip{};
    std::strncpy(clip.name, "walk", sizeof(clip.name) - 1);
    clip.loopMode = NjnLoopMode::Loop;
    clip.frames = {
        NjnFrameEntry{0, 100, 0},
        NjnFrameEntry{1, 100, 7},   // frame 1 carries event 7
        NjnFrameEntry{2, 100, 0},
    };

    NjnV2Writer w;
    njn2WriteMeta(w, cw, ch, cols, rows);
    njn2WritePixl(w, pix.data(), static_cast<uint32_t>(pix.size()));
    njn2WriteClip(w, &clip, 1);
    std::vector<uint8_t> out;
    w.finalise(out);
    return out;
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

// engine.tilemap.load: .njm cells + inline v2 ATTR reach a live C_Tilemap.
static void test_tilemap_load(const std::string& dir) {
    printf("--- engine.tilemap.load (.njm + inline ATTR) ---\n");
    writeFile(dir + "/dungeon.njn", makeTilesetNjn());
    writeFile(dir + "/dungeon.njm", makeMapNjm());

    Fixture f;
    f.bindings.setAssetPath(dir);

    LuaResult r = f.exec(
        "tm = engine.tilemap.load('dungeon')\n"
        "ok = (tm ~= nil) and 1 or 0\n"
        "mw, mh = 0, 0\n"
        "c00, c10, c20, c01 = -1, -1, -1, -1\n"
        "solid_f, solid_k = -1, -1\n"
        "oneway_f = -1\n"
        "if tm ~= nil then\n"
        "  mw, mh = tm:getMapSize()\n"
        "  c00 = tm:getTile(0,0) & 0x1FF\n"
        "  c10 = tm:getTile(1,0) & 0x1FF\n"
        "  c20 = tm:getTile(2,0) & 0x1FF\n"
        "  c01 = tm:getTile(0,1) & 0x1FF\n"
        "  solid_f, solid_k = tm:attrAt(1,0)\n"   // cell (1,0) = tile 1 = SOLID kind 1
        "  oneway_f = (tm:attrAt(2,0))\n"          // cell (2,0) = tile 2 = ONEWAY
        "end\n"
    );
    ASSERT(r.success, r.success ? "tilemap.load script ran" : r.error.c_str());
    ASSERT(f.num("ok") == 1.0, "tilemap.load returned a proxy");
    ASSERT(f.num("mw") == 3.0, "map width is 3");
    ASSERT(f.num("mh") == 2.0, "map height is 2");
    ASSERT(f.num("c00") == 0.0, "cell (0,0) is tile 0");
    ASSERT(f.num("c10") == 1.0, "cell (1,0) is tile 1");
    ASSERT(f.num("c20") == 2.0, "cell (2,0) is tile 2");
    ASSERT(f.num("c01") == 3.0, "cell (0,1) is tile 3");
    ASSERT((static_cast<int>(f.num("solid_f")) & TileAttr::FLAG_SOLID) != 0,
           "inline ATTR: tile 1 is SOLID");
    ASSERT(f.num("solid_k") == 1.0, "inline ATTR: tile 1 kind is 1");
    ASSERT((static_cast<int>(f.num("oneway_f")) & TileAttr::FLAG_ONEWAY) != 0,
           "inline ATTR: tile 2 is ONEWAY");

    // The map is a Lua-owned handle, not a scene object (#256).
    ASSERT(f.scene.getObjects().empty(), "tilemap.load spawned no scene object");
}

// engine.tilemap.load with no scene at all (#256): the handle loads, answers
// collision queries, and draws the fixture's pixels.
static void test_tilemap_load_scene_free(const std::string& dir) {
    printf("--- engine.tilemap.load without a scene (#256) ---\n");
    writeFile(dir + "/room.njn", makeRoomNjn());
    writeFile(dir + "/room.njm", makeRoomNjm());

    SceneFreeFixture f;
    f.bindings.setAssetPath(dir);

    LuaResult r = f.exec(
        "map = engine.tilemap.load('room')\n"
        "kind = type(map)\n"
        "mw, mh = map:getMapSize()\n"
        "wall_f, wall_k = map:attrAt(2, 0)\n"
        "floor_f = (map:attrAt(0, 1))\n"
        // A 4x4 box in row 0 slides right into the wall at x = 32.
        "sx, sy, st, snx, sny, shit = map:sweepAabb(8, 4, 4, 4, 40, 0, 1)\n"
        "shit = shit and 1 or 0\n"
    );
    ASSERT(r.success, r.success ? "scene-free tilemap.load ran" : r.error.c_str());
    ASSERT(f.num("mw") == 4.0 && f.num("mh") == 2.0, "room is 4x2");
    ASSERT((static_cast<int>(f.num("wall_f")) & TileAttr::FLAG_SOLID) != 0,
           "attrAt: the wall tile is SOLID");
    ASSERT(f.num("wall_k") == 3.0, "attrAt: the wall's kind is 3");
    ASSERT(f.num("floor_f") == 0.0, "attrAt: the floor tile has no flags");
    ASSERT(f.num("shit") == 1.0, "sweepAabb hits the wall");
    ASSERT(f.num("sx") == 28.0, "sweepAabb stops the box flush with the wall");
    ASSERT(f.num("sy") == 4.0, "sweepAabb keeps y");
    ASSERT(f.num("snx") == -1.0, "sweepAabb normal points away from the wall");

    // The handle is what a host's restore source draws (gfx.setTilemap).
    lua_State* L = f.engine.getState();
    lua_getglobal(L, "map");
    const C_Tilemap* tm = lua::toTilemap(L, -1);
    lua_pop(L, 1);
    ASSERT(tm != nullptr, "the handle resolves to a tilemap");
    if (tm) {
        Canvas4<64, 32> canvas;
        for (uint16_t ty = 0; ty < 2; ++ty)
            for (uint16_t tx = 0; tx < 4; ++tx)
                tm->drawCellBand(canvas, tx, ty, C_Tilemap::Band::Under, Pixel4(0));
        ASSERT(canvas.getPixel(0, 0).value == 0, "empty cell is the clear colour");
        ASSERT(canvas.getPixel(32, 0).value == 5, "wall cell draws tile 1");
        ASSERT(canvas.getPixel(47, 15).value == 5, "wall fills its 16x16 cell");
        ASSERT(canvas.getPixel(48, 0).value == 0, "the cell after the wall is empty");
        ASSERT(canvas.getPixel(0, 16).value == 9, "floor cells draw tile 2");
        ASSERT(canvas.getPixel(63, 31).value == 9, "the last floor cell draws tile 2");
    }

    // Dropping the last reference collects the handle safely.
    LuaResult gc = f.exec("map = nil collectgarbage() collectgarbage() ok_gc = 1");
    ASSERT(gc.success && f.num("ok_gc") == 1.0, "a collected handle is safe");

    // Other values are not tilemaps.
    lua_pushinteger(L, 3);
    ASSERT(lua::toTilemap(L, -1) == nullptr, "an integer is not a tilemap");
    lua_pop(L, 1);
}

// engine.sprite.load: v2 container exposes PIXL + CLIP through C_Sprite.
static void test_sprite_load_v2(const std::string& dir) {
    printf("--- engine.sprite.load v2 (PIXL + CLIP) ---\n");
    writeFile(dir + "/hero.njn", makeAnimNjn());

    Fixture f;
    f.bindings.setAssetPath(dir);

    LuaResult r = f.exec(
        "h = engine.sprite.load('hero')\n"
        "okh = (h ~= nil and h >= 0) and 1 or 0\n"
        "o = engine.scene.spawn('hero')\n"
        "spr = o:add('C_Sprite')\n"
        "spr:setSheet(h)\n"
        "spr:setClips(h)\n"
        "played = spr:play('walk') and 1 or 0\n"
        "clip_ok = (spr.clip == 'walk') and 1 or 0\n"
        "frame0 = spr.frame\n"
    );
    ASSERT(r.success, r.success ? "sprite.load script ran" : r.error.c_str());
    ASSERT(f.num("okh") == 1.0, "sprite.load returned a valid handle");
    ASSERT(f.num("played") == 1.0, "clip 'walk' decoded from CLIP and plays");
    ASSERT(f.num("clip_ok") == 1.0, "playing clip is 'walk'");
    ASSERT(f.num("frame0") == 0.0, "walk starts on frame 0");
}

// The v1 flat-header path still loads (back-compat, ADR-0003 §6 "keep v1").
static void test_sprite_load_v1_back_compat(const std::string& dir) {
    printf("--- engine.sprite.load v1 back-compat ---\n");
    // Hand-build a v1 .njn: 8-byte flat header + pixels.
    const uint8_t cw = 4, ch = 4, cols = 2, rows = 1;
    std::vector<uint8_t> v1;
    v1.push_back(NJN_MAGIC_0); v1.push_back(NJN_MAGIC_1);
    v1.push_back(NJN_VERSION); v1.push_back(cw);
    v1.push_back(ch); v1.push_back(cols); v1.push_back(rows); v1.push_back(0);
    for (int i = 0; i < cw * ch * cols * rows; ++i) v1.push_back(static_cast<uint8_t>(i % 15));
    writeFile(dir + "/legacy.njn", v1);

    Fixture f;
    f.bindings.setAssetPath(dir);
    LuaResult r = f.exec(
        "h = engine.sprite.load('legacy')\n"
        "okh = (h ~= nil and h >= 0) and 1 or 0\n"
    );
    ASSERT(r.success, r.success ? "v1 sprite.load ran" : r.error.c_str());
    ASSERT(f.num("okh") == 1.0, "v1 flat-header .njn still loads");
}

int main() {
    printf("=== Runtime .njn v2 / .njm loaders (#91) ===\n");

    char tmpl[] = "/tmp/enjin_njn_XXXXXX";
    const char* dir = mkdtemp(tmpl);
    if (!dir) {
        fprintf(stderr, "FAIL: could not create temp dir\n");
        return 1;
    }
    const std::string d(dir);

    test_tilemap_load(d);
    test_tilemap_load_scene_free(d);
    test_sprite_load_v2(d);
    test_sprite_load_v1_back_compat(d);

    printf("\n%d passed, %d failed\n", passes, failures);
    return failures == 0 ? 0 : 1;
}
