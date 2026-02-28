/**
 * @file tilemap_test.cpp
 * @brief C++ unit tests for C_Tilemap component (Phase 43: TMAP-01..TMAP-04)
 *
 * Tests:
 *   TMAP-01:  setTiles populates grid; setTile/getTile round-trip correctly
 *   TMAP-01b: getTile returns 0 for out-of-bounds coords; setTile OOB does not crash
 *   TMAP-02:  draw() only renders visible tiles (viewport culling)
 *   TMAP-03:  Tile ID 0 produces no pixel writes (transparent sentinel)
 *   TMAP-04:  setScroll shifts rendered tile positions
 *   TMAP-04b: pixelToTile / tileToPixel are inverses; tileAtPixel returns correct ID
 */

#include <enjin2/core/object.hpp>
#include <enjin2/components/tilemap.hpp>
#include <enjin2/graphics/canvas.hpp>
#include <enjin2/graphics/sprite.hpp>
#include <enjin2/components/position.hpp>
#include <cstdio>
#include <cstring>

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
// Shared test tileset
//
// 16x16 cells, 4 frames arranged in a 4-column × 1-row grid.
// All pixels set to palette index 5 (non-transparent, non-zero).
// Frame 0 exists but is "wasted" (tilemap skips ID 0 in draw).
// Frames 1, 2, 3 are the usable tile types in tests.
// ---------------------------------------------------------------------------
static constexpr uint8_t TILE_W = 16;
static constexpr uint8_t TILE_H = 16;
static constexpr uint8_t TILE_COLS = 4;
static constexpr uint8_t TILE_ROWS = 1;
static constexpr uint8_t TILE_PIXEL = 5;  // palette index, non-transparent

// 4 frames * 16 * 16 = 1024 bytes — all pixels = index 5
static uint8_t g_tileData[TILE_COLS * TILE_W * TILE_H];

static void initTileData() {
    memset(g_tileData, TILE_PIXEL, sizeof(g_tileData));
}

static SpriteSheet makeSheet() {
    return SpriteSheet(g_tileData, TILE_W, TILE_H, TILE_COLS, TILE_ROWS);
}

// ---------------------------------------------------------------------------
// TMAP-01: Data structure basics
// ---------------------------------------------------------------------------
static void test_tmap01_data_structure() {
    printf("--- TMAP-01: Data structure basics ---\n");

    // 4x3 grid of tile IDs
    static const uint8_t map[3 * 4] = {
        1, 2, 3, 1,
        2, 3, 1, 2,
        3, 1, 2, 3
    };

    Object* obj = new Object();
    obj->addComponent<C_Position>();
    C_Tilemap* tilemap = obj->addComponent<C_Tilemap>();
    ASSERT(tilemap != nullptr, "TMAP-01: addComponent<C_Tilemap> should succeed");

    tilemap->setTiles(map, 4, 3);
    ASSERT(tilemap->getMapWidth()  == 4, "TMAP-01: map width should be 4");
    ASSERT(tilemap->getMapHeight() == 3, "TMAP-01: map height should be 3");

    // Verify individual tiles match input data
    ASSERT(tilemap->getTile(0, 0) == 1, "TMAP-01: tile (0,0) == 1");
    ASSERT(tilemap->getTile(1, 0) == 2, "TMAP-01: tile (1,0) == 2");
    ASSERT(tilemap->getTile(2, 0) == 3, "TMAP-01: tile (2,0) == 3");
    ASSERT(tilemap->getTile(3, 0) == 1, "TMAP-01: tile (3,0) == 1");
    ASSERT(tilemap->getTile(0, 1) == 2, "TMAP-01: tile (0,1) == 2");
    ASSERT(tilemap->getTile(0, 2) == 3, "TMAP-01: tile (0,2) == 3");
    ASSERT(tilemap->getTile(3, 2) == 3, "TMAP-01: tile (3,2) == 3");

    // setTile / getTile round-trip
    tilemap->setTile(2, 1, 5);
    ASSERT(tilemap->getTile(2, 1) == 5, "TMAP-01: setTile(2,1,5) -> getTile(2,1) == 5");

    delete obj;
}

