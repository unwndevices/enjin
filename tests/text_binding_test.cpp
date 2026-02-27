/**
 * @file text_binding_test.cpp
 * @brief Unit tests for Lua text bindings: text(), textWrapped(),
 *        setTextSize/getTextSize, setFont/getFont, getTextWidth, getTextHeight.
 *
 * Verifies measurement correctness, pixel output, newline handling,
 * word-wrap behaviour, and font switching.
 */
#include <enjin2/scripting/bindings.hpp>
#include <enjin2/scripting/lua_engine.hpp>
#include <enjin2/graphics/canvas.hpp>
#include <cstdio>
#include <cstring>

using namespace enjin2;

static int passes = 0;
static int failures = 0;

#define ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            fprintf(stderr, "FAIL [line %d]: %s\n", __LINE__, (msg)); \
            failures++; \
        } else { \
            passes++; \
        } \
    } while(0)

// ============================================================
// Fixture: LuaEngine + LuaBindings + 128x128 Canvas8
// ============================================================
struct TextFixture {
    Canvas8<128, 128> canvas;
    LuaCanvas luaCanvas;
    LuaEngine engine;
    LuaBindings bindings;

    TextFixture()
        : luaCanvas(&canvas)
        , bindings(&engine)
    {
        engine.initialize();
        bindings.registerAll();
        bindings.setCanvas(&luaCanvas);
        canvas.clear(0);
    }

    LuaResult exec(const char* code) {
        return engine.executeString(code);
    }

    double getNum(const char* name) {
        return engine.getGlobalNumber(name);
    }

    std::string getStr(const char* name) {
        return engine.getGlobalString(name);
    }

    bool hasNonZeroPixel(int16_t x0, int16_t y0, int16_t w, int16_t h) {
        for (int16_t y = y0; y < y0 + h; ++y)
            for (int16_t x = x0; x < x0 + w; ++x)
                if (canvas.getPixel(x, y) != 0) return true;
        return false;
    }

    bool regionIsBlank(int16_t x0, int16_t y0, int16_t w, int16_t h) {
        return !hasNonZeroPixel(x0, y0, w, h);
    }
};

// ============================================================
// getTextWidth: default 5x7 font, size 1
// Each char = 6 px wide (5 glyph + 1 spacing)
// ============================================================
static void test_getTextWidth_default_size1() {
    printf("--- getTextWidth default font size 1 ---\n");
    TextFixture f;
    f.exec("w = getTextWidth('Hello')");
    // 5 chars * 6 px = 30
    ASSERT(f.getNum("w") == 30.0, "getTextWidth('Hello') should be 30 at size 1");
}

// ============================================================
// getTextWidth: default 5x7 font, size 2
// ============================================================
static void test_getTextWidth_default_size2() {
    printf("--- getTextWidth default font size 2 ---\n");
    TextFixture f;
    f.exec("setTextSize(2); w = getTextWidth('Hi')");
    // 2 chars * 6 px * 2 = 24
    ASSERT(f.getNum("w") == 24.0, "getTextWidth('Hi') should be 24 at size 2");
}

// ============================================================
// getTextWidth: empty string
// ============================================================
static void test_getTextWidth_empty() {
    printf("--- getTextWidth empty string ---\n");
    TextFixture f;
    f.exec("w = getTextWidth('')");
    ASSERT(f.getNum("w") == 0.0, "getTextWidth('') should be 0");
}

// ============================================================
// getTextHeight: default font size 1  =>  8 * 1 = 8
// ============================================================
static void test_getTextHeight_default_size1() {
    printf("--- getTextHeight default font size 1 ---\n");
    TextFixture f;
    f.exec("h = getTextHeight()");
    ASSERT(f.getNum("h") == 8.0, "getTextHeight() should be 8 at size 1 (default 5x7)");
}

