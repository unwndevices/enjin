// Corner cuts (Tomodachi, the safe-area work): RoundedCorners::cornerAt says
// which pixels a rounded screen hides, and TextRenderer reports each string
// whose ink lands there to the armed CornerWatch, once per string.

#include <enjin2/graphics/canvas.hpp>
#include <enjin2/graphics/corner_watch.hpp>
#include <enjin2/graphics/text_renderer.hpp>
#include <cstdio>
#include <string>
#include <vector>

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
    } while (0)

// A 3x3 solid block for 'A', advancing 4 px: ink is exact.
static const uint8_t kBlockBitmaps[] = {0xFF, 0x80};
static const GFXglyph kBlockGlyphs[] = {{0, 3, 3, 4, 0, 0}};
static const GFXfont kBlockFont = {(uint8_t*)kBlockBitmaps, (GFXglyph*)kBlockGlyphs, 0x41, 0x41, 5};

struct Heard : CornerWatch {
    explicit Heard(RoundedCorners c) : CornerWatch(c) {}
    struct Entry {
        std::string text;
        CornerCut cut;
    };
    std::vector<Entry> cuts;
    void cut(const CornerCut& c) override { cuts.push_back({std::string(c.text), c}); }
};

static constexpr RoundedCorners kCorners{160, 160, 20, 0};

static void test_corner_pixels() {
    ASSERT(kCorners.cornerAt(0, 0) == Corner::TopLeft, "the corner pixel is hidden");
    ASSERT(kCorners.cornerAt(159, 0) == Corner::TopRight, "top-right corner pixel");
    ASSERT(kCorners.cornerAt(159, 159) == Corner::BottomRight, "bottom-right corner pixel");
    ASSERT(kCorners.cornerAt(0, 159) == Corner::BottomLeft, "bottom-left corner pixel");
    // On the diagonal the curve is r(1 - 1/sqrt2) = 5.86 px in.
    ASSERT(kCorners.cornerAt(5, 5) == Corner::TopLeft, "(5,5) is outside the curve");
    ASSERT(kCorners.cornerAt(6, 6) == Corner::None, "(6,6) is inside the curve");
    ASSERT(kCorners.cornerAt(153, 153) == Corner::None, "(153,153) mirrors (6,6)");
    ASSERT(kCorners.cornerAt(154, 154) == Corner::BottomRight, "(154,154) mirrors (5,5)");
    // The sides are never cut: only the corners are rounded.
    ASSERT(kCorners.cornerAt(80, 0) == Corner::None, "the top edge's middle shows");
    ASSERT(kCorners.cornerAt(0, 80) == Corner::None, "the left edge's middle shows");
    ASSERT(kCorners.cornerAt(20, 0) == Corner::None, "the curve ends at x = r");
    ASSERT(kCorners.cornerAt(-1, 0) == Corner::None, "off the canvas is an edge, not a corner");
    // The margin moves the line one px inward.
    constexpr RoundedCorners withMargin{160, 160, 20, 1};
    ASSERT(withMargin.cornerAt(6, 6) == Corner::TopLeft, "a margin of 1 counts (6,6) as cut");
    ASSERT(withMargin.cornerAt(7, 7) == Corner::None, "(7,7) is clear of the margin");
    constexpr RoundedCorners square{160, 160, 0, 0};
    ASSERT(square.cornerAt(0, 0) == Corner::None, "a square screen cuts nothing");
}

// The block font as a face: its glyphs hang from the baseline (yOffset 0), so
// with ascent 0 a string's ink starts at the line's top.
static constexpr Face kBlock{"block", &kBlockFont, 0, 3};

static void draw(Canvas4<160, 160>& c, int x, int y, const char* s) {
    TextRenderer<Pixel4>(c).setFont(kBlock).draw(s, x, y);
}

