// Gauge widget unit test (Phase 3a, #121): C_FillUpGauge rewritten as a data-only
// GaugeComponent + GaugeSystem that draws a circular VU meter.
//
// The value->geometry mapping is the seam, pinned pure: setValue clamps into the
// mode's range, and fillRegion / levelLineY place the dithered band and the level
// line for both fill directions. A render pass then draws onto a Canvas4 and
// asserts the rim and some fill ink appear, and that an empty gauge is a no-op.
#include <enjin2/ui/component.hpp>
#include <enjin2/ui/components.hpp>
#include <enjin2/ui/world.hpp>
#include <enjin2/ui/widgets/gauge.hpp>
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

using GaugeWorld = World<32, GaugeComponent, PositionComponent>;

// setValue saturates into the mode's range; switching mode reclamps the value.
static void test_value_clamps_per_mode() {
    GaugeComponent uni(100, Pixel4(13), GaugeMode::Unidirectional);
    uni.setValue(2.0f);
    ASSERT(uni.value() == 1.0f, "gauge: unidirectional clamps high to 1");
    uni.setValue(-2.0f);
    ASSERT(uni.value() == 0.0f, "gauge: unidirectional clamps low to 0");

    GaugeComponent bi(100, Pixel4(13), GaugeMode::Bidirectional);
    bi.setValue(-2.0f);
    ASSERT(bi.value() == -1.0f, "gauge: bidirectional clamps low to -1");

    // A negative value is legal in bidirectional mode; switching to unidirectional
    // must reclamp it up to 0.
    bi.setMode(GaugeMode::Unidirectional);
    ASSERT(bi.value() == 0.0f, "gauge: mode switch reclamps the held value");
}

// Unidirectional fill grows up from the bottom: half value fills the lower half and
// the level line sits at the midpoint.
static void test_unidirectional_geometry() {
    GaugeComponent g(100, Pixel4(13), GaugeMode::Unidirectional);
    g.setValue(0.5f);
    Rect region = g.fillRegion();
    ASSERT(region.y == 50 && region.height == 50, "gauge: unidirectional fills the bottom half");
    ASSERT(region.width == 100, "gauge: fill spans the full width");
    ASSERT(g.levelLineY() == 50, "gauge: unidirectional level line is at the midpoint");
}

// Bidirectional fill grows out from the center: positive fills above the midline,
// negative below, and the level line tracks the boundary.
static void test_bidirectional_geometry() {
    GaugeComponent g(100, Pixel4(13), GaugeMode::Bidirectional);
    g.setValue(0.5f);
    Rect up = g.fillRegion();
    ASSERT(up.y == 25 && up.height == 25, "gauge: positive fills above the midline");
    ASSERT(g.levelLineY() == 25, "gauge: positive level line above the midline");

    g.setValue(-0.5f);
    Rect down = g.fillRegion();
    ASSERT(down.y == 50 && down.height == 25, "gauge: negative fills below the midline");
    ASSERT(g.levelLineY() == 75, "gauge: negative level line below the midline");
}

// End-to-end render: a half-full gauge inks its rim and lays some dithered fill.
static void test_render_draws_rim_and_fill() {
    GaugeWorld world;
    Canvas4<40, 40> canvas;
    GaugeSystem<GaugeWorld, Canvas4<40, 40>> system(&world, &canvas);

    Entity e = world.create();
    auto* gauge = world.add<GaugeComponent>(e, static_cast<uint16_t>(40), Pixel4(13),
                                            GaugeMode::Unidirectional);
    gauge->setValue(0.5f);
    world.add<PositionComponent>(e, Point(0, 0));

    canvas.clear(Colors::BLACK);
    system.update(0.016f);

    bool anyRim = false;
    bool anyFill = false;
    for (int16_t y = 0; y < 40; ++y) {
        for (int16_t x = 0; x < 40; ++x) {
            uint8_t v = canvas.getPixel(x, y);
            if (v == 13) anyRim = true;
            if (v == GaugeComponent::kFillValue) anyFill = true;
        }
    }
    ASSERT(anyRim, "gauge: render strokes the rim");
    ASSERT(anyFill, "gauge: render lays dithered fill");
}

// A zero-diameter gauge draws nothing rather than dividing by an empty circle.
static void test_render_empty_is_safe() {
    GaugeWorld world;
    Canvas4<16, 16> canvas;
    GaugeSystem<GaugeWorld, Canvas4<16, 16>> system(&world, &canvas);

    Entity e = world.create();
    world.add<GaugeComponent>(e); // diameter 0
    world.add<PositionComponent>(e, Point(0, 0));

    canvas.clear(Colors::BLACK);
    system.update(0.016f);

    bool anyInk = false;
    for (int16_t y = 0; y < 16; ++y)
        for (int16_t x = 0; x < 16; ++x)
            if (canvas.getPixel(x, y) != 0) anyInk = true;
    ASSERT(!anyInk, "gauge: a zero-diameter gauge draws nothing");
}

int main() {
    test_value_clamps_per_mode();
    test_unidirectional_geometry();
    test_bidirectional_geometry();
    test_render_draws_rim_and_fill();
    test_render_empty_is_safe();

    printf("\n%d passed, %d failed\n", passes, failures);
    return failures == 0 ? 0 : 1;
}
