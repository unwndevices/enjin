// Label widget unit test (Phase 3a, #121): C_Label rewritten as a data-only
// LabelComponent + LabelSystem on the upstream ui ECS.
//
// The word-wrap is the gnarly seam, so it is pinned pure with an injected fixed
// -width measurer (no font, no canvas): words pack greedily onto a line, spill
// when they would overflow, and a lone over-wide word still gets its own line
// without looping forever. A render pass then proves the system inks glyphs and
// paints the rounded background panel on a real Canvas4.
#include <enjin2/ui/component.hpp>
#include <enjin2/ui/components.hpp>
#include <enjin2/ui/world.hpp>
#include <enjin2/ui/theme.hpp>
#include <enjin2/ui/widgets/label.hpp>
#include <enjin2/graphics/canvas.hpp>
#include <cstdio>
#include <string>

using namespace enjin2;

static int passes = 0;
static int failures = 0;

#define ASSERT(cond, msg)                                                     \
    do {                                                                      \
        if (!(cond)) { fprintf(stderr, "FAIL: %s\n", msg); failures++; }      \
        else { printf("PASS: %s\n", msg); passes++; }                         \
    } while (0)

// A fixed 6px-per-character measurer stands in for a font so wrapping is exact.
static int measure6(const std::string& s) { return static_cast<int>(s.size()) * 6; }

using LabelWorld = World<32, LabelComponent, PositionComponent, SizeComponent>;

// Words that fit on one line stay on one line; when the budget is tight they spill
// onto the next.
static void test_wrap_greedy() {
    // "hi world" = 8 chars incl. space -> 48px. A 60px budget keeps it as one line.
    auto oneLine = LabelComponent::wrapText("hi world", 60, measure6);
    ASSERT(oneLine.size() == 1, "label: fitting text stays on one line");
    ASSERT(oneLine[0] == "hi world", "label: single line keeps both words");

    // A 40px budget fits "hi" (12px) but not "hi world" (48px) -> two lines.
    auto twoLines = LabelComponent::wrapText("hi world", 40, measure6);
    ASSERT(twoLines.size() == 2, "label: overflowing text wraps to a new line");
    ASSERT(twoLines[0] == "hi" && twoLines[1] == "world", "label: wrap splits on the word boundary");
}

// A single word wider than the budget can't be broken, so it occupies its own line
// rather than wedging the greedy loop.
static void test_wrap_overlong_word() {
    auto lines = LabelComponent::wrapText("supercalifragilistic", 30, measure6);
    ASSERT(lines.size() == 1, "label: an unbreakable word takes one line");
    ASSERT(lines[0] == "supercalifragilistic", "label: over-wide word is preserved whole");
}

// Leading / doubled / trailing spaces collapse instead of producing empty lines.
static void test_wrap_collapses_spaces() {
    auto lines = LabelComponent::wrapText("  a   b  ", 100, measure6);
    ASSERT(lines.size() == 1, "label: runs of spaces don't make empty lines");
    ASSERT(lines[0] == "a b", "label: words rejoin with single spaces");
}

// End-to-end render: a transparent label inks glyph pixels on the canvas.
static void test_render_draws_text() {
    LabelWorld world;
    Canvas4<64, 64> canvas;
    LabelSystem<LabelWorld, Canvas4<64, 64>> system(&world, &canvas);

    Entity e = world.create();
    world.add<LabelComponent>(e, std::string("HELLO"));
    world.add<PositionComponent>(e, Point(0, 0));
    world.add<SizeComponent>(e, Size(64, 32));

    canvas.clear(Colors::BLACK);
    system.update(0.016f);

    bool anyInk = false;
    for (int16_t y = 0; y < 64; ++y)
        for (int16_t x = 0; x < 64; ++x)
            if (canvas.getPixel(x, y) != 0) anyInk = true;
    ASSERT(anyInk, "label: render inks glyph pixels");
}

// A label with a background color paints the rounded panel fill beneath the text.
static void test_render_draws_background_panel() {
    LabelWorld world;
    Canvas4<64, 64> canvas;
    LabelSystem<LabelWorld, Canvas4<64, 64>> system(&world, &canvas);

    Entity e = world.create();
    auto* label = world.add<LabelComponent>(e, std::string("hi"));
    label->setBackground(Pixel4(10));
    world.add<PositionComponent>(e, Point(4, 4));
    world.add<SizeComponent>(e, Size(40, 24));

    canvas.clear(Colors::BLACK);
    system.update(0.016f);

    // The panel fill (value 10) must land somewhere inside the box.
    bool anyPanel = false;
    for (int16_t y = 0; y < 64; ++y)
        for (int16_t x = 0; x < 64; ++x)
            if (canvas.getPixel(x, y) == 10) anyPanel = true;
    ASSERT(anyPanel, "label: background paints the rounded panel");
}

// An empty, transparent label draws nothing rather than touching the canvas.
static void test_render_empty_is_safe() {
    LabelWorld world;
    Canvas4<32, 32> canvas;
    LabelSystem<LabelWorld, Canvas4<32, 32>> system(&world, &canvas);

    Entity e = world.create();
    world.add<LabelComponent>(e, std::string(""));
    world.add<PositionComponent>(e, Point(0, 0));
    world.add<SizeComponent>(e, Size(32, 32));

    canvas.clear(Colors::BLACK);
    system.update(0.016f);

    bool anyInk = false;
    for (int16_t y = 0; y < 32; ++y)
        for (int16_t x = 0; x < 32; ++x)
            if (canvas.getPixel(x, y) != 0) anyInk = true;
    ASSERT(!anyInk, "label: empty transparent label draws nothing");
}

int main() {
    test_wrap_greedy();
    test_wrap_overlong_word();
    test_wrap_collapses_spaces();
    test_render_draws_text();
    test_render_draws_background_panel();
    test_render_empty_is_safe();

    printf("\n%d passed, %d failed\n", passes, failures);
    return failures == 0 ? 0 : 1;
}
