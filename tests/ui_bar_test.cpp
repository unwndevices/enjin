// Bar widget unit test (unwn #206): a lightweight linear level meter as a
// data-only BarComponent + BarSystem, the straight-line sibling of the gauge.
//
// The value->geometry mapping is the seam, pinned pure: setValue clamps into
// [0, 1], and fillRegion places the filled span for both orientations
// (horizontal grows left->right, vertical grows bottom->top). A render pass
// then draws onto a Canvas4 and asserts the filled span inks and an empty bar
// is a no-op.
#include <enjin2/ui/component.hpp>
#include <enjin2/ui/components.hpp>
#include <enjin2/ui/world.hpp>
#include <enjin2/ui/widgets/bar.hpp>
#include <enjin2/graphics/canvas.hpp>
#include <cstdio>

using namespace enjin2;

static int passes = 0;
static int failures = 0;

#define ASSERT(cond, msg)                                                     \
    do {                                                                      \
        if (!(cond)) { fprintf(stderr, "FAIL: %s\n", msg); failures++; }      \
        else { printf("PASS: %s\n", msg); passes++; }                         \
    } while (0)

using BarWorld = World<8, BarComponent, PositionComponent>;

// setValue saturates into [0, 1] — an out-of-range live value never paints past
// the meter's ends.
static void test_value_clamps() {
    BarComponent b(10, 1);
    b.setValue(2.0f);
    ASSERT(b.value() == 1.0f, "bar: clamps high to 1");
    b.setValue(-2.0f);
    ASSERT(b.value() == 0.0f, "bar: clamps low to 0");
    b.setValue(0.5f);
    ASSERT(b.value() == 0.5f, "bar: keeps an in-range value");
}

// Horizontal fill grows left->right from the origin.
static void test_horizontal_geometry() {
    BarComponent b(10, 3, BarOrientation::Horizontal);
    b.setValue(0.5f);
    Rect r = b.fillRegion();
    ASSERT(r.x == 0 && r.width == 5, "bar: horizontal fills half the length from the left");
    ASSERT(r.y == 0 && r.height == 3, "bar: horizontal fill spans the thickness");

    b.setValue(1.0f);
    ASSERT(b.fillRegion().width == 10, "bar: a full horizontal bar fills the whole length");
}

// Vertical fill grows bottom->top: the filled band hugs the meter's lower end.
static void test_vertical_geometry() {
    BarComponent b(10, 3, BarOrientation::Vertical);
    b.setValue(0.5f);
    Rect r = b.fillRegion();
    ASSERT(r.y == 5 && r.height == 5, "bar: vertical fills the lower half");
    ASSERT(r.x == 0 && r.width == 3, "bar: vertical fill spans the thickness");
}

// End-to-end render: a full horizontal bar inks its whole span at the entity
// position and nothing beyond it.
static void test_render_draws_filled_span() {
    BarWorld world;
    Canvas4<20, 20> canvas;
    BarSystem<BarWorld, Canvas4<20, 20>> system(&world, &canvas);

    Entity e = world.create();
    auto* bar = world.add<BarComponent>(e, static_cast<uint16_t>(10), static_cast<uint16_t>(3),
                                        BarOrientation::Horizontal, Pixel4(15));
    bar->setValue(1.0f);
    world.add<PositionComponent>(e, Point(2, 2));

    canvas.clear(Colors::BLACK);
    system.update(0.0f);

    ASSERT((uint8_t)canvas.getPixel(2, 2) == 15, "bar: render inks the span origin");
    ASSERT((uint8_t)canvas.getPixel(11, 4) == 15, "bar: render inks the far end of the span");
    ASSERT((uint8_t)canvas.getPixel(12, 2) == 0, "bar: render stops at the length");
    ASSERT((uint8_t)canvas.getPixel(2, 5) == 0, "bar: render stops at the thickness");
}

// A value-0 bar draws nothing.
static void test_render_empty_is_safe() {
    BarWorld world;
    Canvas4<20, 20> canvas;
    BarSystem<BarWorld, Canvas4<20, 20>> system(&world, &canvas);

    Entity e = world.create();
    world.add<BarComponent>(e, static_cast<uint16_t>(10), static_cast<uint16_t>(3),
                            BarOrientation::Horizontal, Pixel4(15)); // value 0
    world.add<PositionComponent>(e, Point(2, 2));

    canvas.clear(Colors::BLACK);
    system.update(0.0f);

    bool anyInk = false;
    for (int16_t y = 0; y < 20; ++y)
        for (int16_t x = 0; x < 20; ++x)
            if ((uint8_t)canvas.getPixel(x, y) != 0) anyInk = true;
    ASSERT(!anyInk, "bar: a value-0 bar draws nothing");
}

int main() {
    test_value_clamps();
    test_horizontal_geometry();
    test_vertical_geometry();
    test_render_draws_filled_span();
    test_render_empty_is_safe();

    printf("\n%d passed, %d failed\n", passes, failures);
    return failures == 0 ? 0 : 1;
}
