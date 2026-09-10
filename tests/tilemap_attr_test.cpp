/**
 * @file tilemap_attr_test.cpp
 * @brief Tests for tile attributes + collision query API (issue #79)
 *
 * Test IDs:
 *   TMATTR-01  TileAttr flag/kind helpers + tmResolveDir (hflip 1↔3, vflip 0↔2)
 *   TMATTR-02  attrAt returns transform-resolved DIR; out-of-bounds → passable
 *   TMATTR-03  attrAtPixel maps map-pixel coords to grid
 *   TMATTR-04  sweepAabb slides along a SOLID wall (axis-separated)
 *   TMATTR-05  sweepCircle slides along a SOLID wall
 *   TMATTR-06  one-way platform blocks from above, passes from below
 *   TMATTR-07  forEachCellIn iterates overlapped cells
 *   TMATTR-08  buildSolidRects merges contiguous SOLID tiles
 *   TMATTR-09  hflip/vflip render (flip-aware tile draw)
 *   TMATTR-10  palbank render (per-tile Remap)
 *   TMATTR-11  raycast semantic: SOLID bit (attr-driven), not tid != 0
 */

#include <enjin2/core/object.hpp>
#include <enjin2/components/tilemap.hpp>
#include <enjin2/components/position.hpp>
#include <enjin2/graphics/canvas.hpp>
#include <enjin2/graphics/sprite.hpp>
#include <enjin2/graphics/remap.hpp>
#include <cstdio>
#include <cstring>
#include <vector>

using namespace enjin2;

static int passes   = 0;
static int failures = 0;

#define ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            fprintf(stderr, "FAIL: %s\n", msg); \
            failures++; \
        } else { \
            passes++; \
        } \
    } while(0)

// ---------------------------------------------------------------------------
// Tileset: 16x16 cells, frames indexed by tile id (1 = solid, 2 = passable,
// 3 = one-way). All pixels default to index 5; tests override per-frame.
// ---------------------------------------------------------------------------
static constexpr uint8_t TILE_W    = 16;
static constexpr uint8_t TILE_H    = 16;
static constexpr uint8_t TILE_COLS = 4;
static constexpr uint8_t TILE_ROWS = 1;

static uint8_t g_tileData[TILE_COLS * TILE_W * TILE_H];

// Mark frame 1 with a single opaque pixel at visual (x=2, y=3), i.e. row 3,
// col 2, in a sheet of otherwise-transparent (index 15) pixels.
static void markFrame1Pixel() {
    memset(g_tileData, 15, sizeof(g_tileData));
    g_tileData[1 * TILE_W * TILE_H + 3 * TILE_W + 2] = 5;
}

static void initTileData() {
    markFrame1Pixel();
}

static SpriteSheet makeSheet() {
    return SpriteSheet(g_tileData, TILE_W, TILE_H, TILE_COLS, TILE_ROWS);
}

// Build a 4x4 map: col 0 = tile 0 (passable), col 1 = tile 1 (SOLID),
// col 2 = tile 2 (passable), col 3 = tile 0.
static uint16_t g_map4x4[16];
static uint16_t g_zeroMap[16];   // all-transparent 4x4 (for setTile plumbing)

static void initMap() {
    for (int ty = 0; ty < 4; ++ty) {
        g_map4x4[ty * 4 + 0] = C_Tilemap::packCell(0, 0, 0, false, false); // transparent/passable
        g_map4x4[ty * 4 + 1] = C_Tilemap::packCell(1, 0, 0, false, false); // SOLID
        g_map4x4[ty * 4 + 2] = C_Tilemap::packCell(2, 0, 0, false, false); // passable tile id
        g_map4x4[ty * 4 + 3] = C_Tilemap::packCell(0, 0, 0, false, false);
    }
    for (int i = 0; i < 16; ++i) g_zeroMap[i] = 0;
}

// attrs: tile 1 = SOLID, tile 2 = passable (kind 42), tile 3 = ONEWAY dir Up.
static TileAttr g_attrs[4];

