#include <enjin2/graphics/layer_compositor.hpp>
#include <enjin2/graphics/presenter.hpp>
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

// 64x64 => 4x4 = 16 tiles.
using Comp = LayerCompositor<64, 64>;

// Counting presenter: records how many times present() was called and the dirty
// count of the last frame it saw.
struct CountingPresenter : IPresenter<64, 64> {
    int calls = 0;
    uint16_t lastDirty = 0;
    void present(const Frame<64, 64>& frame) override {
        ++calls;
        uint16_t n = 0;
        for (uint16_t ty = 0; ty < frame.tilesY; ++ty)
            for (uint16_t tx = 0; tx < frame.tilesX; ++tx)
                if (frame.isTileDirty(tx, ty)) ++n;
        lastDirty = n;
    }
};

static void runFrame(Comp& c, CountingPresenter& p) {
    c.beginFrame();
    c.compositeDirty();
    c.present(p);
    c.endFrame();
}

static void test_present_once_per_frame()
{
    printf("--- present once per frame ---\n");
    Comp c;
    CountingPresenter p;
    runFrame(c, p);
    runFrame(c, p);
    runFrame(c, p);
    ASSERT(p.calls == 3, "present() called exactly once per frame (3 frames)");
}

static void test_only_dirty_tiles_written()
{
    printf("--- only dirty tiles written to output ---\n");
    Comp c;
    CountingPresenter p;
    c.beginFrame();
    c.layers[1].setPixel(3, 3, Pixel4(7)); // tile (0,0)
    c.compositeDirty();
    ASSERT(c.mergedDirtyCount() == 1, "exactly one tile composited");
    ASSERT(c.output.getPixel(3, 3).value == 7, "drawn pixel reaches output");
    ASSERT(c.output.getPixel(40, 40).value == 0, "untouched tile stays background");
    c.present(p);
    ASSERT(p.lastDirty == 1, "presenter frame carries one dirty tile");
    c.endFrame();
}

static void test_per_layer_tint_applied()
{
    printf("--- per-layer tint applied ---\n");
    Comp c;
    CountingPresenter p;
    // Draw index 5 on layer 1, then tint layer 1 solid->3.
    c.beginFrame();
    c.layers[1].setPixel(2, 2, Pixel4(5)); // tile (0,0)
    c.setTint(1, Remap::solid(3));
    c.compositeDirty();
    ASSERT(c.output.getPixel(2, 2).value == 3, "tint solid(3) recolours the pixel to 3");
    c.present(p);
    c.endFrame();
}

static void test_source_15_transparent_regardless_of_tint()
{
    printf("--- source index 15 stays transparent under tint ---\n");
    Comp c;
    CountingPresenter p;
    // Layer 0 draws index 4 at (5,5). Layer 1 is transparent (15) there but
    // tinted solid(9): the compositor must ignore layer1's 15 source and show
    // layer0's 4.
    c.beginFrame();
    c.layers[0].setPixel(5, 5, Pixel4(4));
    c.setTint(1, Remap::solid(9));
    c.compositeDirty();
    ASSERT(c.output.getPixel(5, 5).value == 4,
           "layer1 transparent source ignored despite solid tint -> shows layer0");
    c.present(p);
    c.endFrame();
}

static void test_static_layer_settles_to_zero()
{
    printf("--- static (None-restore) layer settles to 0 tiles ---\n");
    Comp c;
    CountingPresenter p;
    // Frame 0: draw once on layer 1 (default restore = None).
    c.beginFrame();
    c.layers[1].fillRect(0, 0, 16, 16, Pixel4(2)); // tile (0,0)
    c.compositeDirty();
    ASSERT(c.mergedDirtyCount() == 1, "frame 0: one tile drawn");
    c.present(p);
    c.endFrame();

    // Frames 1..3: no draws, no invalidation -> 0 tiles composited.
    for (int f = 0; f < 3; ++f) {
        runFrame(c, p);
        ASSERT(c.mergedDirtyCount() == 0, "static layer composites 0 tiles on quiet frames");
    }
    ASSERT(c.output.getPixel(1, 1).value == 2, "static content persists in output");
}

