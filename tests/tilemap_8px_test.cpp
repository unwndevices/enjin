/**
 * @file tilemap_8px_test.cpp
 * @brief C_Tilemap with an 8x8 tileset (#372): the tile size is the tileset's.
 *
 * The map is the Marble Maze's shape: 20x20 tiles of 8 px (160x160 px), SOLID
 * walls, tile kinds for holes and the exit, and a radius-2 ball.
 *
 * Test IDs:
 *   TM8-01  pixelToTile / tileToPixel / tileAtPixel at 8 px (scroll, negatives)
 *   TM8-02  attrAtPixel answers the wall / hole / exit at 8 px
 *   TM8-03  sweepCircle r=2 stops on 8 px walls and runs a 1-tile corridor
 *   TM8-04  sweepAabb slides along an 8 px wall
 *   TM8-05  buildSolidRects merges in 8 px units
 *   TM8-06  draw paints 8 px cells; scroll shifts them by pixels
 *   TM8-07  drawRegionBand paints every 8 px cell under a 16 px region
 */

#include <enjin2/core/object.hpp>
#include <enjin2/components/tilemap.hpp>
#include <enjin2/components/position.hpp>
#include <enjin2/graphics/canvas.hpp>
#include <enjin2/graphics/sprite.hpp>
#include <cmath>
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

static bool near(float a, float b) { return std::fabs(a - b) < 1e-3f; }

// ---------------------------------------------------------------------------
// Tileset: 8x8 cells, frame-major. 0 transparent, 1 wall (muted),
// 2 floor (surface), 3 hole (bg), 4 exit (accent).
// ---------------------------------------------------------------------------
static constexpr uint8_t T = 8;
static constexpr uint8_t FRAMES = 5;
static constexpr uint8_t WALL = 1, FLOOR = 2, HOLE = 3, EXIT = 4;
static constexpr uint8_t KIND_WALL = 1, KIND_HOLE = 2, KIND_EXIT = 3;
static const uint8_t kColour[FRAMES] = {PALETTE_TRANSPARENT, ROLE_MUTED, ROLE_SURFACE, ROLE_BG, ROLE_ACCENT};
static uint8_t g_tiles[FRAMES * T * T];

static constexpr uint8_t MAP = 20;
static uint16_t g_maze[MAP * MAP];
static TileAttr g_attrs[FRAMES];

// The maze: an outer wall; a wall column at tx=5 (rows 1..18) with a one-tile
// gap at ty=3; a wall row at ty=10 (cols 6..18); a hole at (8,5); the exit at
// (18,18). Everything else is floor.
static void initMaze() {
    for (int f = 0; f < FRAMES; ++f) memset(g_tiles + f * T * T, kColour[f], T * T);
    for (int ty = 0; ty < MAP; ++ty) {
        for (int tx = 0; tx < MAP; ++tx) {
            uint8_t id = FLOOR;
            if (tx == 0 || ty == 0 || tx == MAP - 1 || ty == MAP - 1) id = WALL;
            else if (tx == 5 && ty != 3) id = WALL;
            else if (ty == 10 && tx >= 6) id = WALL;
            else if (tx == 8 && ty == 5) id = HOLE;
            else if (tx == 18 && ty == 18) id = EXIT;
            g_maze[ty * MAP + tx] = C_Tilemap::packCell(id);
        }
    }
    g_attrs[0] = TileAttr{};
    g_attrs[WALL] = TileAttr{TileAttr::FLAG_SOLID, KIND_WALL};
    g_attrs[FLOOR] = TileAttr{};
    g_attrs[HOLE] = TileAttr{0, KIND_HOLE};
    g_attrs[EXIT] = TileAttr{0, KIND_EXIT};
}

static C_Tilemap* makeMaze(Object* obj) {
    obj->addComponent<C_Position>();
    C_Tilemap* tm = obj->addComponent<C_Tilemap>();
    tm->setSheet(SpriteSheet(g_tiles, T, T, FRAMES, 1));
    tm->setTiles(g_maze, MAP, MAP);
    tm->setAttrs(g_attrs, FRAMES);
    return tm;
}