static void test_string_in_a_corner_is_reported_once() {
    Heard heard(kCorners);
    CornerWatch::arm(&heard);
    Canvas4<160, 160> c;
    draw(c, 0, 0, "AAA");
    CornerWatch::arm(nullptr);
    ASSERT(heard.cuts.size() == 1, "one report for the whole string");
    if (heard.cuts.size() != 1) return;
    const auto& e = heard.cuts[0];
    ASSERT(e.text == "AAA", "the report names the string");
    ASSERT(e.cut.corner == Corner::TopLeft, "the top-left corner cut it");
    ASSERT(e.cut.x == 0 && e.cut.y == 0 && e.cut.w == 11 && e.cut.h == 3, "the ink box is the string's");
    ASSERT(e.cut.pixels > 0 && e.cut.pixels < 27, "only the hidden ink is counted");
    ASSERT(!e.cut.script, "native text is the host's layout");
}

static void test_string_clear_of_the_corners_is_quiet() {
    Heard heard(kCorners);
    CornerWatch::arm(&heard);
    Canvas4<160, 160> c;
    draw(c, 60, 60, "AAA");
    draw(c, 70, 0, "AAA");  // the top edge, away from the corners
    draw(c, 0, 70, "AAA");  // the left edge
    CornerWatch::arm(nullptr);
    ASSERT(heard.cuts.empty(), "text on the sides or the middle is not cut");
}

static void test_other_canvases_and_no_watch_are_quiet() {
    Heard heard(kCorners);
    CornerWatch::arm(&heard);
    Canvas4<32, 32> small;
    TextRenderer<Pixel4>(small).setFont(kBlock).draw("AAA", 0, 0);
    CornerWatch::arm(nullptr);
    ASSERT(heard.cuts.empty(), "an offscreen canvas of another size is not the screen");

    Canvas4<160, 160> c;
    draw(c, 0, 0, "AAA");  // nothing armed: nothing to hear it, nothing breaks
    ASSERT(heard.cuts.empty(), "an unarmed watch hears nothing");
}

static void test_monogram_and_single_chars() {
    Heard heard(kCorners);
    CornerWatch::arm(&heard);
    Canvas4<160, 160> c;
    TextRenderer<Pixel4>(c).draw("x", 150, 145);  // monogram, the default face
    draw(c, 157, 0, "A");
    CornerWatch::arm(nullptr);
    ASSERT(heard.cuts.size() == 2, "the default face and a lone char both report");
    if (heard.cuts.size() != 2) return;
    ASSERT(heard.cuts[0].cut.corner == Corner::BottomRight, "monogram's x in the bottom-right corner");
    ASSERT(heard.cuts[1].text == "A" && heard.cuts[1].cut.corner == Corner::TopRight,
           "a lone char is a string of one");
}

static void test_wrapped_text_is_one_string() {
    Heard heard(kCorners);
    CornerWatch::arm(&heard);
    Canvas4<160, 160> c;
    TextRenderer<Pixel4> tr(c);
    tr.setTextWrap(TextOverflow::Wrap, 14);
    const int lines = tr.draw("AA AA", 0, 0);
    CornerWatch::arm(nullptr);
    ASSERT(lines == 2, "the string wraps");
    ASSERT(heard.cuts.size() == 1, "a wrapped string reports once");
}

static void test_outline_is_not_ink() {
    Heard heard(kCorners);
    CornerWatch::arm(&heard);
    Canvas4<160, 160> c;
    // The fill at (2..4, 10..12) is inside the curve; its outline at (1, 10)
    // and (2, 9) is not.
    TextRenderer<Pixel4>(c).setFont(kBlock).setOutline(true).draw("A", 2, 10);
    CornerWatch::arm(nullptr);
    ASSERT(heard.cuts.empty(), "the legibility outline is not the string's ink");
}

int main() {
    test_corner_pixels();
    test_string_in_a_corner_is_reported_once();
    test_string_clear_of_the_corners_is_quiet();
    test_other_canvases_and_no_watch_are_quiet();
    test_monogram_and_single_chars();
    test_wrapped_text_is_one_string();
    test_outline_is_not_ink();
    printf("\n%d passed, %d failed\n", passes, failures);
    return failures == 0 ? 0 : 1;
}