static void initAttrs() {
    g_attrs[0] = TileAttr{};                       // transparent — no attr
    g_attrs[1] = TileAttr{TileAttr::FLAG_SOLID, 0};
    g_attrs[2] = TileAttr{0, 42};
    g_attrs[3] = TileAttr{TileAttr::FLAG_ONEWAY | (static_cast<uint8_t>(TileDir::Up) << TileAttr::DIR_SHIFT), 0};
}

static C_Tilemap* makeTilemap(Object* obj, uint8_t mapW, uint8_t mapH,
                              const uint16_t* cells) {
    obj->addComponent<C_Position>();
    C_Tilemap* tm = obj->addComponent<C_Tilemap>();
    tm->setSheet(makeSheet());
    if (cells) tm->setTiles(cells, mapW, mapH);
    tm->setAttrs(g_attrs, 4);
    return tm;
}

// ---------------------------------------------------------------------------
// TMATTR-01: flag helpers + DIR resolution
// ---------------------------------------------------------------------------
static void test_tmattr01_resolve_dir() {
    printf("--- TMATTR-01: TileAttr helpers + tmResolveDir ---\n");

    TileAttr solid{ TileAttr::FLAG_SOLID, 7 };
    ASSERT(solid.solid(),   "TMATTR-01: solid() true");
    ASSERT(!solid.oneway(), "TMATTR-01: oneway() false");
    ASSERT(solid.kind == 7, "TMATTR-01: kind preserved");

    TileAttr ow{ static_cast<uint8_t>(TileAttr::FLAG_ONEWAY | (3 << TileAttr::DIR_SHIFT)), 0 };
    ASSERT(ow.oneway(), "TMATTR-01: oneway() true");
    ASSERT(ow.dir() == 3, "TMATTR-01: dir() == 3");

    // hflip swaps Right(1)<->Left(3)
    ASSERT(tmResolveDir(1, true, false) == 3, "TMATTR-01: hflip 1->3");
    ASSERT(tmResolveDir(3, true, false) == 1, "TMATTR-01: hflip 3->1");
    // vflip swaps Up(0)<->Down(2)
    ASSERT(tmResolveDir(0, false, true) == 2, "TMATTR-01: vflip 0->2");
    ASSERT(tmResolveDir(2, false, true) == 0, "TMATTR-01: vflip 2->0");
    // no flip → identity
    ASSERT(tmResolveDir(1, false, false) == 1, "TMATTR-01: no flip identity");
}

// ---------------------------------------------------------------------------
// TMATTR-02: attrAt + transform resolution + OOB
// ---------------------------------------------------------------------------
static void test_tmattr02_attr_at() {
    printf("--- TMATTR-02: attrAt transform-resolved ---\n");

    Object* obj = new Object();
    C_Tilemap* tm = makeTilemap(obj, 4, 4, g_map4x4);

    // (1,0) = tile 1 = SOLID, no flip → dir 0
    TileAttr a = tm->attrAt(1, 0);
    ASSERT(a.solid(), "TMATTR-02: (1,0) solid");
    ASSERT(a.kind == 0, "TMATTR-02: (1,0) kind 0");

    // (2,0) = tile 2 = passable, kind 42
    TileAttr b = tm->attrAt(2, 0);
    ASSERT(!b.solid() && b.kind == 42, "TMATTR-02: (2,0) passable kind 42");

    // Out-of-bounds → default passable
    TileAttr oob = tm->attrAt(99, 99);
    ASSERT(!oob.solid() && oob.kind == 0, "TMATTR-02: OOB passable default");

    delete obj;
}