static void test_backdrop_restore_erases_moved_content()
{
    printf("--- backdrop restore erases old sprite tile ---\n");
    Comp c;
    CountingPresenter p;
    // Layer 1 uses a backdrop so its dirty tiles reset to colour 0 each frame.
    c.setBackdrop(1, Pixel4(0));
    // Settle the initial full backdrop fill out of the way.
    runFrame(c, p); // frame with the setBackdrop full invalidate
    runFrame(c, p); // settles

    // Frame A: draw a sprite pixel in tile (1,1).
    c.beginFrame();
    c.layers[1].setPixel(20, 20, Pixel4(6)); // tile (1,1)
    c.compositeDirty();
    ASSERT(c.output.getPixel(20, 20).value == 6, "sprite drawn in tile (1,1)");
    c.present(p);
    c.endFrame();

    // Frame B: draw nothing. Backdrop restore should repaint tile (1,1) to 0.
    c.beginFrame();
    c.compositeDirty();
    ASSERT(c.output.getPixel(20, 20).value == 0, "old sprite erased by backdrop restore");
    ASSERT(c.mergedDirtyCount() == 1, "only the restored tile recomposited");
    c.present(p);
    c.endFrame();

    // Frame C: quiet -> settles to 0.
    runFrame(c, p);
    ASSERT(c.mergedDirtyCount() == 0, "backdrop layer settles to 0 after erase");
}

static void test_nonzero_backdrop_composites_and_settles()
{
    printf("--- non-zero backdrop reaches output, then settles ---\n");
    Comp c;
    CountingPresenter p;
    c.setBackdrop(0, Pixel4(5)); // whole layer 0 filled with index 5

    // Frame 1: the seed fill must reach output (regression: beginFrame used to
    // wipe it before compositing).
    c.beginFrame();
    c.compositeDirty();
    ASSERT(c.output.getPixel(30, 30).value == 5, "non-zero backdrop composited into output");
    ASSERT(c.mergedDirtyCount() == Comp::TILE_COUNT, "frame 1 repaints the whole backdrop");
    c.present(p);
    c.endFrame();

    // Frame 2 repaints once more (restoreSet still full), then it settles.
    runFrame(c, p);
    ASSERT(c.output.getPixel(30, 30).value == 5, "backdrop persists after restore");
    runFrame(c, p);
    ASSERT(c.mergedDirtyCount() == 0, "static backdrop settles to 0 tiles");
}

static void test_visibility_toggle_recomposites()
{
    printf("--- hiding a layer recomposites the stale tiles ---\n");
    Comp c;
    CountingPresenter p;
    // Layer 2 (opaque) covers a pixel over layer 1.
    c.beginFrame();
    c.layers[1].setPixel(5, 5, Pixel4(4));
    c.layers[2].setPixel(5, 5, Pixel4(9)); // tile (0,0), on top
    c.compositeDirty();
    ASSERT(c.output.getPixel(5, 5).value == 9, "top layer shows while visible");
    c.present(p);
    c.endFrame();

    // Hide layer 2: its tiles must recomposite from lower layers.
    c.setVisible(2, false);
    c.beginFrame();
    c.compositeDirty();
    ASSERT(c.output.getPixel(5, 5).value == 4, "hidden layer's tile falls back to layer below");
    c.present(p);
    c.endFrame();

    // Un-hide: the tile repaints the top layer again.
    c.setVisible(2, true);
    c.beginFrame();
    c.compositeDirty();
    ASSERT(c.output.getPixel(5, 5).value == 9, "un-hiding repaints the top layer");
    c.present(p);
    c.endFrame();
}

int main()
{
    printf("compositor_dirty_test\n");
    printf("=====================\n");

    test_present_once_per_frame();
    test_only_dirty_tiles_written();
    test_per_layer_tint_applied();
    test_source_15_transparent_regardless_of_tint();
    test_static_layer_settles_to_zero();
    test_backdrop_restore_erases_moved_content();
    test_nonzero_backdrop_composites_and_settles();
    test_visibility_toggle_recomposites();

    printf("\nResults: %d passed, %d failed\n", passes, failures);
    return failures == 0 ? 0 : 1;
}
