#include <enjin2/graphics/layer_compositor.hpp>
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

// ============================================================
// test_layer_count_constexpr
// ============================================================
static void test_layer_count_constexpr()
{
    printf("--- layer count constexpr ---\n");

    static_assert(enjin2::ENJIN_LAYER_COUNT == 5,
                  "ENJIN_LAYER_COUNT must be 5 on every host (3 applet + UI + System)");
    static_assert(enjin2::ENJIN_LUA_LAYER_COUNT == 3, "an applet has 3 layers");
    static_assert(enjin2::ENJIN_UI_LAYER == 3 && enjin2::ENJIN_SYSTEM_LAYER == 4,
                  "the UI layer, then the System layer on top");

    ASSERT(enjin2::ENJIN_LAYER_COUNT == 5,
           "ENJIN_LAYER_COUNT runtime value should be 5");
}

// ============================================================
// test_clear_all
// ============================================================
static void test_clear_all()
{
    printf("--- clearAll ---\n");

    LayerCompositor<16, 16> comp;
    comp.clearAll();

    // Layer 0 should be index 0 (black)
    ASSERT(comp.layers[0].getPixel(0, 0).value == 0,
           "clearAll: layer 0 pixel (0,0) should be 0 (black)");
    ASSERT(comp.layers[0].getPixel(8, 8).value == 0,
           "clearAll: layer 0 pixel (8,8) should be 0 (black)");

    // Layers 1-3 should be index 15 (transparent)
    ASSERT(comp.layers[1].getPixel(0, 0).value == 15,
           "clearAll: layer 1 pixel (0,0) should be 15 (transparent)");
    ASSERT(comp.layers[2].getPixel(0, 0).value == 15,
           "clearAll: layer 2 pixel (0,0) should be 15 (transparent)");
    ASSERT(comp.layers[3].getPixel(0, 0).value == 15,
           "clearAll: layer 3 pixel (0,0) should be 15 (transparent)");
    ASSERT(comp.layers[4].getPixel(0, 0).value == 15,
           "clearAll: layer 4 pixel (0,0) should be 15 (transparent)");
}

// ============================================================
// test_single_layer_composition
// ============================================================
static void test_single_layer_composition()
{
    printf("--- single layer composition ---\n");

    LayerCompositor<16, 16> comp;
    comp.clearAll();

    // Draw color 3 at (4,4) on layer 0
    comp.layers[0].setPixel(4, 4, Pixel4(3));
    comp.composite();

    ASSERT(comp.output.getPixel(4, 4).value == 3,
           "single layer: output (4,4) should be 3 after compositing layer 0");

    // Untouched pixel on layer 0 should still be 0
    ASSERT(comp.output.getPixel(0, 0).value == 0,
           "single layer: output (0,0) should be 0 (untouched)");
}

// ============================================================
// test_layer_override
// ============================================================
static void test_layer_override()
{
    printf("--- layer override (painter's order) ---\n");

    LayerCompositor<16, 16> comp;
    comp.clearAll();

    // Layer 0 has color 5 at (2,2)
    comp.layers[0].setPixel(2, 2, Pixel4(5));
    // Layer 1 has color 6 at (2,2) — should override layer 0
    comp.layers[1].setPixel(2, 2, Pixel4(6));
    comp.composite();

    ASSERT(comp.output.getPixel(2, 2).value == 6,
           "layer override: output (2,2) should be 6 (layer 1 overrides layer 0)");
}

// ============================================================
// test_transparency_passthrough
// ============================================================
static void test_transparency_passthrough()
{
    printf("--- transparency passthrough ---\n");

    LayerCompositor<16, 16> comp;
    comp.clearAll();

    // Layer 0 has color 7 at (3,3)
    comp.layers[0].setPixel(3, 3, Pixel4(7));
    // Layer 1 at (3,3) remains 15 (transparent — default after clearAll)
    // Layer 1 is already 15 everywhere after clearAll
    comp.composite();

    ASSERT(comp.output.getPixel(3, 3).value == 7,
           "transparency: output (3,3) should be 7 (layer 0 shows through layer 1 index-15)");
}

