// sprite_flip_test.cpp — tests drawSprite flipH, flipV, rotate90
// Uses pikachu.h (38×38 single-frame) sprite data from the test directory.
#include <enjin2/scripting/bindings.hpp>
#include <enjin2/scripting/lua_engine.hpp>
#include <enjin2/graphics/canvas.hpp>
#include "pikachu.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

using namespace enjin2;

#define ASSERT(cond, msg) \
    do { if (!(cond)) { printf("FAIL: %s (line %d)\n", msg, __LINE__); exit(1); } } while(0)

// A small 4×4 test sprite for precise pixel verification
// Layout (row-major, each byte = palette index, 15=transparent):
//   Row 0:  1  2  3  4
//   Row 1:  5  6  7  8
//   Row 2:  9 10 11 12
//   Row 3: 13 14  0  0
static const uint8_t small_4x4_data[] = {
    1,  2,  3,  4,
    5,  6,  7,  8,
    9, 10, 11, 12,
   13, 14,  0,  0,
};

struct SpriteFixture {
    LuaEngine engine;
    LuaBindings bindings;
    Canvas4<128, 128> canvas;
    LuaCanvas luaCanvas;

    SpriteFixture()
        : bindings(&engine)
        , luaCanvas(&canvas)
    {
        engine.initialize();
        bindings.registerAll();
        bindings.setCanvas(&luaCanvas);

        // Push sprite data as lightuserdata globals
        lua_State* L = engine.getState();
        lua_pushlightuserdata(L, const_cast<uint8_t*>(small_4x4_data));
        lua_setglobal(L, "_td");
        lua_pushlightuserdata(L, const_cast<uint8_t*>(pikachu_data));
        lua_setglobal(L, "_pd");

        // Create sprites: slot 0 = small 4x4, slot 1 = pikachu 38x38
        engine.executeString("_s0 = newSprite(_td, 4, 4, 1, 1)");
        engine.executeString("_s1 = newSprite(_pd, 38, 38, 1, 1)");
    }

    void exec(const char* code) { engine.executeString(code); }
    uint8_t px(int16_t x, int16_t y) { return canvas.getPixel(x, y).value; }
    void clear() { canvas.clear(Pixel4(0)); }
};

int main() {
    printf("=== sprite_flip_test ===\n");

    SpriteFixture f;

    // ── TEST 1: Normal draw (no flip) ──
    {
        f.clear();
        f.exec("drawSprite(_s0, 10, 20)");
        ASSERT(f.px(10, 20) == 1, "normal: top-left = 1");
        ASSERT(f.px(13, 20) == 4, "normal: top-right = 4");
        ASSERT(f.px(10, 23) == 13, "normal: bottom-left = 13");
        ASSERT(f.px(13, 23) == 0, "normal: bottom-right = 0");
        ASSERT(f.px(11, 21) == 6, "normal: (1,1) = 6");
        printf("  PASS: normal draw\n");
    }

    // ── TEST 2: flipH ──
    {
        f.clear();
        f.exec("drawSprite(_s0, 10, 20, true, false, false)");
        ASSERT(f.px(10, 20) == 4, "flipH: top-left = 4");
        ASSERT(f.px(13, 20) == 1, "flipH: top-right = 1");
        ASSERT(f.px(10, 23) == 0, "flipH: bottom-left = 0");
        ASSERT(f.px(13, 23) == 13, "flipH: bottom-right = 13");
        printf("  PASS: flipH\n");
    }

    // ── TEST 3: flipV ──
    {
        f.clear();
        f.exec("drawSprite(_s0, 10, 20, false, true, false)");
        ASSERT(f.px(10, 20) == 13, "flipV: top-left = 13");
        ASSERT(f.px(13, 20) == 0, "flipV: top-right = 0");
        ASSERT(f.px(10, 23) == 1, "flipV: bottom-left = 1");
        ASSERT(f.px(13, 23) == 4, "flipV: bottom-right = 4");
        printf("  PASS: flipV\n");
    }

    // ── TEST 4: flipH + flipV (180° rotation) ──
    {
        f.clear();
        f.exec("drawSprite(_s0, 10, 20, true, true, false)");
        ASSERT(f.px(10, 20) == 0, "flipHV: top-left = 0");
        ASSERT(f.px(13, 20) == 13, "flipHV: top-right = 13");
        ASSERT(f.px(10, 23) == 4, "flipHV: bottom-left = 4");
        ASSERT(f.px(13, 23) == 1, "flipHV: bottom-right = 1");
        printf("  PASS: flipH + flipV\n");
    }

    // ── TEST 5: rotate90 ──
    {
        f.clear();
        f.exec("drawSprite(_s0, 10, 20, false, false, true)");
        // 90° CW: dest(cellH-1-fy, fx) reads src(fx, fy)
        // original (0,0)=1 → dst at (cellH-1-0, 0)=(3,0) → pixel(13,20)
        // original (3,0)=4 → dst at (3, 3) → pixel(13,23)
        // original (0,3)=13 → dst at (0, 0) → pixel(10,20)
        ASSERT(f.px(10 + 3, 20 + 0) == 1, "rot90: (3,0) = 1");
        ASSERT(f.px(10 + 3, 20 + 3) == 4, "rot90: (3,3) = 4");
        ASSERT(f.px(10 + 0, 20 + 0) == 13, "rot90: (0,0) = 13");
        printf("  PASS: rotate90\n");
    }

    // ── TEST 6: backward compatibility ──
    {
        f.clear();
        f.exec("drawSprite(_s0, 0, 0)");
        ASSERT(f.px(0, 0) == 1, "compat: drawSprite(h,x,y) still works");
        printf("  PASS: backward compatibility\n");
    }

    // ── TEST 7: pikachu flipH sanity check ──
    {
        f.clear();
        f.exec("drawSprite(_s1, 0, 0)");
        uint8_t norm_l = f.px(0, 0);
        uint8_t norm_r = f.px(37, 0);

        f.clear();
        f.exec("drawSprite(_s1, 0, 0, true, false, false)");
        uint8_t flip_l = f.px(0, 0);
        uint8_t flip_r = f.px(37, 0);

        ASSERT(flip_l == norm_r, "pikachu flipH: left matches original right");
        ASSERT(flip_r == norm_l, "pikachu flipH: right matches original left");
        printf("  PASS: pikachu flipH\n");
    }

    printf("=== sprite_flip_test: ALL PASSED ===\n");
    return 0;
}
