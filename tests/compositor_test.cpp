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

    static_assert(enjin2::ENJIN_LAYER_COUNT == 4,
                  "ENJIN_LAYER_COUNT must be 4 in default configuration");

    ASSERT(enjin2::ENJIN_LAYER_COUNT == 4,
           "ENJIN_LAYER_COUNT runtime value should be 4");
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

    printf("\nResults: %d passed, %d failed\n", passes, failures);

    return failures == 0 ? 0 : 1;
}
