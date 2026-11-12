/**
 * @file mistakes_raise_test.cpp
 * @brief ADR-0030 in enjin's drawing and tween calls: a programming mistake
 *        (an unknown name, a handle never loaded, a value off its range, a
 *        wrong type) raises, naming it; it is not a quiet default.
 */
#include <enjin2/scripting/bindings.hpp>
#include <enjin2/scripting/lua_engine.hpp>
#include <enjin2/graphics/canvas.hpp>
#include <cstdio>
#include <string>

using namespace enjin2;

static int passes = 0;
static int failures = 0;

#define ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            fprintf(stderr, "FAIL [line %d]: %s\n", __LINE__, std::string(msg).c_str()); \
            failures++; \
        } else { \
            passes++; \
        } \
    } while(0)

namespace {

struct Fixture {
    Canvas8<64, 64> canvas;
    LuaCanvas luaCanvas;
    LuaEngine engine;
    LuaBindings bindings;
    Fixture() : luaCanvas(&canvas), bindings(&engine) {
        engine.initialize();
        bindings.registerAll();
        bindings.setCanvas(&luaCanvas);
    }
    // "" when `code` ran, else its error.
    std::string error(const char* code) {
        const std::string wrapped = std::string("ok, why = pcall(function() ") + code + " end)";
        engine.executeString(wrapped.c_str());
        engine.executeString("why = ok and '' or tostring(why)");
        return engine.getGlobalString("why");
    }
};

bool says(const std::string& err, const char* what) { return err.find(what) != std::string::npos; }

} // namespace

int main() {
    printf("mistakes_raise_test: ADR-0030 in gfx.* and engine.tween\n\n");
    Fixture f;
    ASSERT(says(f.error("gfx.setFont('nope')"), "unknown font 'nope'"), "gfx.setFont: an unknown font raises");
    ASSERT(says(f.error("gfx.text('a', 1, 1, 'middle')"), "unknown align 'middle'"),
           "gfx.text: an unknown align raises");
    ASSERT(f.error("gfx.text('a', 1.5, 1)") != "", "gfx.text: a fractional x raises");
    ASSERT(says(f.error("gfx.setTextWrap('scroll')"), "unknown overflow 'scroll'"),
           "gfx.setTextWrap: an unknown overflow raises");
    ASSERT(says(f.error("gfx.setTextSize(0)"), "size is 1..255"), "gfx.setTextSize: 0 raises");
    ASSERT(says(f.error("gfx.setTextSize(256)"), "size is 1..255"), "gfx.setTextSize: 256 raises");
    ASSERT(f.error("gfx.setColor('red')") != "", "gfx.setColor: a non-number raises");
    ASSERT(f.error("gfx.point('a', 1)") != "", "gfx.point: a non-number raises");
    ASSERT(says(f.error("gfx.freeSprite(3)"), "unknown sprite handle 3"), "gfx.freeSprite: a handle never loaded raises");
    ASSERT(says(f.error("gfx.drawSprite(3, 0, 0)"), "unknown sprite handle 3"), "gfx.drawSprite: a handle never loaded raises");
    ASSERT(says(f.error("gfx.polyline({1, 2, 3})"), "odd count of numbers"),
           "gfx.polyline: a lone x raises");
    ASSERT(says(f.error("engine.tween.to({x = 0}, {x = 1}, 1, 'bouncy')"), "unknown easing 'bouncy'"),
           "engine.tween.to: an unknown easing raises");
    // The right ones still run.
    ASSERT(f.error("gfx.setFont('body'); gfx.text('a', 30, 1, 'center'); gfx.setTextWrap('clip', 20); "
                   "gfx.setTextSize(2); gfx.setColor(3); gfx.point(1, 1); "
                   "assert(engine.tween.to({x = 0}, {x = 1}, 1, 'easeInOut'))") == "",
           "the known names and in-range values run: " + f.error("gfx.setFont('body')"));
    printf("\n%d passed, %d failed\n", passes, failures);
    return failures == 0 ? 0 : 1;
}