// ============================================================
// test_multi_layer_stack
// ============================================================
static void test_multi_layer_stack()
{
    printf("--- multi-layer stack ---\n");

    LayerCompositor<16, 16> comp;
    comp.clearAll();

    // Layer 0: color 1 at (1,1)
    comp.layers[0].setPixel(1, 1, Pixel4(1));
    // Layer 1: leave transparent at (1,1)
    // Layer 2: color 6 at (1,1) — highest visible
    comp.layers[2].setPixel(1, 1, Pixel4(6));
    // Layer 3: leave transparent at (1,1)
    comp.composite();

    ASSERT(comp.output.getPixel(1, 1).value == 6,
           "multi-layer: output (1,1) should be 6 (layer 2 highest non-transparent)");
}

// ============================================================
// test_layer_visibility
// ============================================================
static void test_layer_visibility()
{
    printf("--- layer visibility ---\n");

    LayerCompositor<16, 16> comp;
    comp.clearAll();

    // Layer 1 has color 3 at (0,0)
    comp.layers[1].setPixel(0, 0, Pixel4(3));

    // Hide layer 1 — output should show layer 0 (black = 0)
    comp.visible[1] = false;
    comp.composite();

    ASSERT(comp.output.getPixel(0, 0).value == 0,
           "visibility: output (0,0) should be 0 when layer 1 is hidden");

    // Re-enable layer 1 — output should now show color 3
    comp.visible[1] = true;
    comp.composite();

    ASSERT(comp.output.getPixel(0, 0).value == 3,
           "visibility: output (0,0) should be 3 after layer 1 is made visible again");
}

// ============================================================
// test_mixed_nibble_transparency
// ============================================================
static void test_mixed_nibble_transparency()
{
    printf("--- mixed nibble transparency ---\n");

    LayerCompositor<16, 16> comp;
    comp.clearAll();

    // (0,0) and (1,0) share a packed byte (even/odd x in same byte)
    // Layer 0: color 5 at both (0,0) and (1,0)
    comp.layers[0].setPixel(0, 0, Pixel4(5));
    comp.layers[0].setPixel(1, 0, Pixel4(5));
    // Layer 1: color 3 at (0,0) only; (1,0) stays transparent (15)
    comp.layers[1].setPixel(0, 0, Pixel4(3));
    // Layer 1 at (1,0) is already 15 after clearAll

    comp.composite();

    ASSERT(comp.output.getPixel(0, 0).value == 3,
           "mixed nibble: output (0,0) should be 3 (layer 1 overrides)");
    ASSERT(comp.output.getPixel(1, 0).value == 5,
           "mixed nibble: output (1,0) should be 5 (layer 0 shows through transparent nibble)");
}

// ============================================================
// test_layer0_hidden
// ============================================================
static void test_layer0_hidden()
{
    printf("--- layer 0 hidden ---\n");

    LayerCompositor<16, 16> comp;
    comp.clearAll();

    // Layer 0 has color 7 at (2,2)
    comp.layers[0].setPixel(2, 2, Pixel4(7));
    // Layer 1 has color 3 at (5,5)
    comp.layers[1].setPixel(5, 5, Pixel4(3));

    // Hide layer 0 — output base should be cleared to 0 (black)
    comp.visible[0] = false;
    comp.composite();

    ASSERT(comp.output.getPixel(2, 2).value == 0,
           "layer0 hidden: output (2,2) should be 0 (layer 0 content not visible)");
    ASSERT(comp.output.getPixel(5, 5).value == 3,
           "layer0 hidden: output (5,5) should be 3 (layer 1 still visible)");
    ASSERT(comp.output.getPixel(0, 0).value == 0,
           "layer0 hidden: output (0,0) should be 0 (black base)");
}

// ============================================================
// test_all_four_layers_same_pixel
// ============================================================
static void test_all_four_layers_same_pixel()
{
    printf("--- all four layers same pixel ---\n");

    LayerCompositor<16, 16> comp;
    comp.clearAll();

    // Each layer has a different color at (4,4)
    comp.layers[0].setPixel(4, 4, Pixel4(1));
    comp.layers[1].setPixel(4, 4, Pixel4(3));
    comp.layers[2].setPixel(4, 4, Pixel4(7));
    comp.layers[3].setPixel(4, 4, Pixel4(5));

    comp.composite();

    // Highest layer (3) wins in painter's order
    ASSERT(comp.output.getPixel(4, 4).value == 5,
           "all four layers: output (4,4) should be 5 (layer 3 wins)");
}