// ---------------------------------------------------------------------------
static void test_tm8_01_coordinates() {
    printf("--- TM8-01: pixel <-> tile at 8 px ---\n");
    Object* obj = new Object();
    C_Tilemap* tm = makeMaze(obj);

    int16_t tx = 0, ty = 0;
    tm->pixelToTile(15, 8, tx, ty);
    ASSERT(tx == 1 && ty == 1, "TM8-01: (15,8) is tile (1,1)");
    tm->pixelToTile(16, 7, tx, ty);
    ASSERT(tx == 2 && ty == 0, "TM8-01: (16,7) is tile (2,0)");
    tm->pixelToTile(159, 159, tx, ty);
    ASSERT(tx == 19 && ty == 19, "TM8-01: (159,159) is the last tile");
    tm->pixelToTile(-1, -9, tx, ty);
    ASSERT(tx == -1 && ty == -2, "TM8-01: negatives floor");

    int16_t px = 0, py = 0;
    tm->tileToPixel(3, 2, px, py);
    ASSERT(px == 24 && py == 16, "TM8-01: tile (3,2) top-left is (24,16)");

    ASSERT(C_Tilemap::cellTileId(tm->tileAtPixel(8 * 8 + 7, 5 * 8)) == HOLE,
           "TM8-01: tileAtPixel finds the hole");
    ASSERT(C_Tilemap::cellTileId(tm->tileAtPixel(5 * 8, 3 * 8 + 4)) == FLOOR,
           "TM8-01: the gap in the wall column is floor");
    ASSERT(tm->tileAtPixel(160, 0) == 0, "TM8-01: past the map is 0");

    tm->setScroll(8, -8);
    tm->pixelToTile(0, 8, tx, ty);
    ASSERT(tx == 1 && ty == 0, "TM8-01: scroll is in pixels, one tile = 8 px");
    tm->tileToPixel(1, 0, px, py);
    ASSERT(px == 0 && py == 8, "TM8-01: tileToPixel applies scroll");
    delete obj;
}

static void test_tm8_02_attr_at_pixel() {
    printf("--- TM8-02: attrAtPixel at 8 px ---\n");
    Object* obj = new Object();
    C_Tilemap* tm = makeMaze(obj);

    ASSERT(tm->attrAtPixel(7, 7).solid(), "TM8-02: (7,7) is the outer wall");
    ASSERT(tm->attrAtPixel(7, 7).kind == KIND_WALL, "TM8-02: wall kind");
    ASSERT(!tm->attrAtPixel(8, 8).solid(), "TM8-02: (8,8) is floor");
    ASSERT(tm->attrAtPixel(8, 8).kind == 0, "TM8-02: floor kind 0");
    ASSERT(tm->attrAtPixel(64, 40).kind == KIND_HOLE, "TM8-02: hole's first pixel");
    ASSERT(tm->attrAtPixel(71, 47).kind == KIND_HOLE, "TM8-02: hole's last pixel");
    ASSERT(tm->attrAtPixel(72, 40).kind == 0, "TM8-02: right of the hole is floor");
    ASSERT(tm->attrAtPixel(148, 148).kind == KIND_EXIT, "TM8-02: exit");
    ASSERT(!tm->attrAtPixel(-1, 0).solid(), "TM8-02: off the map is passable");
    delete obj;
}

