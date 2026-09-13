/**
 * @file numerals_lua_test.cpp
 * @brief HUD numerals Lua surface (ADR-0003 §8, Tomodachi #83).
 *
 * Proves the Lua bindings reach the same layout the C++ path uses:
 * gfx.number / gfx.timer draw a loaded digit strip onto the canvas (the pixels
 * are checked against the synthetic strip's frame = fill+1 encoding), and
 * engine.hud.rollingCounter / engine.hud.timer expose the value objects with
 * their full method surface. Correctness of the layout/value logic itself lives
 * in numerals_test.cpp; this file is the parity half.
 */
#include <enjin2/scripting/bindings.hpp>
#include <enjin2/scripting/lua_engine.hpp>
#include <enjin2/graphics/canvas.hpp>
#include <enjin2/graphics/njn2.hpp>
#include <cstdio>
#include <fstream>
#include <vector>

using namespace enjin2;

static int passes = 0;
static int failures = 0;

#define ASSERT(cond, msg)                       \
    do {                                        \
        if (!(cond)) {                          \
            fprintf(stderr, "FAIL: %s\n", msg); \
            failures++;                         \
        } else {                                \
            passes++;                           \
        }                                       \
    } while (0)

// Write a synthetic 4x4, 11-frame digit strip (.njn v2) where frame f is a solid
// fill of index (f+1) — the same encoding numerals_test.cpp uses to recover which
// frame drew into a cell. 11 frames so the colon (frame 10) is present.
static void writeSynthStrip(const char* path) {
    std::vector<uint8_t> pix;
    for (int f = 0; f < 11; ++f) {
        for (int i = 0; i < 16; ++i) pix.push_back(static_cast<uint8_t>(f + 1));
    }
    NjnV2Writer w;
    njn2WriteMeta(w, 4, 4, 11, 1);
    njn2WritePixl(w, pix.data(), static_cast<uint32_t>(pix.size()));
    std::vector<uint8_t> buf;
    w.finalise(buf);
    std::ofstream out(path, std::ios::binary);
    out.write(reinterpret_cast<const char*>(buf.data()),
              static_cast<std::streamsize>(buf.size()));
}

struct Fixture {
    LuaEngine engine;
    LuaBindings bindings;
    Canvas4<64, 64> canvas;
    LuaCanvas luaCanvas;

    Fixture() : bindings(&engine), luaCanvas(&canvas) {
        engine.initialize();
        bindings.registerAll();
        bindings.setCanvas(&luaCanvas);
        bindings.setAssetPath(".");
    }

    bool exec(const char* code) {
        LuaResult r = engine.executeString(code);
        if (!r.success) fprintf(stderr, "lua error: %s\n", r.error.c_str());
        return r.success;
    }
    double num(const char* n) { return engine.getGlobalNumber(n, -999.0); }
    bool   flag(const char* n) { return engine.getGlobalBool(n, false); }
    uint8_t px(int x, int y) { return canvas.getPixel(static_cast<int16_t>(x),
                                                      static_cast<int16_t>(y)).value; }
};

int main() {
    printf("--- HUD numerals (Lua) ---\n");
    writeSynthStrip("./numerals_strip.njn");

    Fixture fx;
    fx.canvas.clear(Pixel4(0));

    // Load the strip and draw a left-aligned number.
    ASSERT(fx.exec(
        "h = engine.sprite.load('numerals_strip')\n"
        "w = gfx.number(0, 0, 42, { strip = h })\n"), "load + gfx.number exec");
    ASSERT(fx.num("h") >= 0, "strip handle valid");
    ASSERT(fx.num("w") == 8, "gfx.number width parity (= 8)");
    ASSERT(fx.px(2, 2) == 5, "Lua cell0 shows frame 4 (fill 5)");
    ASSERT(fx.px(6, 2) == 3, "Lua cell1 shows frame 2 (fill 3)");

    // Right alignment: x=20 is the right edge, so a width-8 field starts at 12.
    fx.canvas.clear(Pixel4(0));
    ASSERT(fx.exec("gfx.number(20, 0, 42, { strip = h, align = 'right' })"),
           "gfx.number right-align exec");
    ASSERT(fx.px(14, 2) == 5, "Lua right-anchored cell0 at x=12");

    // Zero pad.
    fx.canvas.clear(Pixel4(0));
    ASSERT(fx.exec("gfx.number(0, 0, 7, { strip = h, pad = 3 })"), "gfx.number pad exec");
    ASSERT(fx.px(2, 2) == 1 && fx.px(6, 2) == 1 && fx.px(10, 2) == 8,
           "Lua pad3 -> 007 (fills 1,1,8)");

    // Timer draw: 1:05 -> colon (frame 10, fill 11) in cell 2.
    fx.canvas.clear(Pixel4(0));
    ASSERT(fx.exec("tw = gfx.timer(0, 0, 65000, { strip = h })"), "gfx.timer exec");
    ASSERT(fx.num("tw") == 20, "gfx.timer width parity (= 20)");
    ASSERT(fx.px(10, 2) == 11, "Lua timer colon cell shows frame 10");
    ASSERT(fx.px(18, 2) == 6, "Lua timer seconds-units 5 -> frame5 fill 6");

    // engine.hud.rollingCounter surface.
    ASSERT(fx.exec(
        "rc = engine.hud.rollingCounter(0)\n"
        "rc:set(100)\n"
        "for i = 1, 240 do rc:update(1/30) end\n"
        "rcv = rc:value()\n"
        "rct = rc:target()\n"), "rollingCounter exec");
    ASSERT(fx.num("rcv") == 100, "Lua RollingCounter value() lands on 100");
    ASSERT(fx.num("rct") == 100, "Lua RollingCounter target() = 100");

    // engine.hud.timer surface.
    ASSERT(fx.exec(
        "t = engine.hud.timer(3000)\n"
        "t:update(1.0)\n"
        "t_ms = t:millis()\n"
        "t_done1 = t:done()\n"
        "t:update(5.0)\n"
        "t_done2 = t:done()\n"
        "t_fmt = (t:format() == '00:00')\n"
        "t2fmt = (engine.hud.timer(65000):format() == '01:05')\n"), "hud.timer exec");
    ASSERT(fx.num("t_ms") == 2000, "Lua Timer millis after 1s = 2000");
    ASSERT(fx.flag("t_done1") == false, "Lua Timer not done at 2s");
    ASSERT(fx.flag("t_done2") == true, "Lua Timer done after overshoot");
    ASSERT(fx.flag("t_fmt"), "Lua Timer format at 0 = 00:00");
    ASSERT(fx.flag("t2fmt"), "Lua Timer format 65000ms = 01:05");

    printf("passes=%d failures=%d\n", passes, failures);
    return failures == 0 ? 0 : 1;
}