// ---------------------------------------------------------------------------
// TMATTR-02b: DIR is flip-resolved at fetch (cell hflip 1->3, vflip 0->2)
// ---------------------------------------------------------------------------
static void test_tmattr02b_dir_flip_resolve() {
    printf("--- TMATTR-02b: DIR flip-resolved at fetch ---\n");

    // Re-key tile id 1 as DIR=Right(1) for this test.
    g_attrs[1] = TileAttr{ static_cast<uint8_t>(TileAttr::FLAG_SOLID | (1 << TileAttr::DIR_SHIFT)), 0 };

    Object* obj = new Object();
    obj->addComponent<C_Position>();
    C_Tilemap* tm = obj->addComponent<C_Tilemap>();
    tm->setSheet(makeSheet());
    tm->setTiles(g_zeroMap, 4, 4);
    tm->setAttrs(g_attrs, 4);

    // cell (0,0) = tile 1, hflip → dir should resolve 1 -> 3
    tm->setTile(0, 0, C_Tilemap::packCell(1, 0, 0, /*hflip*/true, false));
    ASSERT(tm->attrAt(0, 0).dir() == 3, "TMATTR-02b: hflipped DIR 1->3");

    // cell (0,1) = tile 1, vflip → dir should be unchanged by vflip (1 is E/W)
    tm->setTile(0, 1, C_Tilemap::packCell(1, 0, 0, false, /*vflip*/true));
    ASSERT(tm->attrAt(0, 1).dir() == 1, "TMATTR-02b: vflip leaves E/W DIR");

    // Now Up(0) on tile 3 + vflip → 2
    g_attrs[3] = TileAttr{ static_cast<uint8_t>(TileAttr::FLAG_ONEWAY | (0 << TileAttr::DIR_SHIFT)), 0 };
    tm->setAttrs(g_attrs, 4);
    tm->setTile(0, 2, C_Tilemap::packCell(3, 0, 0, false, /*vflip*/true));
    ASSERT(tm->attrAt(0, 2).dir() == 2, "TMATTR-02b: vflipped DIR 0->2");

    delete obj;

    // restore the standard attrs for later tests
    initAttrs();
}

// ---------------------------------------------------------------------------
// TMATTR-03: attrAtPixel
// ---------------------------------------------------------------------------
static void test_tmattr03_attr_at_pixel() {
    printf("--- TMATTR-03: attrAtPixel ---\n");

    Object* obj = new Object();
    C_Tilemap* tm = makeTilemap(obj, 4, 4, g_map4x4);

    // (16..32) px maps to col 1 → SOLID
    TileAttr a = tm->attrAtPixel(20, 8);
    ASSERT(a.solid(), "TMATTR-03: attrAtPixel(20,8) solid (col 1)");

    // col 2 is passable
    TileAttr b = tm->attrAtPixel(40, 0);
    ASSERT(!b.solid() && b.kind == 42, "TMATTR-03: attrAtPixel(40,0) passable");

    // Negative map px → floor division → OOB → passable
    TileAttr c = tm->attrAtPixel(-100, -100);
    ASSERT(!c.solid(), "TMATTR-03: negative map px passable");

    delete obj;
}

// ---------------------------------------------------------------------------
// TMATTR-04: sweepAabb slides along a SOLID wall
// ---------------------------------------------------------------------------
static void test_tmattr04_sweep_aabb() {
    printf("--- TMATTR-04: sweepAabb slides along SOLID wall ---\n");

    Object* obj = new Object();
    C_Tilemap* tm = makeTilemap(obj, 4, 4, g_map4x4);

    // Body 8x8 at (0,0), moving diagonally into the wall (col 1, left face x=16).
    SweepResult r = tm->sweepAabb(0, 0, 8, 8, 100.0f, 100.0f, 1.0f);
    // X blocked: x+w touches 16 → x = 8. Y slides fully → y = 100.
    ASSERT(r.hit, "TMATTR-04: hit");
    ASSERT(r.x == 8.0f, "TMATTR-04: X clamped to wall (x == 8)");
    ASSERT(r.y == 100.0f, "TMATTR-04: Y slides fully (y == 100)");
    ASSERT(r.normalX == -1.0f, "TMATTR-04: normalX == -1 (pushed left)");
    ASSERT(r.normalY == 0.0f, "TMATTR-04: normalY == 0");

    delete obj;
}

// ---------------------------------------------------------------------------
// TMATTR-05: sweepCircle slides along a SOLID wall
// ---------------------------------------------------------------------------
static void test_tmattr05_sweep_circle() {
    printf("--- TMATTR-05: sweepCircle slides along SOLID wall ---\n");

    Object* obj = new Object();
    C_Tilemap* tm = makeTilemap(obj, 4, 4, g_map4x4);

    // Circle radius 4 centred at (4,8) (AABB [0,8,8,8]), moving +X into the wall.
    SweepResult r = tm->sweepCircle(4, 8, 4, 100.0f, 0.0f, 1.0f);
    ASSERT(r.hit, "TMATTR-05: hit");
    // AABB x clamps to 8 → centre cx = 8 + 4 = 12
    ASSERT(r.x == 12.0f, "TMATTR-05: centre cx == 12");
    ASSERT(r.y == 8.0f, "TMATTR-05: centre cy unchanged (no Y motion)");
    ASSERT(r.normalX == -1.0f, "TMATTR-05: normalX == -1");

    delete obj;
}