static void test_tm8_03_sweep_circle() {
    printf("--- TM8-03: sweepCircle r=2 at 8 px ---\n");
    Object* obj = new Object();
    C_Tilemap* tm = makeMaze(obj);
    const float r = 2.0f;

    // Left into the outer wall (its face at x=8): centre stops at 10.
    SweepResult a = tm->sweepCircle(20.0f, 20.0f, r, -200.0f, 0.0f, 0.1f);
    ASSERT(a.hit, "TM8-03: hits the outer wall");
    ASSERT(near(a.x, 10.0f) && near(a.y, 20.0f), "TM8-03: centre stops at x=10");
    ASSERT(near(a.normalX, 1.0f) && near(a.normalY, 0.0f), "TM8-03: normal +x");

    // Right into the wall column tx=5 (face at x=40) on row 1: stops at 38.
    SweepResult b = tm->sweepCircle(20.0f, 12.0f, r, 300.0f, 0.0f, 0.1f);
    ASSERT(b.hit && near(b.x, 38.0f), "TM8-03: stops on the wall column at x=38");
    ASSERT(near(b.normalX, -1.0f), "TM8-03: normal -x");

    // Down the corridor into the wall row ty=10 (face at y=80): stops at 78.
    SweepResult c = tm->sweepCircle(100.0f, 60.0f, r, 0.0f, 400.0f, 0.1f);
    ASSERT(c.hit && near(c.y, 78.0f), "TM8-03: stops on the wall row at y=78");
    ASSERT(near(c.normalY, -1.0f), "TM8-03: normal -y");

    // Through the one-tile gap at ty=3 (y 24..32): the ball spans 26..30.
    SweepResult d = tm->sweepCircle(20.0f, 28.0f, r, 400.0f, 0.0f, 0.1f);
    ASSERT(!d.hit && near(d.x, 60.0f), "TM8-03: rolls through the 8 px gap");

    // One pixel low in the gap, the ball overlaps row 4 and the column stops it.
    SweepResult e = tm->sweepCircle(20.0f, 31.0f, r, 400.0f, 0.0f, 0.1f);
    ASSERT(e.hit && near(e.x, 38.0f), "TM8-03: off-centre in the gap clips the wall");

    // Diagonal into the corner of the outer wall: both axes stop.
    SweepResult f = tm->sweepCircle(14.0f, 14.0f, r, -100.0f, -100.0f, 0.1f);
    ASSERT(f.hit && near(f.x, 10.0f) && near(f.y, 10.0f), "TM8-03: corner stops both axes");

    // Rolling over the hole does not block: a hole is a kind, not a wall.
    SweepResult g = tm->sweepCircle(50.0f, 44.0f, r, 300.0f, 0.0f, 0.1f);
    ASSERT(!g.hit && near(g.x, 80.0f), "TM8-03: holes are passable");
    delete obj;
}

static void test_tm8_04_sweep_aabb() {
    printf("--- TM8-04: sweepAabb at 8 px ---\n");
    Object* obj = new Object();
    C_Tilemap* tm = makeMaze(obj);

    // A 4x4 box moving up-right: X stops on the column (40-4=36), Y goes on.
    SweepResult r = tm->sweepAabb(30.0f, 50.0f, 4.0f, 4.0f, 100.0f, -100.0f, 0.1f);
    ASSERT(r.hit, "TM8-04: hit");
    ASSERT(near(r.x, 36.0f) && near(r.y, 40.0f), "TM8-04: slides to (36,40)");
    ASSERT(near(r.normalX, -1.0f) && near(r.normalY, 0.0f), "TM8-04: normal -x");
    delete obj;
}

static void test_tm8_05_solid_rects() {
    printf("--- TM8-05: buildSolidRects at 8 px ---\n");
    Object* obj = new Object();
    C_Tilemap* tm = makeMaze(obj);

    const std::vector<Rect> rects = tm->buildSolidRects();
    int solidTiles = 0;
    for (int ty = 0; ty < MAP; ++ty)
        for (int tx = 0; tx < MAP; ++tx)
            if (tm->isSolid(static_cast<uint8_t>(tx), static_cast<uint8_t>(ty))) ++solidTiles;
    long area = 0;
    bool aligned = true;
    for (const Rect& rc : rects) {
        area += static_cast<long>(rc.width) * rc.height;
        if (rc.x % 8 || rc.y % 8 || rc.width % 8 || rc.height % 8) aligned = false;
    }
    ASSERT(area == solidTiles * 64L, "TM8-05: rects cover exactly the solid tiles");
    ASSERT(aligned, "TM8-05: rects sit on the 8 px grid");
    ASSERT(!rects.empty() && rects[0].x == 0 && rects[0].y == 0 &&
               rects[0].width == 160 && rects[0].height == 8,
           "TM8-05: the top wall is one 160x8 rect");
    bool gapOpen = true;
    for (const Rect& rc : rects)
        if (40 >= rc.x && 40 < rc.x + rc.width && 28 >= rc.y && 28 < rc.y + rc.height)
            gapOpen = false;
    ASSERT(gapOpen, "TM8-05: no rect covers the gap");
    delete obj;
}

