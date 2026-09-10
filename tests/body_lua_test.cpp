/**
 * @file body_lua_test.cpp
 * @brief C_Body + ColliderSet Lua binding coverage (ADR-0003 §4, Tomodachi #80).
 *
 * The swept/depenetration/flipper physics itself is covered in body_test; this
 * file proves the Lua surface reaches it: engine.scene.colliders() + addSeg/
 * addCircle/addFlipper, obj:add("C_Body", {...}) writable config, body:step(dt),
 * and the polled contact buffer (body:numContacts()/body:contact(i)) — the
 * "contact buffer polls in Lua" half of the Done-when, with no callback registry.
 */
#include <enjin2/core/scene.hpp>
#include <enjin2/core/object.hpp>
#include <enjin2/components/body.hpp>
#include <enjin2/components/tilemap.hpp>
#include <enjin2/graphics/tilemap_asset.hpp>
#include <enjin2/graphics/sprite.hpp>
#include <enjin2/scripting/bindings.hpp>
#include <enjin2/scripting/lua_engine.hpp>
#include <cstdio>
#include <cmath>

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

struct Fixture {
    LuaEngine engine;
    LuaBindings bindings;
    Scene scene;

    Fixture() : bindings(&engine), scene(1u) {
        engine.initialize();
        bindings.registerAll();
        bindings.setActiveScene(&scene);
    }

    void exec(const char* code) {
        LuaResult r = engine.executeString(code);
        ASSERT(r.success, "Lua exec succeeds");
    }
    double num(const char* n) { return engine.getGlobalNumber(n, -999.0); }
    bool   b(const char* n)   { return engine.getGlobalBool(n, false); }
};

static void test_body_and_colliders_surface() {
    printf("--- BODY-LUA-01: C_Body writable config + collider set building ---\n");
    Fixture f;

    f.exec(
        "col = engine.scene.colliders()\n"
        "col:addSeg(6, 150, 154, 150, 0.5, 0)\n"     // floor
        "col:addCircle(80, 60, 9, 1.15, 1)\n"       // bouncy bumper
        "col:addFlipper(55, 150, 30, 0, -1.2, 0.2)\n"
        "total = col:count()\n"
        "nf = col:numFlippers()\n"
        "o = engine.scene.spawn()\n"
        "body = o:add('C_Body', {radius=4, restitution=0.3, drag=0.0})\n"
        "body:setColliders(col)\n"
        "body.x = 80\n"
        "body.y = 40\n"
        "r_body = body.radius\n"
        "e_body = body.restitution\n"
        "x_body = body.x\n"
    );

    ASSERT(static_cast<int>(f.num("total")) == 3, "colliders:count() == 3 (seg+circle+flipper)");
    ASSERT(static_cast<int>(f.num("nf")) == 1, "colliders:numFlippers() == 1");
    ASSERT(std::fabs(f.num("r_body") - 4.0) < 1e-4, "C_Body radius set via add params");
    ASSERT(std::fabs(f.num("e_body") - 0.3) < 1e-4, "C_Body restitution set via add params");
    ASSERT(std::fabs(f.num("x_body") - 80.0) < 1e-4, "C_Body x writable");
}

static void test_floor_rest_lua() {
    printf("--- BODY-LUA-02: ball rests on a floor, no tunnelling (from Lua) ---\n");
    Fixture f;

    f.exec(
        "col = engine.scene.colliders()\n"
        "col:addSeg(-100, 120, 260, 120, 0.2, 0)\n"  // floor at y=120
        "o = engine.scene.spawn()\n"
        "body = o:add('C_Body', {radius=4, restitution=0.2})\n"
        "body:setColliders(col)\n"
        "body.x = 80\n"
        "body.y = 60\n"
        "body:setGravity(0, 320)\n"
        "for i=1,90 do body:step(1/30) end\n"
        "restY = body.y\n"
        "restVy = body.vy\n"
    );

    const double restY = f.num("restY");
    ASSERT(std::fabs(restY - 116.0) < 1.0, "ball rests on the floor (y≈116)");
    ASSERT(std::fabs(f.num("restVy")) < 8.0, "ball settles to rest");
    ASSERT(restY <= 120.0, "ball never tunnelled below the floor");
}

static void test_bouncy_contact_poll() {
    printf("--- BODY-LUA-03: contact buffer polls kind + relSpeed ---\n");
    Fixture f;

    // A ball dropped straight onto a bouncy bumper circle.
    f.exec(
        "col = engine.scene.colliders()\n"
        "col:addCircle(80, 120, 9, 1.15, 1)\n"  // BOUNCY (kind 1)
        "o = engine.scene.spawn()\n"
        "body = o:add('C_Body', {radius=4, restitution=0.5})\n"
        "body:setColliders(col)\n"
        "body.x = 80\n"
        "body.y = 80\n"
        "body:setGravity(0, 320)\n"
        "maxRel = -1\n"
        "sawBouncy = false\n"
        "for i=1,40 do\n"
        "  body:step(1/30)\n"
        "  local n = body:numContacts()\n"
        "  for k=1,n do\n"
        "    local c = body:contact(k)\n"
        "    if c.kind == 1 then sawBouncy = true end\n"
        "    if c.relSpeed > maxRel then maxRel = c.relSpeed end\n"
        "  end\n"
        "end\n"
    );

    ASSERT(f.b("sawBouncy"), "contact buffer polled a BOUNCY (kind 1) contact");
    ASSERT(f.num("maxRel") >= 0.0, "relSpeed polled from contact (non-negative)");
}