// ============================================================
// test_composite_preserves_layer_buffers
// ============================================================
static void test_composite_preserves_layer_buffers()
{
    printf("--- composite preserves layer buffers ---\n");

    LayerCompositor<16, 16> comp;
    comp.clearAll();

    // Set distinct pixels on different layers
    comp.layers[0].setPixel(0, 0, Pixel4(2));
    comp.layers[1].setPixel(3, 3, Pixel4(6));
    comp.layers[2].setPixel(7, 7, Pixel4(4));

    comp.composite();

    // Verify layer buffers are unchanged after compositing
    ASSERT(comp.layers[0].getPixel(0, 0).value == 2,
           "preserves: layer 0 (0,0) should still be 2 after composite");
    ASSERT(comp.layers[1].getPixel(3, 3).value == 6,
           "preserves: layer 1 (3,3) should still be 6 after composite");
    ASSERT(comp.layers[2].getPixel(7, 7).value == 4,
           "preserves: layer 2 (7,7) should still be 4 after composite");
    // Layer 1 untouched pixel should still be 15 (transparent)
    ASSERT(comp.layers[1].getPixel(0, 0).value == 15,
           "preserves: layer 1 (0,0) should still be 15 (transparent) after composite");
}

// ============================================================
// main
// ============================================================
// ============================================================
// The System layer (ENJIN_SYSTEM_LAYER): hidden until shown, on top of every
// layer and of the cover, and no cost while hidden.
// ============================================================
using Sys = LayerCompositor<64, 64>;

static bool tileDirty(const Sys& c, uint16_t tx, uint16_t ty)
{
    const uint16_t t = static_cast<uint16_t>(ty * Sys::TILES_X + tx);
    return (c.mergedDirty[t >> 3] & (1u << (t & 7))) != 0;
}

static void settle(Sys& c)
{
    c.beginFrame();
    c.compositeDirty();
    c.endFrame();
}

static void test_system_layer_hidden_by_default()
{
    printf("--- System layer: hidden by default ---\n");
    static Sys c;
    ASSERT(!c.visible[ENJIN_SYSTEM_LAYER], "the System layer starts hidden");
    for (uint8_t l = 0; l < ENJIN_SYSTEM_LAYER; ++l)
        ASSERT(c.visible[l], "every other layer starts visible");
    c.layers[0].fillRect(0, 0, 64, 64, Pixel4(2));
    c.layers[ENJIN_SYSTEM_LAYER].fillRect(0, 0, 8, 8, Pixel4(6));
    c.compositeDirty();
    ASSERT(c.output.getPixel(2, 2).value == 2, "a hidden System layer shows nothing");
    c.composite();
    ASSERT(c.output.getPixel(2, 2).value == 2, "nor in the full composite");
}

static void test_system_layer_no_cost_hidden()
{
    printf("--- System layer: no cost while hidden ---\n");
    static Sys c;
    c.resetDirtyState();
    settle(c);
    // Drawing on, clearing and invalidating the hidden layer recomposites nothing.
    c.beginFrame();
    c.layers[ENJIN_SYSTEM_LAYER].fillRect(10, 10, 20, 20, Pixel4(5));
    c.layers[ENJIN_SYSTEM_LAYER].clear(Pixel4(PALETTE_TRANSPARENT));
    c.invalidateLayer(ENJIN_SYSTEM_LAYER);
    c.compositeDirty();
    ASSERT(c.mergedDirtyCount() == 0, "a hidden System layer dirties no tile");
    c.endFrame();
}