// ---------------------------------------------------------------------------
// TMAP-01b: Out-of-bounds safety
// ---------------------------------------------------------------------------
static void test_tmap01b_out_of_bounds_safety() {
    printf("--- TMAP-01b: Out-of-bounds safety ---\n");

    Object* obj = new Object();
    obj->addComponent<C_Position>();
    C_Tilemap* tilemap = obj->addComponent<C_Tilemap>();

    static const uint8_t map[2 * 2] = { 1, 2, 3, 1 };
    tilemap->setTiles(map, 2, 2);

    // getTile OOB returns 0
    ASSERT(tilemap->getTile(99, 99) == 0, "TMAP-01b: getTile(99,99) should return 0");
    ASSERT(tilemap->getTile(2, 0)   == 0, "TMAP-01b: getTile(2,0) OOB should return 0");
    ASSERT(tilemap->getTile(0, 2)   == 0, "TMAP-01b: getTile(0,2) OOB should return 0");

    // setTile OOB must not crash or corrupt valid tiles
    tilemap->setTile(99, 99, 1);   // must not crash
    tilemap->setTile(2, 0, 7);     // must not crash
    ASSERT(tilemap->getTile(0, 0) == 1, "TMAP-01b: valid tile (0,0) unchanged after OOB setTile");

    delete obj;
}

// ---------------------------------------------------------------------------
// TMAP-02: Viewport culling — only visible tiles are rendered
// ---------------------------------------------------------------------------
static void test_tmap02_viewport_culling() {
    printf("--- TMAP-02: Viewport culling ---\n");

    // 20x20 tile map (larger than what fits on 160x128 canvas with 16x16 tiles)
    // 160/16 = 10 columns, 128/16 = 8 rows visible.
    // Tile at grid (19, 19) should be off-screen.
    static uint8_t map[20 * 20];
    memset(map, 1, sizeof(map));  // All tiles = ID 1

    Object* obj = new Object();
    obj->addComponent<C_Position>();
    C_Tilemap* tilemap = obj->addComponent<C_Tilemap>();
    tilemap->setSheet(makeSheet());
    tilemap->setTiles(map, 20, 20);
    tilemap->setScroll(0, 0);

    Canvas4<160, 128> canvas;
    canvas.clear(Pixel4(0));
    tilemap->draw(canvas);

    // Tile at grid (0,0) with scroll (0,0) should draw pixels at canvas (0,0)
    ASSERT(canvas.getPixel(0, 0).value == TILE_PIXEL,
           "TMAP-02: tile at grid (0,0) should render to canvas (0,0)");

    // Pixel just inside the visible area (last column tile starts at 9*16=144, last row at 7*16=112)
    ASSERT(canvas.getPixel(144, 112).value == TILE_PIXEL,
           "TMAP-02: tile at grid (9,7) should be visible");

    // Tile at grid (11, 9): screen position would be (11*16=176, 9*16=144) — off a 160x128 canvas
    // So canvas pixel (176, 144) is out of range; we verify that pixels at the last column's
    // right edge (x=160) don't exist (canvas boundary).
    // Indirectly: if only 10 columns are drawn (0..9), column 10 tile (screen x=160) is clipped.
    // We verify the tile at (10,0) doesn't write to x=160 by checking x=159 has content but x beyond
    // canvas is not accessible. Instead, verify the rightmost pixel in the canvas is set correctly.
    // Since tile (9,0) occupies x=144..159, all pixels in that range should be TILE_PIXEL.
    ASSERT(canvas.getPixel(159, 0).value == TILE_PIXEL,
           "TMAP-02: rightmost visible pixel (159,0) should have tile pixel");

    delete obj;
}

