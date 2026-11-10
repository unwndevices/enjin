// The one text route (ADR-0029, Tomodachi #476): the rules every string on
// every host is drawn by.
//   * y is the top of the line; the baseline is y + ascent * size.
//   * A string's width is its advance.
//   * Text is UTF-8; a character the face lacks draws a tofu, logged once per
//     character and face.
//   * Overflow is one setting: wrap (at spaces; inside a word only when the
//     word is longer than the line) | clip | ellipsis, at the box's width or
//     the layer's edge.
//   * The state is sticky and lives on the canvas: one canvas's settings never
//     reach another.

#include <enjin2/graphics/canvas.hpp>
#include <enjin2/graphics/faces/faces.hpp>
#include <enjin2/graphics/palette.hpp>
#include <enjin2/graphics/text_renderer.hpp>

#include <cstdio>
#include <string>
#include <vector>

using namespace enjin2;

static int passes = 0;
static int failures = 0;

#define ASSERT(cond, msg)                                                    \
    do {                                                                     \
        if (!(cond)) {                                                       \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, msg);    \
            failures++;                                                      \
        } else {                                                             \
            passes++;                                                        \
        }                                                                    \
    } while (0)

static constexpr uint8_t kT = PALETTE_TRANSPARENT;
using Canvas = Canvas4<96, 48>;

struct Box {
    int x0 = 9999, y0 = 9999, x1 = -1, y1 = -1;  // inclusive ink bounds
    int n = 0;
    bool empty() const { return n == 0; }
};

// The ink of colour @p c (any non-transparent when c == 255).
static Box ink(const Canvas& cv, uint8_t c = 255, int x0 = 0, int y0 = 0, int x1 = 96, int y1 = 48) {
    Box b;
    for (int y = y0; y < y1; ++y)
        for (int x = x0; x < x1; ++x) {
            const uint8_t v = cv.getPixel(int16_t(x), int16_t(y)).value;
            if (c == 255 ? v != kT : v == c) {
                b.n++;
                if (x < b.x0) b.x0 = x;
                if (y < b.y0) b.y0 = y;
                if (x > b.x1) b.x1 = x;
                if (y > b.y1) b.y1 = y;
            }
        }
    return b;
}

static bool same(const Canvas& a, const Canvas& b) {
    for (int y = 0; y < 48; ++y)
        for (int x = 0; x < 96; ++x)
            if (a.getPixel(int16_t(x), int16_t(y)).value != b.getPixel(int16_t(x), int16_t(y)).value) return false;
    return true;
}

static Canvas blank() {
    Canvas c;
    c.clear(Pixel4(kT));
    return c;
}

static void test_y_is_the_top_of_the_line() {
    printf("--- y is the top of the line, in every face ---\n");
    for (const Face* f : faces::all) {
        Canvas c = blank();
        TextRenderer<Pixel4> t(c);
        t.setFont(*f);
        t.draw("H", 2, 3);
        const Box b = ink(c);
        // H stands on the baseline: its lowest row is baseline - 1, its top is
        // cap height above it.
        ASSERT(b.y1 == 3 + f->ascent - 1, f->name);
        ASSERT(b.y0 == 3 + f->ascent - f->capHeight, f->name);
    }
    // At scale 2 everything doubles from the same top.
    Canvas c = blank();
    TextRenderer<Pixel4> t(c);
    t.setTextSize(2);
    t.draw("H", 0, 1);
    const Box b = ink(c);
    ASSERT(b.y1 == 1 + 2 * faces::monogram.ascent - 1, "scale 2 baseline");
    ASSERT(b.y0 == 1 + 2 * (faces::monogram.ascent - faces::monogram.capHeight), "scale 2 cap top");
}

static void test_width_is_the_advance() {
    printf("--- a string's width is its advance ---\n");
    Canvas c = blank();
    TextRenderer<Pixel4> t(c);
    ASSERT(t.width("Track 01") == 8 * 6, "monogram: 6 px a character, the last one's too");
    ASSERT(t.width("") == 0, "empty");
    ASSERT(t.width("ab\nabcd") == 4 * 6, "the widest line");
    t.setTextSize(3);
    ASSERT(t.width("ab") == 2 * 6 * 3, "times the size");
    t.setFont(faces::body);
    t.setTextSize(1);
    int sum = 0;
    for (char ch : std::string("Hello")) sum += text::advance(faces::body, uint8_t(ch));
    ASSERT(t.width("Hello") == sum, "body: the sum of the advances");
    ASSERT(text::width(faces::display, "Hi", 2) == 2 * (text::advance(faces::display, 'H') + text::advance(faces::display, 'i')),
           "the free function measures without a canvas");
}