static void test_tm8_06_draw() {
    printf("--- TM8-06: draw at 8 px ---\n");
    Object* obj = new Object();
    C_Tilemap* tm = makeMaze(obj);
    Canvas4<160, 160> canvas;
    canvas.clear(Pixel4(ROLE_ACCENT4));
    tm->draw(canvas);
    ASSERT(canvas.getPixel(7, 7).value == ROLE_MUTED, "TM8-06: wall at (7,7)");
    ASSERT(canvas.getPixel(8, 8).value == 1, "TM8-06: floor at (8,8)");
    ASSERT(canvas.getPixel(64, 40).value == 0, "TM8-06: hole cell starts at (64,40)");
    ASSERT(canvas.getPixel(71, 47).value == 0, "TM8-06: hole cell ends at (71,47)");
    ASSERT(canvas.getPixel(72, 47).value == 1, "TM8-06: floor right of the hole");
    ASSERT(canvas.getPixel(150, 150).value == ROLE_ACCENT, "TM8-06: exit");

    tm->setScroll(4, 0);
    canvas.clear(Pixel4(ROLE_ACCENT4));
    tm->draw(canvas);
    ASSERT(canvas.getPixel(3, 8).value == ROLE_MUTED, "TM8-06: scroll 4 px shows half the wall");
    ASSERT(canvas.getPixel(4, 8).value == 1, "TM8-06: then floor");
    ASSERT(canvas.getPixel(60, 40).value == 0, "TM8-06: the hole moved 4 px left");
    delete obj;
}

static void test_tm8_07_draw_region_band() {
    printf("--- TM8-07: drawRegionBand at 8 px ---\n");
    Object* obj = new Object();
    C_Tilemap* tm = makeMaze(obj);
    // Over-band exit so the band filter shows.
    tm->setTile(18, 18, C_Tilemap::packCell(EXIT, /*band*/1));
    Canvas4<160, 160> canvas;
    canvas.clear(Pixel4(ROLE_ACCENT4));

    // One 16 px compositor tile at (64,32) = map cells (8..9, 4..5): hole at (8,5).
    tm->drawRegionBand(canvas, 64, 32, 16, 16, C_Tilemap::Band::Under, Pixel4(6));
    ASSERT(canvas.getPixel(64, 32).value == 1, "TM8-07: cell (8,4) floor");
    ASSERT(canvas.getPixel(79, 32).value == 1, "TM8-07: cell (9,4) floor");
    ASSERT(canvas.getPixel(64, 47).value == 0, "TM8-07: cell (8,5) hole");
    ASSERT(canvas.getPixel(79, 47).value == 1, "TM8-07: cell (9,5) floor");
    ASSERT(canvas.getPixel(63, 32).value == ROLE_ACCENT4, "TM8-07: left of the region untouched");
    ASSERT(canvas.getPixel(80, 47).value == ROLE_ACCENT4, "TM8-07: right of the region untouched");

    // The over-band exit is skipped by the under pass, painted by the over pass.
    tm->drawRegionBand(canvas, 144, 144, 16, 16, C_Tilemap::Band::Under, Pixel4(6));
    ASSERT(canvas.getPixel(150, 150).value == 6, "TM8-07: other band clears");
    ASSERT(canvas.getPixel(158, 158).value == ROLE_MUTED, "TM8-07: wall in the same region");
    tm->drawRegionBand(canvas, 144, 144, 16, 16, C_Tilemap::Band::Over, Pixel4(15));
    ASSERT(canvas.getPixel(150, 150).value == ROLE_ACCENT, "TM8-07: over pass draws the exit");

    // Scrolled by half a cell: the region's cells shift with the map.
    tm->setScroll(4, 0);
    canvas.clear(Pixel4(ROLE_ACCENT4));
    tm->drawRegionBand(canvas, 48, 32, 16, 16, C_Tilemap::Band::Under, Pixel4(6));
    ASSERT(canvas.getPixel(60, 40).value == 0, "TM8-07: hole at screen x=60 when scrolled");
    ASSERT(canvas.getPixel(59, 40).value == 1, "TM8-07: floor left of it");

    // A region past the map is the clear colour.
    tm->setScroll(0, 0);
    tm->drawRegionBand(canvas, -16, 0, 16, 16, C_Tilemap::Band::Under, Pixel4(6));
    tm->drawRegionBand(canvas, 0, 0, 16, 16, C_Tilemap::Band::Under, Pixel4(6));
    ASSERT(canvas.getPixel(0, 0).value == ROLE_MUTED, "TM8-07: on-map cell after an off-map region");
    delete obj;
}

int main() {
    initMaze();
    test_tm8_01_coordinates();
    test_tm8_02_attr_at_pixel();
    test_tm8_03_sweep_circle();
    test_tm8_04_sweep_aabb();
    test_tm8_05_solid_rects();
    test_tm8_06_draw();
    test_tm8_07_draw_region_band();
    printf("\n%d passed, %d failed\n", passes, failures);
    return failures > 0 ? 1 : 0;
}
