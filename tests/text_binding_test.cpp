/**
 * @file text_binding_test.cpp
 * @brief The Lua text surface (ADR-0029, Tomodachi #476): gfx.text,
 *        gfx.setFont, gfx.setTextWrap, gfx.setTextSize, gfx.getTextWidth and
 *        the outline/effect setters.
 *
 * Lua draws through the same TextRenderer and the same per-layer state native
 * code uses, so every pixel check here is against a native draw.
 */
#include <enjin2/graphics/canvas.hpp>
#include <enjin2/graphics/faces/faces.hpp>
#include <enjin2/graphics/text_renderer.hpp>
#include <enjin2/scripting/bindings.hpp>
#include <enjin2/scripting/lua_engine.hpp>

#include <cstdio>
#include <cstring>
#include <string>

using namespace enjin2;

static int passes = 0;
static int failures = 0;

#define ASSERT(cond, msg)                                                     \
    do {                                                                      \
        if (!(cond)) {                                                        \
            fprintf(stderr, "FAIL [line %d]: %s\n", __LINE__, (msg));         \
            failures++;                                                       \
        } else {                                                              \
            passes++;                                                         \
        }                                                                     \
    } while (0)

using Layer = Canvas4<96, 64>;

// Two layers, as a host gives an applet.
struct TextFixture {
    Layer layers[2];
    LuaCanvas wrap0{&layers[0]};
    LuaCanvas wrap1{&layers[1]};
    LuaCanvas* canvases[2] = {&wrap0, &wrap1};
    bool visible[2] = {true, true};
    LuaEngine engine;
    LuaBindings bindings{&engine};

    TextFixture() {
        engine.initialize();
        bindings.registerAll();
        bindings.setLayers(canvases, 2, visible);
        for (auto& l : layers) l.clear(Pixel4(PALETTE_TRANSPARENT));
    }
    LuaResult exec(const std::string& code) { return engine.executeString(code.c_str()); }
    double num(const char* name) { return engine.getGlobalNumber(name); }
};

static Layer blank() {
    Layer l;
    l.clear(Pixel4(PALETTE_TRANSPARENT));
    return l;
}

static bool same(const Layer& a, const Layer& b) {
    for (int y = 0; y < 64; ++y)
        for (int x = 0; x < 96; ++x)
            if (a.getPixel(int16_t(x), int16_t(y)).value != b.getPixel(int16_t(x), int16_t(y)).value) return false;
    return true;
}

static void test_text_is_the_native_draw() {
    printf("--- gfx.text draws what TextRenderer draws, y the line's top ---\n");
    TextFixture f;
    ASSERT(f.exec("gfx.setColor(gfx.COLOR.ACCENT); lines = gfx.text('Track 01', 3, 2)").success, "gfx.text runs");
    ASSERT(f.num("lines") == 1, "returns the lines drawn");
    Layer ref = blank();
    TextRenderer<Pixel4>(ref).setTextColor(ROLE_ACCENT).draw("Track 01", 3, 2);
    ASSERT(same(f.layers[0], ref), "monogram, accent, the line's top at y");

    TextFixture g;
    g.exec("gfx.setFont('display'); gfx.setTextSize(2); gfx.text('Hi', 90, 0, 'right')");
    Layer ref2 = blank();
    TextRenderer<Pixel4>(ref2).setFont(faces::display).setTextSize(2).draw("Hi", 90, 0, TextAlign::Right);
    ASSERT(same(g.layers[0], ref2), "face, size and align reach the same draw");
}

static void test_measure() {
    printf("--- gfx.getTextWidth is the advance in the layer's face and size ---\n");
    TextFixture f;
    f.exec("a = gfx.getTextWidth('Track 01'); gfx.setTextSize(2); b = gfx.getTextWidth('ab'); "
           "gfx.setTextSize(1); gfx.setFont('body'); c = gfx.getTextWidth('Hello'); d = gfx.getTextWidth()");
    ASSERT(f.num("a") == 48, "monogram: 6 px a character");
    ASSERT(f.num("b") == 24, "times the size");
    ASSERT(f.num("c") == text::width(faces::body, "Hello"), "body");
    ASSERT(f.num("d") == 0, "no string: 0");
}