static void test_default_face_is_monogram_in_the_text_role() {
    printf("--- a fresh canvas draws monogram in the text role ---\n");
    Canvas c = blank();
    TextRenderer<Pixel4> t(c);
    ASSERT(&t.face() == &faces::monogram, "monogram is the default face");
    t.draw("H", 0, 0);
    ASSERT(ink(c, ROLE_TEXT).n == ink(c).n && ink(c).n > 0, "every pixel in the text role");
    ASSERT(t.lineHeight() == faces::monogram.font->yAdvance, "the line height is the face's yAdvance");
}

static int g_logged = 0;
static uint32_t g_lastCp = 0;
static void countingLog(const Face&, uint32_t cp) {
    ++g_logged;
    g_lastCp = cp;
}

static void test_utf8_and_the_tofu() {
    printf("--- UTF-8 in; a missing character is a tofu, logged once ---\n");
    text::missingLog() = &countingLog;
    g_logged = 0;
    {
        Canvas c = blank();
        TextRenderer<Pixel4> t(c);
        // U+20AC (euro, 3 bytes) is in no face: one tofu one advance wide.
        ASSERT(t.width("\xE2\x82\xAC") == text::tofuAdvance(faces::monogram), "a 3-byte character is one tofu");
        ASSERT(t.width("a\xE2\x82\xAC" "b") == 3 * 6, "one character between two");
        t.draw("\xE2\x82\xAC", 0, 0);
        const Box b = ink(c);
        const int base = faces::monogram.ascent;
        ASSERT(b.x0 == 0 && b.x1 == text::tofuAdvance(faces::monogram) - 2, "the box is an advance wide, less a column");
        ASSERT(b.y1 == base - 1 && b.y0 == base - faces::monogram.capHeight, "the box is cap high, on the baseline");
        ASSERT(c.getPixel(2, int16_t(base - 3)).value == kT, "the box is hollow");
        ASSERT(g_logged == 1 && g_lastCp == 0x20AC, "logged on first draw");
        t.draw("\xE2\x82\xAC\xE2\x82\xAC", 0, 20);
        ASSERT(g_logged == 1, "not again for the same character and face");
        t.setFont(faces::body);
        t.draw("\xE2\x82\xAC", 0, 30);
        ASSERT(g_logged == 2, "once more for another face");
    }
    {
        Canvas c = blank();
        TextRenderer<Pixel4> t(c);
        ASSERT(t.width("\xFF") == text::tofuAdvance(faces::monogram), "a malformed byte is one tofu");
        ASSERT(t.width("\xC3") == text::tofuAdvance(faces::monogram), "a cut sequence is one tofu");
        size_t i = 0;
        ASSERT(text::decodeUtf8("\xC3\xA9", i) == 0xE9 && i == 2, "two bytes decode to U+00E9");
        i = 0;
        ASSERT(text::decodeUtf8("\xC0\x80", i) == 0xFFFD, "an overlong form is malformed");
    }
    text::missingLog() = &text::defaultMissingLog;
}

static void test_wrap_breaks_at_spaces() {
    printf("--- wrap: at spaces, inside a word only when it is longer than the line ---\n");
    const int lineH = faces::monogram.font->yAdvance;
    {
        Canvas c = blank();
        TextRenderer<Pixel4> t(c);
        t.setTextWrap(TextOverflow::Wrap, 30);  // five monogram characters
        ASSERT(t.draw("aaa bbb", 0, 0) == 2, "two words that don't fit together: two lines");
        Canvas ref = blank();
        TextRenderer<Pixel4> r(ref);
        r.draw("aaa", 0, 0);
        r.draw("bbb", 0, lineH);
        ASSERT(same(c, ref), "each line starts at x, the space at the break dropped");
    }
    {
        Canvas c = blank();
        TextRenderer<Pixel4> t(c);
        t.setTextWrap(TextOverflow::Wrap, 30);
        ASSERT(t.draw("abcdefgh", 0, 0) == 2, "a word longer than the line breaks inside it");
        Canvas ref = blank();
        TextRenderer<Pixel4> r(ref);
        r.draw("abcde", 0, 0);
        r.draw("fgh", 0, lineH);
        ASSERT(same(c, ref), "as much of the word as fits, then the rest");
    }
    {
        Canvas c = blank();
        TextRenderer<Pixel4> t(c);
        t.setTextWrap(TextOverflow::Wrap, 30);
        ASSERT(t.draw("ab cdefghij", 0, 0) == 3, "a short word, then the long one alone");
    }
    {
        Canvas c = blank();
        TextRenderer<Pixel4> t(c);
        // No width: the box runs to the layer's edge (96 - 60 = 36 = six characters).
        ASSERT(t.draw("aaa bbb", 60, 0) == 2, "wraps at the layer's edge");
        ASSERT(t.draw("aa bbb", 60, 20) == 1, "a line that fits stays one");
    }
    {
        Canvas c = blank();
        TextRenderer<Pixel4> t(c);
        ASSERT(t.draw("a\nb\n\nc", 0, 0) == 4, "a newline starts a line, an empty one too");
    }
}

