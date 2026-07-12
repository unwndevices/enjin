// Overlay + PopUp widget test (Phase 3a, #121): OverlayBg's dim and PopUpUI's modal
// rewritten as data-only OverlayComponent / PopUpComponent + their systems.
//
// Two pure seams carry the logic: the overlay's per-pixel subtractive dim (which
// saturates at black), and the popup's auto-hide countdown (which accrues time and
// hides itself at the deadline). Render passes then dim a real Canvas4 and draw the
// popup card, and confirm a hidden popup paints nothing.
#include <enjin2/ui/component.hpp>
#include <enjin2/ui/components.hpp>
#include <enjin2/ui/world.hpp>
#include <enjin2/ui/widgets/overlay.hpp>
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

// The overlay's dim subtracts its opacity from a pixel and saturates at 0 so a
// dark pixel never wraps back to bright.
static void test_overlay_dim_saturates() {
    OverlayComponent overlay(5);
    ASSERT(overlay.dim(12) == 7, "overlay: dim subtracts the opacity");
    ASSERT(overlay.dim(5) == 0, "overlay: dim to exactly zero");
    ASSERT(overlay.dim(2) == 0, "overlay: dim saturates at black");
}

// The overlay system darkens the whole canvas by the overlay's opacity.
static void test_overlay_render_darkens_all() {
    using OverlayWorld = World<8, OverlayComponent>;
    OverlayWorld world;
    Canvas4<16, 16> canvas;
    OverlaySystem<OverlayWorld, Canvas4<16, 16>> system(&world, &canvas);

    Entity e = world.create();
    world.add<OverlayComponent>(e, static_cast<uint8_t>(4));

    canvas.clear(Pixel4(10));
    system.update(0.016f);

    ASSERT(canvas.getPixel(0, 0) == 6, "overlay: top-left dimmed 10 -> 6");
    ASSERT(canvas.getPixel(15, 15) == 6, "overlay: bottom-right dimmed 10 -> 6");
}

// A hidden overlay leaves the canvas untouched.
static void test_overlay_hidden_is_noop() {
    using OverlayWorld = World<8, OverlayComponent>;
    OverlayWorld world;
    Canvas4<16, 16> canvas;
    OverlaySystem<OverlayWorld, Canvas4<16, 16>> system(&world, &canvas);

    Entity e = world.create();
    auto* overlay = world.add<OverlayComponent>(e, static_cast<uint8_t>(4));
    overlay->setVisible(false);

    canvas.clear(Pixel4(10));
    system.update(0.016f);
    ASSERT(canvas.getPixel(0, 0) == 10, "overlay: hidden overlay does not dim");
}

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
    test_overlay_dim_saturates();
    test_overlay_render_darkens_all();
    test_overlay_hidden_is_noop();
    test_popup_truncates_lines();
    test_popup_auto_hide_seam();
    test_popup_render();

    printf("\n%d passed, %d failed\n", passes, failures);
    return failures == 0 ? 0 : 1;
}
