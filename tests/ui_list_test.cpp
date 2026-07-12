// List widget unit test (Phase 3a, #121): C_List rewritten as a data-only
// ListComponent + ListSystem on the upstream ui ECS.
//
// The list is presentation-only: items arrive pre-stringified (the getString
// projection now lives at the scene/host edge), selection is plain state, and a
// ListSystem draws it to an ICanvas<Pixel4> via TextRenderer<Pixel4>.
//
// Two pure seams are pinned without a canvas — selection clamping and the marquee
// advance — then a render pass asserts the selected row and its highlight land on
// a real Canvas4.
#include <enjin2/ui/component.hpp>
#include <enjin2/ui/components.hpp>
#include <enjin2/ui/world.hpp>
#include <enjin2/ui/theme.hpp>
#include <enjin2/ui/widgets/list.hpp>
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

using ListWorld = World<32, ListComponent, PositionComponent, SizeComponent>;

// Selection walks the item range and clamps at both ends — it never wraps and
// never runs off the vector (the old C_List guarded exactly these bounds).
static void test_selection_clamps() {
    ListComponent list({"alpha", "beta", "gamma"});
    ASSERT(list.currentSelectionIndex() == 0, "list: starts at index 0");
    ASSERT(list.currentSelection() == "alpha", "list: selection maps to item string");

    list.moveUp(); // already at top
    ASSERT(list.currentSelectionIndex() == 0, "list: moveUp clamps at top");

    list.moveDown();
    list.moveDown();
    ASSERT(list.currentSelectionIndex() == 2, "list: moveDown advances");
    ASSERT(list.currentSelection() == "gamma", "list: selection follows index");

    list.moveDown(); // already at bottom
    ASSERT(list.currentSelectionIndex() == 2, "list: moveDown clamps at bottom");
}

// setCurrentSelection is host-driven and must saturate rather than index OOB.
static void test_set_selection_saturates() {
    ListComponent list({"a", "b", "c"});
    list.setCurrentSelection(1);
    ASSERT(list.currentSelectionIndex() == 1, "list: setCurrentSelection sets index");
    list.setCurrentSelection(99);
    ASSERT(list.currentSelectionIndex() == 2, "list: setCurrentSelection saturates high");
    list.setCurrentSelection(-5);
    ASSERT(list.currentSelectionIndex() == 0, "list: setCurrentSelection saturates low");
}

// Replacing the items must not leave the selection dangling past the new end.
static void test_update_items_reclamps() {
    ListComponent list({"a", "b", "c", "d"});
    list.setCurrentSelection(3);
    list.updateItems({"x", "y"});
    ASSERT(list.itemCount() == 2, "list: updateItems swaps the backing vector");
    ASSERT(list.currentSelectionIndex() == 1, "list: updateItems reclamps selection");
    list.updateItems({});
    ASSERT(list.itemCount() == 0, "list: updateItems accepts an empty list");
    ASSERT(list.currentSelectionIndex() == 0, "list: empty list pins index at 0");
}

// Marquee advance is the time-based seam. When the label fits, the offset stays
// pinned at zero; when it overflows, the offset only starts creeping after the
// start delay, then wraps back to zero after the end delay.
static void test_marquee_advance() {
    ListComponent list({"a-very-long-label-that-overflows"});
    list.setMarqueeTiming(/*start*/ 600, /*speed*/ 50, /*end*/ 1000);

    // Fits: never scrolls, offset held at zero even across a long tick.
    list.advanceMarquee(5000.0f, /*textWidth*/ 20, /*maxTextWidth*/ 40);
    ASSERT(list.marqueeOffset() == 0, "list: marquee stays home when text fits");

    // Overflows but still inside the start delay: no movement yet.
    list.advanceMarquee(500.0f, /*textWidth*/ 100, /*maxTextWidth*/ 40);
    ASSERT(list.marqueeOffset() == 0, "list: marquee waits out the start delay");

    // One more tick crosses the start delay; the same call, being past the speed
    // threshold, also takes the first pixel step (matching C_List::Update).
    list.advanceMarquee(200.0f, 100, 40);
    ASSERT(list.marqueeOffset() >= 1, "list: marquee creeps after the start delay");

    // The overflow extent is textWidth - maxTextWidth = 60. Step (one pixel per
    // speed tick) until the offset passes it and enters the end-dwell.
    for (int i = 0; i < 200 && list.marqueeOffset() <= 60; ++i) {
        list.advanceMarquee(60.0f, 100, 40);
    }
    ASSERT(list.marqueeOffset() > 60, "list: marquee reached the overflow end");

    // Dwelling at the end past the end delay wraps the offset home.
    list.advanceMarquee(1100.0f, 100, 40);
    ASSERT(list.marqueeOffset() == 0, "list: marquee wraps back home after end delay");
}

// End-to-end render: a list on a Canvas4 must paint its selected row — both the
// highlight bar (a bright pixel where the accent fill lands) and glyph ink.
static void test_render_draws_selection() {
    ListWorld world;
    Canvas4<64, 64> canvas;
    ListSystem<ListWorld, Canvas4<64, 64>> system(&world, &canvas);

    Entity e = world.create();
    world.add<ListComponent>(e, std::vector<std::string>{"one", "two", "three"});
    world.add<PositionComponent>(e, Point(0, 0));
    world.add<SizeComponent>(e, Size(64, 64));
    world.get<ListComponent>(e)->setCurrentSelection(1);

    canvas.clear(Colors::BLACK);
    system.update(0.016f);

    // Scan the canvas: a rendered list must have put *some* non-black ink down,
    // and the accent-colored highlight must reach full brightness somewhere.
    bool anyInk = false;
    bool anyHighlight = false;
    for (int16_t y = 0; y < 64; ++y) {
        for (int16_t x = 0; x < 64; ++x) {
            uint8_t v = canvas.getPixel(x, y);
            if (v != 0) anyInk = true;
            if (v == static_cast<uint8_t>(kDefaultTheme.accent)) anyHighlight = true;
        }
    }
    ASSERT(anyInk, "list: render puts ink on the canvas");
    ASSERT(anyHighlight, "list: render paints the accent selection highlight");
}

// A list with no items must render as a no-op rather than indexing an empty vector.
static void test_render_empty_is_safe() {
    ListWorld world;
    Canvas4<32, 32> canvas;
    ListSystem<ListWorld, Canvas4<32, 32>> system(&world, &canvas);

    Entity e = world.create();
    world.add<ListComponent>(e, std::vector<std::string>{});
    world.add<PositionComponent>(e, Point(0, 0));
    world.add<SizeComponent>(e, Size(32, 32));

    canvas.clear(Colors::BLACK);
    system.update(0.016f); // must not crash / read OOB

    bool anyInk = false;
    for (int16_t y = 0; y < 32; ++y)
        for (int16_t x = 0; x < 32; ++x)
            if (canvas.getPixel(x, y) != 0) anyInk = true;
    ASSERT(!anyInk, "list: empty list draws nothing");
}

int main() {
    test_selection_clamps();
    test_set_selection_saturates();
    test_update_items_reclamps();
    test_marquee_advance();
    test_render_draws_selection();
    test_render_empty_is_safe();

    printf("\n%d passed, %d failed\n", passes, failures);
    return failures == 0 ? 0 : 1;
}