static void test_clip_and_ellipsis() {
    printf("--- clip cuts at the box; ellipsis cuts so that ... fits ---\n");
    {
        Canvas c = blank();
        TextRenderer<Pixel4> t(c);
        t.setTextWrap(TextOverflow::Clip, 30);
        ASSERT(t.draw("abcdefgh", 10, 0) == 1, "one line");
        const Box b = ink(c);
        ASSERT(b.x1 < 40 && b.x0 >= 10, "nothing past the box's edge");
        ASSERT(!ink(c, 255, 0, 0, 96, 48).empty(), "the head is drawn");
    }
    {
        Canvas c = blank();
        TextRenderer<Pixel4> t(c);
        t.setTextWrap(TextOverflow::Ellipsis, 30);
        t.draw("abcdefgh", 0, 0);
        Canvas ref = blank();
        TextRenderer<Pixel4> r(ref);
        r.draw("ab...", 0, 0);  // 2 characters + three dots = 5 advances = 30 px
        ASSERT(same(c, ref), "the longest head that leaves room for ...");
        Canvas c2 = blank();
        TextRenderer<Pixel4> t2(c2);
        t2.setTextWrap(TextOverflow::Ellipsis, 30);
        t2.draw("abcde", 0, 0);
        Canvas ref2 = blank();
        TextRenderer<Pixel4> r2(ref2);
        r2.draw("abcde", 0, 0);
        ASSERT(same(c2, ref2), "a line that fits is drawn whole");
    }
}

static void test_align() {
    printf("--- x is the left edge, centre or right edge ---\n");
    Canvas a = blank(), b = blank();
    TextRenderer<Pixel4>(a).draw("ab", 60, 0, TextAlign::Right);
    TextRenderer<Pixel4>(b).draw("ab", 48, 0);
    ASSERT(same(a, b), "right: the advance ends at x");
    Canvas c = blank(), d = blank();
    TextRenderer<Pixel4>(c).draw("ab", 48, 0, TextAlign::Center);
    TextRenderer<Pixel4>(d).draw("ab", 42, 0);
    ASSERT(same(c, d), "center: half the advance either side of x");
    Canvas e = blank();
    TextRenderer<Pixel4> t(e);
    t.setTextWrap(TextOverflow::Wrap, 30);
    ASSERT(t.draw("aa bb", 48, 0, TextAlign::Center) == 1 && t.draw("aaa bbb", 48, 20, TextAlign::Center) == 2,
           "a centred box is the width, centred on x");
}

static void test_state_is_sticky_and_per_canvas() {
    printf("--- the state is sticky, and each canvas has its own ---\n");
    Canvas a = blank(), b = blank();
    {
        TextRenderer<Pixel4> t(a);
        t.setFont(faces::display);
        t.setTextSize(2);
        t.setTextColor(ROLE_ACCENT);
        t.setTextWrap(TextOverflow::Clip, 20);
    }
    TextRenderer<Pixel4> again(a);
    ASSERT(&again.face() == &faces::display && again.textSize() == 2, "a later renderer sees the settings");
    ASSERT(a.text().overflow == TextOverflow::Clip && a.text().width == 20 && a.text().color == ROLE_ACCENT,
           "they live on the canvas");
    TextRenderer<Pixel4> other(b);
    ASSERT(&other.face() == &faces::monogram && other.textSize() == 1 && b.text().overflow == TextOverflow::Wrap,
           "another canvas keeps its defaults");
    again.reset();
    ASSERT(&again.face() == &faces::monogram && a.text().width == 0, "reset puts the defaults back");
}