// ---------------------------------------------------------------------------
// TMATTR-06: one-way platform — blocks from above, passes from below
// ---------------------------------------------------------------------------
static void test_tmattr06_oneway() {
    printf("--- TMATTR-06: one-way blocks from above, passes from below ---\n");

    // Build a map where row 2 is a one-way platform (tile 3 = ONEWAY dir Up).
    static uint16_t onewayMap[16];
    for (int ty = 0; ty < 4; ++ty) {
        for (int tx = 0; tx < 4; ++tx) {
            uint16_t cell = (ty == 2) ? C_Tilemap::packCell(3, 0, 0, false, false)
                                      : C_Tilemap::packCell(2, 0, 0, false, false);
            onewayMap[ty * 4 + tx] = cell;
        }
    }
    g_attrs[3] = TileAttr{ static_cast<uint8_t>(TileAttr::FLAG_ONEWAY | (static_cast<uint8_t>(TileDir::Up) << TileAttr::DIR_SHIFT)), 0 };

    Object* obj = new Object();
    obj->addComponent<C_Position>();
    C_Tilemap* tm = obj->addComponent<C_Tilemap>();
    tm->setSheet(makeSheet());
    tm->setTiles(onewayMap, 4, 4);
    tm->setAttrs(g_attrs, 4);

    // Platform top is at y=32 (row 2). Falling circle (radius 4) from above:
    // bottom must land at y=32 → centre cy = 28.
    SweepResult fall = tm->sweepCircle(8, 12, 4, 0.0f, 100.0f, 0.5f);   // dy = 50
    ASSERT(fall.hit, "TMATTR-06: falling circle hits the platform");
    ASSERT(fall.y == 28.0f, "TMATTR-06: lands with centre cy == 28");
    ASSERT(fall.normalY == -1.0f, "TMATTR-06: normalY == -1 (pushed up)");

    // Rising circle from below passes straight through.
    SweepResult jump = tm->sweepCircle(8, 60, 4, 0.0f, -100.0f, 0.5f);  // dy = -50
    ASSERT(!jump.hit, "TMATTR-06: rising circle passes through from below");
    ASSERT(jump.y == 10.0f, "TMATTR-06: rising circle reaches cy == 10");

    delete obj;
}

// ---------------------------------------------------------------------------
// TMATTR-07: forEachCellIn
// ---------------------------------------------------------------------------
static void test_tmattr07_for_each_cell() {
    printf("--- TMATTR-07: forEachCellIn ---\n");

    Object* obj = new Object();
    C_Tilemap* tm = makeTilemap(obj, 4, 4, g_map4x4);

    std::vector<int> cells;
    // AABB [0,40) x [0,16) overlaps cols 0,1,2 (row 0)
    tm->forEachCellIn(0, 0, 40.0f, 16.0f, [&](uint8_t tx, uint8_t ty, uint16_t cell) {
        cells.push_back(tx); (void)ty; (void)cell;
    });
    ASSERT(cells.size() == 3, "TMATTR-07: 3 cells overlap [0,40)x[0,16)");
    ASSERT(cells[0] == 0 && cells[1] == 1 && cells[2] == 2,
           "TMATTR-07: cells are cols 0,1,2");

    delete obj;
}

// ---------------------------------------------------------------------------
// TMATTR-08: buildSolidRects merges contiguous SOLID tiles
// ---------------------------------------------------------------------------
static void test_tmattr08_build_solid_rects() {
    printf("--- TMATTR-08: buildSolidRects ---\n");

    Object* obj = new Object();
    C_Tilemap* tm = makeTilemap(obj, 4, 4, g_map4x4);

    // col 1 is the only SOLID column, all 4 rows → one 16x64 rect at x=16.
    std::vector<Rect> rects = tm->buildSolidRects();
    ASSERT(rects.size() == 1, "TMATTR-08: one merged rect");
    ASSERT(rects[0].x == 16 && rects[0].y == 0 &&
           rects[0].width == 16 && rects[0].height == 64,
           "TMATTR-08: rect = {x=16,y=0,w=16,h=64}");

    delete obj;
}