// ============================================================
// getTextHeight: default font size 3  =>  8 * 3 = 24
// ============================================================
static void test_getTextHeight_default_size3() {
    printf("--- getTextHeight default font size 3 ---\n");
    TextFixture f;
    f.exec("setTextSize(3); h = getTextHeight()");
    ASSERT(f.getNum("h") == 24.0, "getTextHeight() should be 24 at size 3");
}

// ============================================================
// getTextHeight: default8 font size 1  =>  14 * 1 = 14
// ============================================================
static void test_getTextHeight_default8() {
    printf("--- getTextHeight default8 font size 1 ---\n");
    TextFixture f;
    f.exec("setFont('default8'); h = getTextHeight()");
    ASSERT(f.getNum("h") == 14.0, "getTextHeight() should be 14 for default8 at size 1");
}

// ============================================================
// text() draws pixels in the expected region
// ============================================================
static void test_text_draws_pixels() {
    printf("--- text() draws pixels ---\n");
    TextFixture f;
    f.exec("setColor(7); text('A', 0, 0)");
    // 'A' at size 1 should write some non-zero pixels in the 6x8 area at (0,0)
    ASSERT(f.hasNonZeroPixel(0, 0, 6, 8), "text('A',0,0) should produce non-zero pixels in 6x8 region");
}

// ============================================================
// text() does not bleed outside expected bounds
// ============================================================
static void test_text_no_bleed() {
    printf("--- text() no bleed beyond glyph ---\n");
    TextFixture f;
    f.exec("setColor(7); text('A', 10, 10)");
    // Row 0..9 and column 0..9 should be blank (character starts at 10,10)
    ASSERT(f.regionIsBlank(0, 0, 10, 10), "area before origin should be blank");
    // Past the glyph: column 16+ should be blank (10 + 6 = 16)
    ASSERT(f.regionIsBlank(16, 10, 20, 8), "area after glyph should be blank");
}

// ============================================================
// setTextSize / getTextSize roundtrip
// ============================================================
static void test_setTextSize_roundtrip() {
    printf("--- setTextSize / getTextSize roundtrip ---\n");
    TextFixture f;
    f.exec("setTextSize(4); sz = getTextSize()");
    ASSERT(f.getNum("sz") == 4.0, "getTextSize() should return 4 after setTextSize(4)");
}

// ============================================================
// setFont / getFont roundtrip
// ============================================================
static void test_setFont_roundtrip() {
    printf("--- setFont / getFont roundtrip ---\n");
    TextFixture f;
    f.exec("setFont('default8'); f1 = getFont(); setFont('default'); f2 = getFont()");
    ASSERT(f.getStr("f1") == "default8", "getFont() should return 'default8'");
    ASSERT(f.getStr("f2") == "default",  "getFont() should return 'default' after switching back");
}

// ============================================================
// setFont with unknown name keeps current
// ============================================================
static void test_setFont_unknown_keeps_current() {
    printf("--- setFont unknown name keeps current ---\n");
    TextFixture f;
    f.exec("setFont('default8'); setFont('nonexistent'); f = getFont()");
    ASSERT(f.getStr("f") == "default8", "setFont('nonexistent') should keep 'default8'");
}

// ============================================================
// text() with newline: second line should render below first
// ============================================================
static void test_text_newline() {
    printf("--- text() with newline ---\n");
    TextFixture f;
    // Draw two lines: "A\nB" at (0,0). Second line starts at y=8 for default font.
    f.exec("setColor(7); text('A\\nB', 0, 0)");
    ASSERT(f.hasNonZeroPixel(0, 0, 6, 8), "first line 'A' should have pixels in row 0..7");
    ASSERT(f.hasNonZeroPixel(0, 8, 6, 8), "second line 'B' should have pixels in row 8..15");
}

