// Icon widget unit test (Phase 3a, #121): C_Sprite-backed Icon rewritten as a
// data-only IconComponent + IconSystem that blits a grayscale bitmap.
//
// The transparency test is the seam, pinned pure on a tiny hand-built bitmap:
// in-range non-matte pixels are opaque, matte pixels and out-of-range reads are
// clear. A render pass then blits onto a Canvas4 and asserts opaque pixels land at
// the icon's origin while matte pixels leave the background showing through.
#include <enjin2/ui/component.hpp>
#include <enjin2/ui/components.hpp>
#include <enjin2/ui/world.hpp>
#include <enjin2/ui/widgets/icon.hpp>
#include <enjin2/graphics/canvas.hpp>
#include <cstdint>
#include <cstdio>

using namespace enjin2;

static int passes = 0;
static int failures = 0;

#define ASSERT(cond, msg)                                                     \
    do {                                                                      \
        if (!(cond)) { fprintf(stderr, "FAIL: %s\n", msg); failures++; }      \
        else { printf("PASS: %s\n", msg); passes++; }                         \
    } while (0)

// A 2x2 bitmap: an opaque white pixel at (0,0), the rest transparent (matte 16).
static const uint8_t kDot[4] = {
    15, 16,
    16, 16,
};

using IconWorld = World<32, IconComponent, PositionComponent>;

// sampleAt / isOpaqueAt read the buffer and treat both the matte value and any
// out-of-range coordinate as clear — no read ever runs off the buffer.
static void test_transparency_seam() {
    IconComponent icon(kDot, 2, 2);
    ASSERT(icon.sampleAt(0, 0) == 15, "icon: samples the stored pixel value");
    ASSERT(icon.isOpaqueAt(0, 0), "icon: a non-matte pixel is opaque");
    ASSERT(!icon.isOpaqueAt(1, 0), "icon: a matte pixel is transparent");
    ASSERT(!icon.isOpaqueAt(5, 5), "icon: out-of-range reads are transparent");
    ASSERT(icon.sampleAt(-1, 0) == icon.matte, "icon: out-of-range sample returns the matte");
}

// End-to-end blit: the opaque pixel lands at the icon's origin, and the matte
// pixels leave the (non-black) background untouched.
static void test_render_blits_with_matte() {
    IconWorld world;
    Canvas4<16, 16> canvas;
    IconSystem<IconWorld, Canvas4<16, 16>> system(&world, &canvas);

    Entity e = world.create();
    world.add<IconComponent>(e, kDot, 2, 2);
    world.add<PositionComponent>(e, Point(3, 4));

    canvas.clear(Pixel4(2)); // distinctive background so matte skips are visible
    system.update(0.016f);

    ASSERT(canvas.getPixel(3, 4) == 15, "icon: opaque pixel blits at the origin");
    ASSERT(canvas.getPixel(4, 4) == 2, "icon: matte pixel leaves the background");
    ASSERT(canvas.getPixel(3, 5) == 2, "icon: matte pixel below leaves the background");
}

// A hidden icon draws nothing.
static void test_render_hidden_is_noop() {
    IconWorld world;
    Canvas4<16, 16> canvas;
    IconSystem<IconWorld, Canvas4<16, 16>> system(&world, &canvas);

    Entity e = world.create();
    auto* icon = world.add<IconComponent>(e, kDot, 2, 2);
    icon->setVisible(false);
    world.add<PositionComponent>(e, Point(0, 0));

    canvas.clear(Colors::BLACK);
    system.update(0.016f);

    bool anyInk = false;
    for (int16_t y = 0; y < 16; ++y)
        for (int16_t x = 0; x < 16; ++x)
            if (canvas.getPixel(x, y) != 0) anyInk = true;
    ASSERT(!anyInk, "icon: a hidden icon draws nothing");
}

// A null-bitmap icon is a safe no-op rather than a crash.
static void test_render_null_bitmap_is_safe() {
    IconWorld world;
    Canvas4<16, 16> canvas;
    IconSystem<IconWorld, Canvas4<16, 16>> system(&world, &canvas);

    Entity e = world.create();
    world.add<IconComponent>(e); // no bitmap
    world.add<PositionComponent>(e, Point(0, 0));

    canvas.clear(Colors::BLACK);
    system.update(0.016f); // must not dereference a null buffer

    bool anyInk = false;
    for (int16_t y = 0; y < 16; ++y)
        for (int16_t x = 0; x < 16; ++x)
            if (canvas.getPixel(x, y) != 0) anyInk = true;
    ASSERT(!anyInk, "icon: a null bitmap draws nothing");
}

int main() {
    test_transparency_seam();
    test_render_blits_with_matte();
    test_render_hidden_is_noop();
    test_render_null_bitmap_is_safe();

    printf("\n%d passed, %d failed\n", passes, failures);
    return failures == 0 ? 0 : 1;
}