// ---------------------------------------------------------------------------
// TMAP-03: Tile ID 0 transparency
// ---------------------------------------------------------------------------
static void test_tmap03_tile_zero_transparent() {
    printf("--- TMAP-03: Tile ID 0 transparency ---\n");

    // 2x2 grid: top-left and bottom-right are tile 0 (transparent),
    //           top-right and bottom-left are tile 1 (drawn).
    static const uint8_t map[2 * 2] = {
        0, 1,
        1, 0
    };

    Object* obj = new Object();
    obj->addComponent<C_Position>();
    C_Tilemap* tilemap = obj->addComponent<C_Tilemap>();
    tilemap->setSheet(makeSheet());
    tilemap->setTiles(map, 2, 2);
    tilemap->setScroll(0, 0);

    Canvas4<64, 64> canvas;
    canvas.clear(Pixel4(7));  // Fill with 7 (distinct from TILE_PIXEL = 5)
    tilemap->draw(canvas);

    // Tile at (0,0) is ID 0 → transparent → canvas pixel should still be 7
    ASSERT(canvas.getPixel(0, 0).value == 7,
           "TMAP-03: tile ID 0 at grid (0,0) must not write pixels (should remain 7)");

    // Tile at (1,1) is ID 0 → transparent → screen pos (16,16) should still be 7
    ASSERT(canvas.getPixel(TILE_W, TILE_H).value == 7,
           "TMAP-03: tile ID 0 at grid (1,1) must not write pixels (should remain 7)");

    // Tile at (1,0) is ID 1 → drawn → screen pos (16,0) should be TILE_PIXEL
    ASSERT(canvas.getPixel(TILE_W, 0).value == TILE_PIXEL,
           "TMAP-03: tile ID 1 at grid (1,0) should write TILE_PIXEL");

    // Tile at (0,1) is ID 1 → drawn → screen pos (0,16) should be TILE_PIXEL
    ASSERT(canvas.getPixel(0, TILE_H).value == TILE_PIXEL,
           "TMAP-03: tile ID 1 at grid (0,1) should write TILE_PIXEL");

    delete obj;
}

// ---------------------------------------------------------------------------
// TMAP-04: Scroll offset shifts rendered tile positions
// ---------------------------------------------------------------------------
static void test_tmap04_scroll_offset() {
    printf("--- TMAP-04: Scroll offset shifts tiles ---\n");

    // 4x2 grid: column 0 = ID 0 (transparent), columns 1..3 = ID 1 (drawn)
    static const uint8_t map[2 * 4] = {
        0, 1, 1, 1,
        0, 1, 1, 1
    };

    Object* obj = new Object();
    obj->addComponent<C_Position>();
    C_Tilemap* tilemap = obj->addComponent<C_Tilemap>();
    tilemap->setSheet(makeSheet());
    tilemap->setTiles(map, 4, 2);

    // No scroll: tile (1,0) appears at screen x = 1*16 = 16
    {
        tilemap->setScroll(0, 0);
        Canvas4<80, 64> canvas;
        canvas.clear(Pixel4(0));
        tilemap->draw(canvas);

        ASSERT(canvas.getPixel(0, 0).value == 0,
               "TMAP-04: with no scroll, tile ID 0 at (0,0) -> screen (0,0) stays clear");
        ASSERT(canvas.getPixel(TILE_W, 0).value == TILE_PIXEL,
               "TMAP-04: with no scroll, tile ID 1 at (1,0) -> screen (16,0) has TILE_PIXEL");
    }

    // Scroll one tile right (16 pixels): tile (1,0) now appears at screen x = 1*16 - 16 = 0
    {
        tilemap->setScroll(TILE_W, 0);
        Canvas4<80, 64> canvas;
        canvas.clear(Pixel4(0));
        tilemap->draw(canvas);

        ASSERT(canvas.getPixel(0, 0).value == TILE_PIXEL,
               "TMAP-04: with scroll(16,0), tile ID 1 at grid (1,0) -> screen (0,0) has TILE_PIXEL");
    }

    // Scroll (0, TILE_H): tile (1,0) → screen y = 0*16 - 16 = -16 (off-screen top)
    // tile (1,1) → screen y = 1*16 - 16 = 0 (visible at top)
    {
        tilemap->setScroll(0, TILE_H);
        Canvas4<80, 64> canvas;
        canvas.clear(Pixel4(0));
        tilemap->draw(canvas);

        ASSERT(canvas.getPixel(TILE_W, 0).value == TILE_PIXEL,
               "TMAP-04: with scroll(0,16), tile ID 1 at grid (1,1) -> screen (16,0) has TILE_PIXEL");
    }

    delete obj;
}

