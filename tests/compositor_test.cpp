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
                  "ENJIN_LAYER_COUNT must be 5 in default configuration (Phase 47: debug layer added)");

    ASSERT(enjin2::ENJIN_LAYER_COUNT == 5,
           "ENJIN_LAYER_COUNT runtime value should be 5 (Phase 47: debug layer added)");
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
    // Layer 1 has color 8 at (2,2) — should override layer 0
    comp.layers[1].setPixel(2, 2, Pixel4(8));
    comp.composite();

    ASSERT(comp.output.getPixel(2, 2).value == 8,
           "layer override: output (2,2) should be 8 (layer 1 overrides layer 0)");
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
    // Layer 2: color 9 at (1,1) — highest visible
    comp.layers[2].setPixel(1, 1, Pixel4(9));
    // Layer 3: leave transparent at (1,1)
    comp.composite();

    ASSERT(comp.output.getPixel(1, 1).value == 9,
           "multi-layer: output (1,1) should be 9 (layer 2 highest non-transparent)");
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
    comp.layers[3].setPixel(4, 4, Pixel4(9));

    comp.composite();

    // Highest layer (3) wins in painter's order
    ASSERT(comp.output.getPixel(4, 4).value == 9,
           "all four layers: output (4,4) should be 9 (layer 3 wins)");
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
    comp.layers[2].setPixel(7, 7, Pixel4(10));

    comp.composite();

    // Verify layer buffers are unchanged after compositing
    ASSERT(comp.layers[0].getPixel(0, 0).value == 2,
           "preserves: layer 0 (0,0) should still be 2 after composite");
    ASSERT(comp.layers[1].getPixel(3, 3).value == 6,
           "preserves: layer 1 (3,3) should still be 6 after composite");
    ASSERT(comp.layers[2].getPixel(7, 7).value == 10,
           "preserves: layer 2 (7,7) should still be 10 after composite");
    // Layer 1 untouched pixel should still be 15 (transparent)
    ASSERT(comp.layers[1].getPixel(0, 0).value == 15,
           "preserves: layer 1 (0,0) should still be 15 (transparent) after composite");
}

// ============================================================
// main
// ============================================================
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

    printf("\nResults: %d passed, %d failed\n", passes, failures);

    return failures == 0 ? 0 : 1;
}
