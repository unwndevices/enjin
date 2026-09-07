#include <enjin2/graphics/canvas.hpp>
#include <cstdio>

using namespace enjin2;

static int passes = 0;
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

// A 48x32 canvas => 3x2 = 6 tiles of 16x16.
using C = Canvas4<48, 32>;

static void test_tile_geometry()
{
    printf("--- tile geometry ---\n");
    ASSERT(C::TILE_SIZE == 16, "TILE_SIZE is 16");
    ASSERT(C::TILES_X == 3, "48px wide => 3 tile columns");
    ASSERT(C::TILES_Y == 2, "32px tall => 2 tile rows");
    ASSERT(C::TILE_COUNT == 6, "3x2 => 6 tiles");
}

static void test_fresh_canvas_is_fully_dirty()
{
    printf("--- fresh canvas fully dirty ---\n");
    C c;
    ASSERT(c.hasDirty(), "fresh canvas has dirty tiles (needs first composite)");
    ASSERT(c.dirtyTileCount() == 6, "fresh canvas: all 6 tiles dirty");
}

static void test_clear_dirty_resets()
{
    printf("--- clearDirty resets ---\n");
    C c;
    c.clearDirty();
    ASSERT(!c.hasDirty(), "after clearDirty: no dirty tiles");
    ASSERT(c.dirtyTileCount() == 0, "after clearDirty: count 0");
}

static void test_setpixel_marks_only_its_tile()
{
    printf("--- setPixel marks one tile ---\n");
    C c;
    c.clearDirty();
    c.setPixel(20, 4, Pixel4(3)); // tile (1,0)
    ASSERT(c.dirtyTileCount() == 1, "setPixel marks exactly one tile");
    ASSERT(c.isTileDirty(1, 0), "setPixel(20,4) marks tile (1,0)");
    ASSERT(!c.isTileDirty(0, 0), "tile (0,0) stays clean");
    ASSERT(!c.isTileDirty(2, 1), "tile (2,1) stays clean");
}

static void test_invalidate_rect_marks_span()
{
    printf("--- invalidate(rect) marks tile span ---\n");
    C c;
    c.clearDirty();
    // Rect from (12,4) size 12x8 covers x pixels 12..23 => tiles 0..1, y tile 0.
    c.invalidate(12, 4, 12, 8);
    ASSERT(c.isTileDirty(0, 0), "rect spans tile (0,0)");
    ASSERT(c.isTileDirty(1, 0), "rect spans tile (1,0)");
    ASSERT(!c.isTileDirty(2, 0), "rect does not reach tile (2,0)");
    ASSERT(!c.isTileDirty(0, 1), "rect does not reach row 1");
    ASSERT(c.dirtyTileCount() == 2, "rect marks exactly 2 tiles");
}

static void test_invalidate_clips_to_canvas()
{
    printf("--- invalidate clips ---\n");
    C c;
    c.clearDirty();
    c.invalidate(-100, -100, 8, 8); // fully off-canvas top-left corner overlaps (0,0)? no: x2=-92
    ASSERT(!c.hasDirty(), "fully off-canvas rect marks nothing");
    c.invalidate(-4, -4, 8, 8); // overlaps (0,0)
    ASSERT(c.isTileDirty(0, 0), "partly-onscreen rect marks the overlapped tile");
    ASSERT(c.dirtyTileCount() == 1, "only the overlapped tile is marked");
}

static void test_fillrect_marks_covered_tiles()
{
    printf("--- fillRect marks covered tiles ---\n");
    C c;
    c.clearDirty();
    c.fillRect(0, 0, 48, 16, Pixel4(5)); // whole top row of tiles
    ASSERT(c.isTileDirty(0, 0) && c.isTileDirty(1, 0) && c.isTileDirty(2, 0),
           "fillRect covers all top-row tiles");
    ASSERT(!c.isTileDirty(0, 1), "bottom row untouched");
    ASSERT(c.dirtyTileCount() == 3, "fillRect marks 3 tiles");
}

static void test_clear_marks_all()
{
    printf("--- clear marks all ---\n");
    C c;
    c.clearDirty();
    c.clear(Pixel4(0));
    ASSERT(c.dirtyTileCount() == 6, "clear() marks every tile dirty");
}

static void test_static_layer_repaints_zero_tiles()
{
    printf("--- static layer: 0 tiles across frames ---\n");
    C c;
    // Frame 0: draw once, then consume + clear at frame boundary.
    c.clearDirty();
    c.fillRect(0, 0, 16, 16, Pixel4(2));
    ASSERT(c.dirtyTileCount() == 1, "frame 0: one tile drawn");
    c.clearDirty(); // compositor consumed the frame

    // Frames 1..3: nothing drawn, nothing invalidated -> 0 dirty tiles.
    for (int f = 0; f < 3; ++f) {
        ASSERT(c.dirtyTileCount() == 0, "static layer repaints 0 tiles on a quiet frame");
    }
    // Content still present (dirty tracking never touched pixels).
    ASSERT(c.getPixel(0, 0).value == 2, "static layer keeps its content");
}

int main()
{
    printf("canvas_dirty_test\n");
    printf("=================\n");

    test_tile_geometry();
    test_fresh_canvas_is_fully_dirty();
    test_clear_dirty_resets();
    test_setpixel_marks_only_its_tile();
    test_invalidate_rect_marks_span();
    test_invalidate_clips_to_canvas();
    test_fillrect_marks_covered_tiles();
    test_clear_marks_all();
    test_static_layer_repaints_zero_tiles();

    printf("\nResults: %d passed, %d failed\n", passes, failures);
    return failures == 0 ? 0 : 1;
}