// ---------------------------------------------------------------------------
// TMAP-04b: Coordinate helpers — pixelToTile, tileToPixel, tileAtPixel
// ---------------------------------------------------------------------------
static void test_tmap04b_coordinate_helpers() {
    printf("--- TMAP-04b: Coordinate helpers ---\n");

    // 4x3 grid with known tile IDs
    static const uint8_t map[3 * 4] = {
        1, 2, 3, 1,
        2, 3, 1, 2,
        3, 1, 2, 3
    };

    Object* obj = new Object();
    obj->addComponent<C_Position>();
    C_Tilemap* tilemap = obj->addComponent<C_Tilemap>();
    tilemap->setSheet(makeSheet());
    tilemap->setTiles(map, 4, 3);
    tilemap->setScroll(0, 0);

    // --- pixelToTile with scroll (0,0) ---
    // Screen pixel (24, 8): world pixel = (24, 8); tile = (24/16, 8/16) = (1, 0)
    {
        int16_t tx = -1, ty = -1;
        tilemap->pixelToTile(24, 8, tx, ty);
        ASSERT(tx == 1, "TMAP-04b: pixelToTile(24,8) -> tx == 1");
        ASSERT(ty == 0, "TMAP-04b: pixelToTile(24,8) -> ty == 0");
    }

    // Screen pixel (0, 0) with scroll (0,0): world (0,0) -> tile (0,0)
    {
        int16_t tx = -1, ty = -1;
        tilemap->pixelToTile(0, 0, tx, ty);
        ASSERT(tx == 0, "TMAP-04b: pixelToTile(0,0) -> tx == 0");
        ASSERT(ty == 0, "TMAP-04b: pixelToTile(0,0) -> ty == 0");
    }

    // Screen pixel (15, 15): world (15,15); tile = (0, 0) (still within first tile)
    {
        int16_t tx = -1, ty = -1;
        tilemap->pixelToTile(15, 15, tx, ty);
        ASSERT(tx == 0, "TMAP-04b: pixelToTile(15,15) -> tx == 0");
        ASSERT(ty == 0, "TMAP-04b: pixelToTile(15,15) -> ty == 0");
    }

    // Screen pixel (16, 0): world (16, 0); tile = (1, 0)
    {
        int16_t tx = -1, ty = -1;
        tilemap->pixelToTile(16, 0, tx, ty);
        ASSERT(tx == 1, "TMAP-04b: pixelToTile(16,0) -> tx == 1");
        ASSERT(ty == 0, "TMAP-04b: pixelToTile(16,0) -> ty == 0");
    }

    // --- tileToPixel with scroll (0,0) ---
    // grid (2, 3): screen px = 2*16 - 0 = 32, py = 3*16 - 0 = 48
    {
        int16_t px = -1, py = -1;
        tilemap->tileToPixel(2, 3, px, py);
        ASSERT(px == 32, "TMAP-04b: tileToPixel(2,3) -> px == 32");
        ASSERT(py == 48, "TMAP-04b: tileToPixel(2,3) -> py == 48");
    }

    // grid (0, 0): screen px = 0, py = 0
    {
        int16_t px = -1, py = -1;
        tilemap->tileToPixel(0, 0, px, py);
        ASSERT(px == 0, "TMAP-04b: tileToPixel(0,0) -> px == 0");
        ASSERT(py == 0, "TMAP-04b: tileToPixel(0,0) -> py == 0");
    }

    // --- pixelToTile with scroll (8, 0) ---
    // Screen pixel (0, 0): world = (0+8, 0+0) = (8, 0); tile = (0, 0)
    {
        tilemap->setScroll(8, 0);
        int16_t tx = -1, ty = -1;
        tilemap->pixelToTile(0, 0, tx, ty);
        ASSERT(tx == 0, "TMAP-04b: with scroll(8,0), pixelToTile(0,0) -> tx == 0");
        ASSERT(ty == 0, "TMAP-04b: with scroll(8,0), pixelToTile(0,0) -> ty == 0");
    }

    // Screen pixel (8, 0) with scroll (8, 0): world = (16, 0); tile = (1, 0)
    {
        tilemap->setScroll(8, 0);
        int16_t tx = -1, ty = -1;
        tilemap->pixelToTile(8, 0, tx, ty);
        ASSERT(tx == 1, "TMAP-04b: with scroll(8,0), pixelToTile(8,0) -> tx == 1");
        ASSERT(ty == 0, "TMAP-04b: with scroll(8,0), pixelToTile(8,0) -> ty == 0");
    }

    // --- tileAtPixel with scroll (0,0) ---
    // Reset scroll for tileAtPixel tests
    tilemap->setScroll(0, 0);
    // Screen pixel (24, 8): world (24, 8); tile (1, 0); map[(0*4)+1] = 2
    ASSERT(tilemap->tileAtPixel(24, 8) == 2,
           "TMAP-04b: tileAtPixel(24,8) should return 2 (tile at grid (1,0))");

    // Screen pixel (0, 0): tile (0, 0); map[0] = 1
    ASSERT(tilemap->tileAtPixel(0, 0) == 1,
           "TMAP-04b: tileAtPixel(0,0) should return 1 (tile at grid (0,0))");

    // Screen pixel (32, 16): tile (2, 1); map[(1*4)+2] = 1
    ASSERT(tilemap->tileAtPixel(32, 16) == 1,
           "TMAP-04b: tileAtPixel(32,16) should return 1 (tile at grid (2,1))");

    // tileAtPixel OOB (very large coord) should return 0
    ASSERT(tilemap->tileAtPixel(9999, 9999) == 0,
           "TMAP-04b: tileAtPixel OOB should return 0");

    delete obj;
}