static void test_wrap_modes() {
    printf("--- gfx.setTextWrap: wrap | clip | ellipsis, at a width or the layer's edge ---\n");
    TextFixture f;
    f.exec("n1 = gfx.text('aaa bbb', 0, 0)");
    ASSERT(f.num("n1") == 1, "fits before the layer's edge: one line");
    f.exec("gfx.setTextWrap('wrap', 30); n2 = gfx.text('aaa bbb', 0, 20)");
    ASSERT(f.num("n2") == 2, "wraps at the given width");
    f.exec("gfx.setTextWrap('ellipsis', 30); n3 = gfx.text('abcdefgh', 0, 40)");
    ASSERT(f.num("n3") == 1, "ellipsis: one line");
    Layer ref = blank();
    TextRenderer<Pixel4> r(ref);
    r.draw("aaa bbb", 0, 0);
    r.setTextWrap(TextOverflow::Wrap, 30).draw("aaa bbb", 0, 20);
    r.setTextWrap(TextOverflow::Ellipsis, 30).draw("abcdefgh", 0, 40);
    ASSERT(same(f.layers[0], ref), "the same pixels as native");
    ASSERT(!f.exec("gfx.setTextWrap('scroll')").success, "another overflow raises");
    ASSERT(!f.exec("gfx.setTextWrap('clip', -3)").success, "a width under 1 raises");
}

static void test_state_is_per_layer() {
    printf("--- each layer keeps its own text state ---\n");
    TextFixture f;
    f.exec("gfx.setLayer(1); gfx.setFont('display'); gfx.setTextSize(3)");
    f.exec("gfx.setLayer(2); w2 = gfx.getTextWidth('ab')");
    ASSERT(f.num("w2") == 12, "layer 2 is still monogram at size 1");
    f.exec("gfx.setLayer(1); w1 = gfx.getTextWidth('ab')");
    ASSERT(f.num("w1") == text::width(faces::display, "ab", 3), "layer 1 kept display at 3");
    ASSERT(f.layers[0].text().face == &faces::display, "the setting is on the canvas, where native code sees it");
}

static void test_reload_resets_the_state() {
    printf("--- a reload starts every layer in monogram, wrap, size 1 ---\n");
    TextFixture f;
    f.exec("gfx.setFont('body'); gfx.setTextSize(4); gfx.setTextWrap('clip', 10); gfx.setTextOutline(true)");
    f.bindings.setLayers(f.canvases, 2, f.visible);
    const TextState& st = f.layers[0].text();
    ASSERT(st.face == nullptr && st.size == 1 && st.overflow == TextOverflow::Wrap && st.width == 0 && !st.outline,
           "defaults again");
}

static void test_faces() {
    printf("--- gfx.setFont takes monogram, body and display ---\n");
    TextFixture f;
    for (const char* name : {"monogram", "body", "display"})
        ASSERT(f.exec(std::string("gfx.setFont('") + name + "')").success, name);
    for (const char* gone : {"default", "default8", "nonexistent"})
        ASSERT(!f.exec(std::string("gfx.setFont('") + gone + "')").success, gone);
}

static void test_outline_and_effect() {
    printf("--- Lua reaches the outline and the shader ---\n");
    TextFixture f;
    f.exec("gfx.setTextOutline(true); gfx.text('HH', 4, 4)");
    Layer ref = blank();
    TextRenderer<Pixel4>(ref).setOutline(true).draw("HH", 4, 4);
    ASSERT(same(f.layers[0], ref), "the outline, as native draws it");
    ASSERT(!f.exec("gfx.setTextOutline(true, 9)").success, "an outline colour 8-14 raises");
    ASSERT(f.exec("gfx.setTextEffect(nil)").success, "nil drops the shader");
}

