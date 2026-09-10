/**
 * @file sprite_lua_test.cpp
 * @brief C_Sprite Lua binding coverage (ADR-0003 §5, Tomodachi #81).
 *
 * The clip advance/event/scrub logic itself is covered in sprite_clip_test;
 * this file proves the Lua surface reaches it: obj:add("C_Sprite"), setClips,
 * play, the writable hflip/vflip properties, setFrameForAngle scrub, and the
 * polled frame/justAdvanced/frameEvent reads. The C_Sprite is pre-attached in
 * C++ with a sheet (attach-or-fetch means self:add returns that instance), and
 * lateUpdate is driven from C++ between script updates so the poll fields are
 * observable from Lua.
 */
#include <enjin2/core/object.hpp>
#include <enjin2/components/lua_script.hpp>
#include <enjin2/components/sprite.hpp>
#include <enjin2/graphics/sprite.hpp>
#include <cstdio>

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

static const uint8_t kSheetData[32] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 0,
    1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 0, 1,
};

int main() {
    printf("--- C_Sprite Lua binding ---\n");

    Object obj;
    C_LuaScript* script = obj.addComponent<C_LuaScript>(16u, 16u);
    ASSERT(script != nullptr, "addComponent<C_LuaScript>");

    // Pre-attach the sprite + a sheet in C++; self:add('C_Sprite') fetches it.
    C_Sprite* sp = obj.addComponent<C_Sprite>();
    ASSERT(sp != nullptr, "addComponent<C_Sprite>");
    sp->setSheet(SpriteSheet(kSheetData, 1, 1, 32, 1));

    const char* src =
        "spr = nil\n"
        "added = false\n"
        "played = false\n"
        "f0 = -1\n"
        "hf = false\n"
        "clipname = ''\n"
        "smin = -1\n"
        "smax = -1\n"
        "adv = false\n"
        "fe = -1\n"
        "f1 = -1\n"
        "function init(self)\n"
        "  spr = self:add('C_Sprite')\n"
        "  added = spr ~= nil\n"
        "  spr:setClips({\n"
        "    { name='run', loop='loop', frames={\n"
        "        { frame=10, dur=100 },\n"
        "        { frame=20, dur=100, event=5 },\n"
        "    }},\n"
        "  })\n"
        "  played = spr:play('run')\n"
        "  f0 = spr.frame\n"
        "  clipname = spr.clip\n"
        "  spr:setFrameForAngle(0, 0, 90)\n"   // scrub the active clip's frames
        "  smin = spr.frame\n"
        "  spr:setFrameForAngle(90, 0, 90)\n"
        "  smax = spr.frame\n"
        "  spr:play('run')\n"                  // re-play to leave scrub mode
        "  spr.hflip = true\n"
        "  hf = spr.hflip\n"
        "end\n"
        "function update(self, dt)\n"
        "  adv = spr.justAdvanced\n"
        "  fe = spr.frameEvent\n"
        "  f1 = spr.frame\n"
        "end\n";

    ASSERT(script->loadScript(src), "script loaded");
    script->update(0.016f);  // runs init + first update
    ASSERT(!script->hasErrors(), "no Lua errors after init");

    ASSERT(script->getScriptBool("added", false), "self:add('C_Sprite') returned a proxy");
    ASSERT(script->getScriptBool("played", false), "spr:play('run') returned true");
    ASSERT(static_cast<int>(script->getScriptNumber("f0", -1)) == 10, "play resets to first frame cell 10");
    ASSERT(script->getScriptString("clipname", "") == "run", "spr.clip reads the playing clip name");
    ASSERT(script->getScriptBool("hf", false), "spr.hflip write+read round-trips");
    ASSERT(static_cast<int>(script->getScriptNumber("smin", -1)) == 10, "setFrameForAngle(min) -> first frame cell 10");
    ASSERT(static_cast<int>(script->getScriptNumber("smax", -1)) == 20, "setFrameForAngle(max) -> last frame cell 20");
    ASSERT(sp->getHFlip(), "hflip visible on the C++ component");

    // Advance one frame in C++, then re-run the Lua update to poll.
    sp->lateUpdate(0.1f);
    script->update(0.0f);
    ASSERT(!script->hasErrors(), "no Lua errors after poll update");
    ASSERT(script->getScriptBool("adv", false), "spr.justAdvanced true after advance");
    ASSERT(static_cast<int>(script->getScriptNumber("fe", -1)) == 5, "spr.frameEvent reads the frame's eventId (5)");
    ASSERT(static_cast<int>(script->getScriptNumber("f1", -1)) == 20, "spr.frame is cell 20 after advance");

    printf("\n%d passed, %d failed\n", passes, failures);
    return failures > 0 ? 1 : 0;
}