// ---------------------------------------------------------------------------
// TMATTR-09: hflip / vflip render
// ---------------------------------------------------------------------------
static void test_tmattr09_flip_render() {
    printf("--- TMATTR-09: flip render ---\n");

    // frame 1 has a single marked pixel at source (2,3). Restore that.
    markFrame1Pixel();

    Object* obj = new Object();
    obj->addComponent<C_Position>();
    C_Tilemap* tm = obj->addComponent<C_Tilemap>();
    tm->setSheet(makeSheet());
    tm->setTiles(g_zeroMap, 1, 1);

    Canvas4<16, 16> canvas;

    // No flip: pixel at (2,3)
    tm->setTile(0, 0, C_Tilemap::packCell(1, 0, 0, false, false));
    canvas.clear(Pixel4(0));
    tm->draw(canvas);
    ASSERT(canvas.getPixel(2, 3).value == 5, "TMATTR-09: no flip, pixel at (2,3)");

    // hflip: source (2,3) → (15-2, 3) = (13,3)
    tm->setTile(0, 0, C_Tilemap::packCell(1, 0, 0, true, false));
    canvas.clear(Pixel4(0));
    tm->draw(canvas);
    ASSERT(canvas.getPixel(13, 3).value == 5, "TMATTR-09: hflip draws at (13,3)");
    ASSERT(canvas.getPixel(2, 3).value == 0, "TMATTR-09: hflip does not draw at (2,3)");

    // vflip: source (2,3) → (2, 15-3) = (2,12)
    tm->setTile(0, 0, C_Tilemap::packCell(1, 0, 0, false, true));
    canvas.clear(Pixel4(0));
    tm->draw(canvas);
    ASSERT(canvas.getPixel(2, 12).value == 5, "TMATTR-09: vflip draws at (2,12)");

    delete obj;

    initTileData();
}

// ---------------------------------------------------------------------------
// TMATTR-10: palbank render (per-tile Remap)
// ---------------------------------------------------------------------------
static void test_tmattr10_palbank_render() {
    printf("--- TMATTR-10: palbank render ---\n");

    // frame 1 all pixels index 5.
    memset(g_tileData, 5, sizeof(g_tileData));

    Object* obj = new Object();
    obj->addComponent<C_Position>();
    C_Tilemap* tm = obj->addComponent<C_Tilemap>();
    tm->setSheet(makeSheet());
    tm->setTiles(g_zeroMap, 1, 1);

    // Bank 1 remaps 5 -> 7.
    Remap bank;
    bank.lut[5] = 7;
    tm->setPalbank(1, bank);

    Canvas4<16, 16> canvas;

    // palbank 0 → identity → pixel 5
    tm->setTile(0, 0, C_Tilemap::packCell(1, /*band*/0, /*palbank*/0, false, false));
    canvas.clear(Pixel4(0));
    tm->draw(canvas);
    ASSERT(canvas.getPixel(0, 0).value == 5, "TMATTR-10: palbank 0 leaves index 5");

    // palbank 1 → remap → pixel 7
    tm->setTile(0, 0, C_Tilemap::packCell(1, 0, 1, false, false));
    canvas.clear(Pixel4(0));
    tm->draw(canvas);
    ASSERT(canvas.getPixel(0, 0).value == 7, "TMATTR-10: palbank 1 remaps 5->7");

    delete obj;

    initTileData();
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main() {
    initTileData();
    initMap();
    initAttrs();

    test_tmattr01_resolve_dir();
    test_tmattr02_attr_at();
    test_tmattr02b_dir_flip_resolve();
    test_tmattr03_attr_at_pixel();
    test_tmattr04_sweep_aabb();
    test_tmattr05_sweep_circle();
    test_tmattr06_oneway();
    test_tmattr07_for_each_cell();
    test_tmattr08_build_solid_rects();
    test_tmattr09_flip_render();
    test_tmattr10_palbank_render();

    printf("\n%d passed, %d failed\n", passes, failures);
    return failures > 0 ? 1 : 0;
}