static void test_flipper_launch_lua() {
    printf("--- BODY-LUA-04: flipper impulse launches the ball (from Lua) ---\n");
    Fixture f;

    // Horizontal flipper; ball rests, then setFlipperTarget flips it up.
    f.exec(
        "col = engine.scene.colliders()\n"
        "col:addFlipper(50, 150, 30, 0, -1.2, 0.2)\n"
        "o = engine.scene.spawn()\n"
        "body = o:add('C_Body', {radius=4, restitution=0.2})\n"
        "body:setColliders(col)\n"
        "body.x = 65\n"
        "body.y = 146\n"
        "body:setGravity(0, 320)\n"
        "for i=1,50 do body:step(1/30) end\n"
        "settleY = body.y\n"
        "col:setFlipperTarget(1, -1.2)\n"
        "sawFlip = false\n"
        "launched = false\n"
        "for i=1,30 do\n"
        "  body:step(1/30)\n"
        "  local n = body:numContacts()\n"
        "  for k=1,n do\n"
        "    if body:contact(k).kind == 4 then sawFlip = true end\n"
        "  end\n"
        "  if body.vy < -30 then launched = true end\n"
        "end\n"
        "finalY = body.y\n"
    );

    const double settleY = f.num("settleY");
    ASSERT(settleY > 143.0 && settleY < 147.0, "ball rests on the flipper before the flip");
    ASSERT(f.b("sawFlip"), "contact buffer polled a FLIPPER (kind 4) contact");
    ASSERT(f.b("launched"), "flipper impulse launched the ball upward");
    ASSERT(f.num("finalY") < 146.0, "ball left the flipper after launch");
}

// ---------------------------------------------------------------------------
// Tile-derived colliders (ADR-0003 §4): C_Tilemap::buildSolidRects() over SOLID
// attrs feeds the ColliderSet via addAabb — the maze (tile) half of the two
// collider sources. Native C++ test of the bridge (the Lua addSolidRects is a
// thin wrapper over the same path).
// ---------------------------------------------------------------------------
static void test_tile_derived_colliders() {
    printf("--- BODY-NATIVE-01: tile-derived AABBs from buildSolidRects ---\n");

    Object obj;
    C_Tilemap* tm = obj.addComponent<C_Tilemap>();
    ASSERT(tm != nullptr, "addComponent<C_Tilemap>");

    static uint8_t sheetData[16 * 16] = {0};
    tm->setSheet(SpriteSheet(sheetData, 16, 16, 1, 1));  // 16×16 tiles

    TileAttr attrs[2] = {};
    attrs[1].flags |= TileAttr::FLAG_SOLID;              // tile id 1 = SOLID
    tm->setAttrs(attrs, 2);

    uint8_t map[10 * 10] = {0};
    for (int c = 0; c < 10; ++c) map[9 * 10 + c] = 1;    // bottom row solid
    tm->setTiles(map, 10, 10);

    const std::vector<Rect> rects = tm->buildSolidRects();
    ASSERT(rects.size() == 1, "bottom solid row merges into one AABB");
    if (!rects.empty()) {
        ASSERT(rects[0].x == 0 && rects[0].y == 144, "AABB origin (0, 144)");
        ASSERT(rects[0].width == 160 && rects[0].height == 16, "AABB is 160×16");
    }

    // Feed the rects into a collider set and drop a ball onto the floor.
    ColliderSet set;
    for (const auto& r : rects) {
        set.addAabb(static_cast<float>(r.x), static_cast<float>(r.y),
                    static_cast<float>(r.x + r.width),
                    static_cast<float>(r.y + r.height), 0.1f, ColliderKinds::Wall);
    }

    BodyState body;
    body.x = 40; body.y = 60; body.radius = 4; body.restitution = 0.2f;
    StepStats stats;
    for (int i = 0; i < 90; ++i) {
        std::vector<Contact> contacts;
        stepFrame(body, set, 0.0f, 320.0f, 1.0f / 30.0f, 4, true, contacts, stats);
    }
    ASSERT(std::fabs(body.y - (144.0f - 4.0f)) < 1.0f, "ball rests on the tile floor (y≈140)");
    ASSERT(body.y <= 144.0f, "ball never tunnelled through the tile floor");
}

int main() {
    test_body_and_colliders_surface();
    test_floor_rest_lua();
    test_bouncy_contact_poll();
    test_flipper_launch_lua();
    test_tile_derived_colliders();

    printf("\n%d passed, %d failed\n", passes, failures);
    return failures > 0 ? 1 : 0;
}