static void test_clip_window_and_print() {
    printf("--- the clip window; print continues at the cursor ---\n");
    {
        Canvas c = blank();
        TextRenderer<Pixel4> t(c);
        t.setClip(3, 0, 6, 48);
        t.draw("HHHH", 0, 0);
        const Box b = ink(c);
        ASSERT(b.x0 >= 3 && b.x1 <= 8 && b.n > 0, "only inside the window");
        t.clearClip();
        t.draw("H", 40, 0);
        ASSERT(ink(c, 255, 40, 0, 50, 48).n > 0, "cleared, everywhere again");
    }
    {
        Canvas c = blank(), ref = blank();
        TextRenderer<Pixel4> t(c);
        t.setCursor(4, 2);
        t.print("ab");
        t.print("cd");
        TextRenderer<Pixel4>(ref).draw("abcd", 4, 2);
        ASSERT(same(c, ref), "print, print = one draw");
    }
}

static void test_outline_under_the_whole_line() {
    printf("--- the outline is drawn under the whole line, the fill over it ---\n");
    Canvas c = blank();
    TextRenderer<Pixel4> t(c);
    t.setOutline(true);
    t.draw("HH", 2, 2);
    Canvas fill = blank();
    TextRenderer<Pixel4>(fill).draw("HH", 2, 2);
    // Every fill pixel survives: the second H's outline never covers the first H.
    bool kept = true;
    for (int y = 0; y < 48; ++y)
        for (int x = 0; x < 96; ++x)
            if (fill.getPixel(int16_t(x), int16_t(y)).value == ROLE_TEXT &&
                c.getPixel(int16_t(x), int16_t(y)).value != ROLE_TEXT)
                kept = false;
    ASSERT(kept, "no fill pixel lost to a neighbour's outline");
    ASSERT(ink(c, ROLE_BG).n > 0, "the outline is bg");
}

static void test_scope_and_cap_offset() {
    printf("--- TextScope starts at the defaults and puts the layer's state back ---\n");
    Canvas c = blank();
    TextRenderer<Pixel4>(c).setFont(faces::display).setTextSize(3).setTextWrap(TextOverflow::Clip, 9);
    {
        TextScope<Pixel4> scope(c);
        TextRenderer<Pixel4> t(c);
        ASSERT(&t.face() == &faces::monogram && t.textSize() == 1, "inside, the defaults");
        t.setFont(faces::body).setOutline(true);
    }
    ASSERT(c.text().face == &faces::display && c.text().size == 3 && c.text().width == 9 && !c.text().outline,
           "after, the layer's own state");
    ASSERT(text::capOffset(faces::monogram) == 4 && text::capOffset(faces::display, 2) == 10, "capOffset");
    TextRenderer<Pixel4> t(c);
    t.reset();
    ASSERT(t.topForCap(20) == 16, "topForCap = capTop - capOffset");
}

static void test_missing_log_stops_after_64() {
    printf("--- the missing-character log remembers 64 pairs, then goes quiet ---\n");
    text::missingLog() = &countingLog;
    g_logged = 0;
    Canvas c = blank();
    TextRenderer<Pixel4> t(c);
    t.setTextWrap(TextOverflow::Clip, 1);
    char buf[8];
    for (uint32_t cp = 0x3041; cp < 0x3041 + 80; ++cp) {  // hiragana: in no face
        buf[0] = char(0xE0 | (cp >> 12)); buf[1] = char(0x80 | ((cp >> 6) & 0x3F)); buf[2] = char(0x80 | (cp & 0x3F)); buf[3] = 0;
        t.draw(buf, 0, 0);
        t.draw(buf, 0, 0);
    }
    ASSERT(g_logged <= 64, "never more than 64 lines, however many frames draw them");
    text::missingLog() = &text::defaultMissingLog;
}

int main() {
    test_y_is_the_top_of_the_line();
    test_width_is_the_advance();
    test_default_face_is_monogram_in_the_text_role();
    test_utf8_and_the_tofu();
    test_wrap_breaks_at_spaces();
    test_clip_and_ellipsis();
    test_align();
    test_state_is_sticky_and_per_canvas();
    test_clip_window_and_print();
    test_outline_under_the_whole_line();
    test_scope_and_cap_offset();
    test_missing_log_stops_after_64();
    printf("\ntext_renderer_test: %d passed, %d failed\n", passes, failures);
    return failures ? 1 : 0;
}
