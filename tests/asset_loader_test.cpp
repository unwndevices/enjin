/**
 * @file asset_loader_test.cpp
 * @brief Runtime .njn v2 / .njm loaders (ADR-0003 §6, Tomodachi #91).
 *
 * Proves the two #91 acceptance slices through the real Lua surface:
 *
 *   1. engine.tilemap.load(name) → C_Tilemap: parses a `.njm` map + pulls the
 *      tileset `.njn` v2 ATTR chunk inline, so the map's cells AND per-tile
 *      attributes are queryable and the tilemap is a live scene drawable.
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
        bindings.registerAll();
        scene.initialize();
        bindings.setActiveScene(&scene);
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

    // The map object is a live drawable in the scene (it renders next frame).
    ASSERT(f.scene.getObjects().size() >= 1, "tilemap.load spawned a scene object");
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
    test_sprite_load_v2(d);
    test_sprite_load_v1_back_compat(d);

    printf("\n%d passed, %d failed\n", passes, failures);
    return failures == 0 ? 0 : 1;
}