static void test_newline_and_utf8() {
    printf("--- newlines and UTF-8 ---\n");
    TextFixture f;
    f.exec("n = gfx.text('a\\nb', 0, 0); w = gfx.getTextWidth('\\u{20AC}')");
    ASSERT(f.num("n") == 2, "a newline starts a line");
    ASSERT(f.num("w") == text::tofuAdvance(faces::monogram), "a missing character measures one tofu");
}

static void test_mistakes_raise() {
    printf("--- an unknown face or align, or a size off 1..255, raises (ADR-0030) ---\n");
    TextFixture f;
    f.exec("gfx.setFont('body'); ok, why = pcall(gfx.setFont, 'nonexistent'); "
           "raised = (not ok) and why:find(\"unknown font 'nonexistent'\", 1, true) ~= nil");
    ASSERT(f.exec("assert(raised)").success, "gfx.setFont('nonexistent') raises, naming the font");
    ASSERT(f.layers[0].text().face == &faces::body, "and keeps the current face");
    ASSERT(!f.exec("gfx.text('a', 0, 0, 'middle')").success, "an unknown align raises");
    ASSERT(!f.exec("gfx.text('a', 0.5, 0)").success, "a fractional x raises");
    ASSERT(!f.exec("gfx.setTextSize(0)").success, "size 0 raises");
    ASSERT(!f.exec("gfx.setTextSize(256)").success, "size 256 raises");
    ASSERT(f.layers[0].text().size == 1, "a refused size keeps the current one");
}

// Native callers draw through the same state (Tomodachi #478 follow-up):
// selectFont / setDrawColor / fillRect / drawText are gfx.setFont,
// gfx.setColor, gfx.rectangle("fill") and gfx.text: the same pixels, and the
// state they set stays on the layer as the gfx calls would leave it.
static void test_native_draw_is_gfx_draw() {
    printf("--- native draw = gfx draw ---\n");
    TextFixture lua;
    lua.exec("gfx.setTextSize(2) gfx.setFont('body') gfx.setColor(gfx.COLOR.SURFACE) "
             "gfx.rectangle('fill', 2, 2, 80, 30) gfx.setColor(gfx.COLOR.ACCENT) "
             "gfx.text('Hi', 4, 10) gfx.text('Yo', 90, 10, 'right') "
             "gfx.text('c', 48, 40, 'center')");
    TextFixture native;
    native.exec("gfx.setTextSize(2)");
    ASSERT(native.bindings.selectFont("body"), "selectFont finds a face");
    native.bindings.setDrawColor(ROLE_SURFACE);
    native.bindings.fillRect(2, 2, 80, 30);
    native.bindings.setDrawColor(ROLE_ACCENT);
    ASSERT(native.bindings.drawText("Hi", 4, 10) == 1, "drawText returns the lines drawn");
    native.bindings.drawText("Yo", 90, 10, TextAlign::Right);
    native.bindings.drawText("c", 48, 40, TextAlign::Center);
    int inked = 0;
    for (int16_t y = 0; y < 64; ++y)
        for (int16_t x = 0; x < 96; ++x) inked += lua.layers[0].getPixel(x, y).value == ROLE_ACCENT;
    ASSERT(inked > 20, "the reference drew text");
    ASSERT(same(lua.layers[0], native.layers[0]), "native draw writes gfx's pixels");
    ASSERT(native.layers[0].text().face == &faces::body, "the face native code set is on the layer");
    native.exec("c = gfx.getColor()");
    ASSERT(native.num("c") == ROLE_ACCENT, "the script reads the colour native code set");
    ASSERT(!native.bindings.selectFont("nope"), "a name that is not a face is refused");
    ASSERT(native.layers[0].text().face == &faces::body, "a refused face keeps the current one");
}

int main() {
    test_text_is_the_native_draw();
    test_measure();
    test_wrap_modes();
    test_state_is_per_layer();
    test_reload_resets_the_state();
    test_faces();
    test_outline_and_effect();
    test_newline_and_utf8();
    test_mistakes_raise();
    test_native_draw_is_gfx_draw();
    printf("\ntext_binding_test: %d passed, %d failed\n", passes, failures);
    return failures ? 1 : 0;
}