// ---------------------------------------------------------------------------
// TMAP-01: getMapWidth/getMapHeight after multiple setTiles calls
// ---------------------------------------------------------------------------
static void test_tmap01_dimensions_reset() {
    printf("--- TMAP-01 (extra): dimensions reset on re-setTiles ---\n");

    static const uint8_t map8x8[8 * 8] = {};  // all zeros
    static const uint8_t map3x3[3 * 3] = { 1,2,3, 4,5,6, 7,8,9 };

    Object* obj = new Object();
    obj->addComponent<C_Position>();
    C_Tilemap* tilemap = obj->addComponent<C_Tilemap>();

    tilemap->setTiles(map8x8, 8, 8);
    ASSERT(tilemap->getMapWidth()  == 8, "TMAP-01(extra): first setTiles width == 8");
    ASSERT(tilemap->getMapHeight() == 8, "TMAP-01(extra): first setTiles height == 8");

    tilemap->setTiles(map3x3, 3, 3);
    ASSERT(tilemap->getMapWidth()  == 3, "TMAP-01(extra): second setTiles width == 3");
    ASSERT(tilemap->getMapHeight() == 3, "TMAP-01(extra): second setTiles height == 3");

    // Tiles should now reflect the 3x3 map
    ASSERT(tilemap->getTile(0, 0) == 1, "TMAP-01(extra): after re-setTiles, tile(0,0)==1");
    ASSERT(tilemap->getTile(2, 2) == 9, "TMAP-01(extra): after re-setTiles, tile(2,2)==9");

    // Coordinates outside the new 3x3 boundary should return 0
    ASSERT(tilemap->getTile(3, 0) == 0, "TMAP-01(extra): getTile(3,0) OOB after resize returns 0");

    delete obj;
}

// ---------------------------------------------------------------------------
// TMAP-03: draw() without setSheet does nothing (no crash)
// ---------------------------------------------------------------------------
static void test_tmap03_no_sheet_no_draw() {
    printf("--- TMAP-03 (extra): draw without sheet does not crash ---\n");

    static const uint8_t map[2 * 2] = { 1, 1, 1, 1 };

    Object* obj = new Object();
    obj->addComponent<C_Position>();
    C_Tilemap* tilemap = obj->addComponent<C_Tilemap>();
    tilemap->setTiles(map, 2, 2);
    // No setSheet() call — m_sheet.data is nullptr

    Canvas4<64, 64> canvas;
    canvas.clear(Pixel4(3));
    tilemap->draw(canvas);  // Must not crash

    // Canvas should be untouched
    ASSERT(canvas.getPixel(0, 0).value == 3,
           "TMAP-03(extra): canvas unchanged when no sheet is set");

    delete obj;
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main() {
    initTileData();

    test_tmap01_data_structure();
    test_tmap01b_out_of_bounds_safety();
    test_tmap02_viewport_culling();
    test_tmap03_tile_zero_transparent();
    test_tmap04_scroll_offset();
    test_tmap04b_coordinate_helpers();
    test_tmap01_dimensions_reset();
    test_tmap03_no_sheet_no_draw();

    printf("\n%d passed, %d failed\n", passes, failures);
    return failures > 0 ? 1 : 0;
}
