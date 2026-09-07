// PopUp widget test (Phase 3a, #121): PopUpUI's modal rewritten as a data-only
// PopUpComponent + its system.
//
// The popup's auto-hide countdown (which accrues time and hides itself at the
// deadline) is the pure seam; a render pass then draws the popup card and
// confirms a hidden popup paints nothing.
//
// The former OverlayBg subtractive dim (OverlayComponent/OverlaySystem) was
// removed with presenter ticket #34: full-frame dimming is now expressed as a
// per-layer tint Remap on the compositor, not a canvas-walking ECS system.
#include <enjin2/ui/component.hpp>
#include <enjin2/ui/components.hpp>
#include <enjin2/ui/world.hpp>
#include <enjin2/ui/widgets/popup.hpp>
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

// setLines truncates each message to the popup's character cap.
static void test_popup_truncates_lines() {
    PopUpComponent popup;
    popup.setLines("this-line-is-way-too-long-to-fit", "short");
    ASSERT(popup.line1.size() == PopUpComponent::kMaxChars, "popup: long line truncated to the cap");
    ASSERT(popup.line2 == "short", "popup: short line kept whole");
}

// The auto-hide countdown accrues time while shown and hides the popup once the
// delay elapses; a manual popup (autoHide 0) stays put.
static void test_popup_auto_hide_seam() {
    PopUpComponent popup;
    popup.setIcon(PopUpComponent::Icon::Info);

    popup.show(/*autoHideMs*/ 100);
    ASSERT(popup.isVisible(), "popup: show makes it visible");
    popup.advance(40.0f);
    ASSERT(popup.isVisible(), "popup: still visible before the deadline");
    popup.advance(80.0f); // crosses 100ms -> elapsed >= autoHide latched
    popup.advance(1.0f);  // next tick observes the deadline and hides
    ASSERT(!popup.isVisible(), "popup: auto-hides past the deadline");

    PopUpComponent manual;
    manual.show(/*manual*/ 0);
    manual.advance(100000.0f);
    ASSERT(manual.isVisible(), "popup: a manual popup never auto-hides");
}

// End-to-end render: a shown popup draws its rim circle; a hidden one draws nothing.
static void test_popup_render() {
    using PopUpWorld = World<8, PopUpComponent, PositionComponent>;
    PopUpWorld world;
    Canvas4<64, 64> canvas;
    PopUpSystem<PopUpWorld, Canvas4<64, 64>> system(&world, &canvas);

    Entity e = world.create();
    auto* popup = world.add<PopUpComponent>(e);
    popup->radius = 20;
    popup->setLines("hi", "there");
    popup->setIcon(PopUpComponent::Icon::Warning);
    world.add<PositionComponent>(e, Point(32, 32)); // card center

    canvas.clear(Colors::BLACK);
    popup->show();
    system.update(0.016f);

    bool anyRim = false;
    for (int16_t y = 0; y < 64; ++y)
        for (int16_t x = 0; x < 64; ++x)
            if (canvas.getPixel(x, y) == 6) anyRim = true;
    ASSERT(anyRim, "popup: a shown popup strokes its rim");

    // Hide it and redraw onto a clean canvas: nothing should land.
    canvas.clear(Colors::BLACK);
    popup->hide();
    system.update(0.016f);
    bool anyInk = false;
    for (int16_t y = 0; y < 64; ++y)
        for (int16_t x = 0; x < 64; ++x)
            if (canvas.getPixel(x, y) != 0) anyInk = true;
    ASSERT(!anyInk, "popup: a hidden popup draws nothing");
}

int main() {
    test_popup_truncates_lines();
    test_popup_auto_hide_seam();
    test_popup_render();

    printf("\n%d passed, %d failed\n", passes, failures);
    return failures == 0 ? 0 : 1;
}