// ============================================================
// textWrapped(): text that exceeds maxWidth wraps to next line
// ============================================================
static void test_textWrapped_wraps() {
    printf("--- textWrapped() wraps long text ---\n");
    TextFixture f;
    // "AAA BBB" with maxWidth=24 (4 chars fit: 4*6=24). "AAA " = 4 chars = 24 px.
    // "BBB" won't fit on the same line, so it wraps.
    f.exec("setColor(7); textWrapped('AAA BBB', 0, 0, 24)");
    ASSERT(f.hasNonZeroPixel(0, 0, 24, 8), "first line 'AAA ' should have pixels");
    ASSERT(f.hasNonZeroPixel(0, 8, 24, 8), "wrapped line 'BBB' should have pixels on row 8+");
}

// ============================================================
// textWrapped(): short text stays on one line
// ============================================================
static void test_textWrapped_no_wrap_if_fits() {
    printf("--- textWrapped() no wrap when text fits ---\n");
    TextFixture f;
    // "Hi" = 2*6 = 12 px, maxWidth = 60: fits on one line
    f.exec("setColor(7); textWrapped('Hi', 0, 0, 60)");
    ASSERT(f.hasNonZeroPixel(0, 0, 12, 8), "text should render on first line");
    ASSERT(f.regionIsBlank(0, 8, 60, 8),   "no second line when text fits");
}

// ============================================================
// getTextWidth after font switch: default8 has proportional widths
// ============================================================
static void test_getTextWidth_default8() {
    printf("--- getTextWidth with default8 font ---\n");
    TextFixture f;
    f.exec("setFont('default8'); w = getTextWidth('A')");
    // defaultFont8pt7b 'A' glyph xAdvance = 5 (from glyph table at index 0x41-0x20)
    ASSERT(f.getNum("w") == 5.0, "getTextWidth('A') should be 5 for default8 font");
}

// ============================================================
// text() at size 2 draws larger glyphs
// ============================================================
static void test_text_size2_larger() {
    printf("--- text() at size 2 produces larger output ---\n");
    TextFixture f;
    f.exec("setColor(7); setTextSize(2); text('A', 0, 0)");
    // Size 2: glyph spans 12x16 pixels (6*2 x 8*2)
    ASSERT(f.hasNonZeroPixel(0, 0, 12, 16), "size 2 'A' should have pixels in 12x16 region");
    // The region beyond the scaled glyph should be blank
    ASSERT(f.regionIsBlank(12, 0, 20, 16), "area past size-2 glyph should be blank");
}

// ============================================================
// text() resets state after hot-reload (registerAll called twice)
// ============================================================
static void test_text_state_reset_on_reload() {
    printf("--- text state resets on registerAll reload ---\n");
    TextFixture f;
    f.exec("setTextSize(4); setFont('default8')");
    ASSERT(f.getNum("") == 0.0 || true, ""); // dummy; real checks below
    // Simulate reload
    f.bindings.registerAll();
    f.exec("sz = getTextSize(); fn = getFont()");
    ASSERT(f.getNum("sz") == 1.0,       "textSize should reset to 1 after registerAll()");
    ASSERT(f.getStr("fn") == "default", "font should reset to 'default' after registerAll()");
}

// ============================================================
// main
// ============================================================
int main() {
    printf("text_binding_test\n");
    printf("=================\n");

    test_getTextWidth_default_size1();
    test_getTextWidth_default_size2();
    test_getTextWidth_empty();
    test_getTextHeight_default_size1();
    test_getTextHeight_default_size3();
    test_getTextHeight_default8();
    test_text_draws_pixels();
    test_text_no_bleed();
    test_setTextSize_roundtrip();
    test_setFont_roundtrip();
    test_setFont_unknown_keeps_current();
    test_text_newline();
    test_textWrapped_wraps();
    test_textWrapped_no_wrap_if_fits();
    test_getTextWidth_default8();
    test_text_size2_larger();
    test_text_state_reset_on_reload();

    printf("\nResults: %d passed, %d failed\n", passes, failures);
    return failures == 0 ? 0 : 1;
}