static void test_system_layer_show_hide_tiles()
{
    printf("--- System layer: showing and hiding recomposite its tiles ---\n");
    static Sys c;
    c.resetDirtyState();
    c.layers[0].fillRect(0, 0, 64, 64, Pixel4(1));
    settle(c);

    // Draw while hidden, then show: the tiles it has something on go out.
    c.beginFrame();
    c.layers[ENJIN_SYSTEM_LAYER].fillRect(20, 20, 4, 4, Pixel4(7));  // tile (1,1)
    c.setVisible(ENJIN_SYSTEM_LAYER, true);
    c.compositeDirty();
    ASSERT(c.output.getPixel(21, 21).value == 7, "shown: its pixels are on top");
    ASSERT(c.mergedDirtyCount() == 1, "showing recomposites only the tiles it has something on");
    ASSERT(tileDirty(c, 1, 1), "the tile under its pixels");
    c.endFrame();

    // Hide: the same tiles recomposite from the layers below.
    c.beginFrame();
    c.setVisible(ENJIN_SYSTEM_LAYER, false);
    c.compositeDirty();
    ASSERT(c.output.getPixel(21, 21).value == 1, "hidden: the layers below show again");
    ASSERT(c.mergedDirtyCount() == 1 && tileDirty(c, 1, 1), "hiding recomposites the same tile");
    c.endFrame();

    // Show, then clear it in the same frame as it hides: what it showed goes.
    c.setVisible(ENJIN_SYSTEM_LAYER, true);
    settle(c);
    c.beginFrame();
    c.layers[ENJIN_SYSTEM_LAYER].fillRect(40, 40, 4, 4, Pixel4(6));  // tile (2,2)
    c.compositeDirty();
    ASSERT(c.output.getPixel(41, 41).value == 6, "drawn while visible");
    c.endFrame();
    c.beginFrame();
    c.layers[ENJIN_SYSTEM_LAYER].fillRect(40, 40, 4, 4, Pixel4(PALETTE_TRANSPARENT));
    c.layers[ENJIN_SYSTEM_LAYER].fillRect(20, 20, 4, 4, Pixel4(PALETTE_TRANSPARENT));
    c.setVisible(ENJIN_SYSTEM_LAYER, false);
    c.compositeDirty();
    ASSERT(c.output.getPixel(41, 41).value == 1 && c.output.getPixel(21, 21).value == 1,
           "cleared, then hidden: nothing of it stays");
    c.endFrame();
    c.beginFrame();
    c.compositeDirty();
    ASSERT(c.mergedDirtyCount() == 0, "and the next frame costs nothing");
    c.endFrame();
}

static void test_system_layer_above_cover()
{
    printf("--- System layer: above the cover ---\n");
    static Sys c;
    static Sys::Layer src;
    c.resetDirtyState();
    src.clear(Pixel4(4));
    c.layers[0].fillRect(0, 0, 64, 64, Pixel4(1));
    c.layers[ENJIN_UI_LAYER].fillRect(0, 0, 64, 64, Pixel4(2));
    c.layers[ENJIN_SYSTEM_LAYER].fillRect(8, 8, 4, 4, Pixel4(7));
    c.setVisible(ENJIN_SYSTEM_LAYER, true);
    Sys::Mask whole;
    whole.fill();
    c.setCoverSource(&src);
    c.setCoverBand(0, whole, Sys::CoverFill::solid(Pixel4(3)));
    Sys::Mask disc;
    disc.disc(32, 32, 20);
    c.setCoverBand(1, disc, Sys::CoverFill::source());
    c.compositeDirty();
    ASSERT(c.output.getPixel(9, 9).value == 7, "the System layer is over a solid band");
    ASSERT(c.output.getPixel(0, 0).value == 3, "the cover is over the UI layer");
    c.layers[ENJIN_SYSTEM_LAYER].fillRect(30, 30, 2, 2, Pixel4(6));
    c.compositeDirty();
    ASSERT(c.output.getPixel(30, 30).value == 6 && c.output.getPixel(31, 31).value == 6,
           "and over a source band");
    ASSERT(c.output.getPixel(33, 33).value == 4, "the source band shows where it is clear");
    c.clearCover();
    c.compositeDirty();
    ASSERT(c.output.getPixel(9, 9).value == 7 && c.output.getPixel(0, 0).value == 2,
           "with no cover: the System layer over the UI layer");

    // A tint on it applies in the masked path too.
    c.setCoverBand(0, whole, Sys::CoverFill::solid(Pixel4(3)));
    c.setTint(ENJIN_SYSTEM_LAYER, Remap::solid(5));
    c.compositeDirty();
    ASSERT(c.output.getPixel(9, 9).value == 5, "its tint applies over the cover");
}

int main()
{
    printf("compositor_test\n");
    printf("===============\n");

    test_layer_count_constexpr();
    test_clear_all();
    test_single_layer_composition();
    test_layer_override();
    test_transparency_passthrough();
    test_multi_layer_stack();
    test_layer_visibility();
    test_mixed_nibble_transparency();
    test_layer0_hidden();
    test_all_four_layers_same_pixel();
    test_composite_preserves_layer_buffers();
    test_system_layer_hidden_by_default();
    test_system_layer_no_cost_hidden();
    test_system_layer_show_hide_tiles();
    test_system_layer_above_cover();

    printf("\nResults: %d passed, %d failed\n", passes, failures);

    return failures == 0 ? 0 : 1;
}
