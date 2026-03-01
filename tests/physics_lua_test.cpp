// physics_lua_test.cpp — Lua integration tests for engine.physics.* bindings
// Phase 45: PHYS-09..PHYS-13
// Tests all 10 engine.physics.* functions via executeString() + Lua assertions.
#include <enjin2/scripting/bindings.hpp>
#include <enjin2/scripting/lua_engine.hpp>
#include <cstdio>
#include <cstdlib>
#include <cmath>

using namespace enjin2;

#define ASSERT(cond, msg) \
    do { if (!(cond)) { printf("FAIL: %s (line %d)\n", msg, __LINE__); exit(1); } } while(0)

#define ASSERT_NEAR(a, b, eps, msg) \
    do { if (std::fabs((a)-(b)) > (eps)) { \
        printf("FAIL: %s — got %f, expected %f (line %d)\n", msg, (double)(a), (double)(b), __LINE__); \
        exit(1); } } while(0)

// Helper: run a Lua snippet and check it succeeds
static void runLua(LuaEngine& eng, const char* code, const char* label) {
    LuaResult r = eng.executeString(code);
    if (!r.success) {
        printf("FAIL: %s — Lua error: %s\n", label, r.error.c_str());
        exit(1);
    }
}

int main() {
    printf("=== physics_lua_test ===\n");

    LuaEngine eng;
    eng.initialize();
    LuaBindings bindings(&eng);
    bindings.registerAll();
    lua_State* L = eng.getState();

    // ── engine.physics sub-table exists ──────────────────────────────────────
    {
        runLua(eng, R"(
            assert(type(engine.physics) == "table", "engine.physics must be a table")
        )", "engine.physics is table");
        printf("  PASS: engine.physics is table\n");
    }

    // ── Default gravity is (0, 0) ─────────────────────────────────────────────
    {
        runLua(eng, R"(
            local gx, gy = engine.physics.getGravity()
            assert(math.abs(gx) < 0.001, "default gravity gx should be 0, got " .. tostring(gx))
            assert(math.abs(gy) < 0.001, "default gravity gy should be 0, got " .. tostring(gy))
        )", "default gravity is (0,0)");
        printf("  PASS: default gravity is (0, 0)\n");
    }

    // ── setGravity / getGravity round-trip ───────────────────────────────────
    {
        runLua(eng, R"(
            engine.physics.setGravity(0, 500)
            local gx, gy = engine.physics.getGravity()
            assert(math.abs(gx - 0) < 0.01, "setGravity: gx should be 0, got " .. tostring(gx))
            assert(math.abs(gy - 500) < 0.01, "setGravity: gy should be 500, got " .. tostring(gy))
        )", "setGravity/getGravity round-trip");
        printf("  PASS: setGravity/getGravity round-trip\n");
    }

    // ── applyGravity — global gravity form (3 args) ──────────────────────────
    {
        // Gravity is (0, 500) from previous test
        runLua(eng, R"(
            local vx, vy = engine.physics.applyGravity(0, 0, 1.0)
            assert(math.abs(vx - 0) < 0.01,   "applyGravity(global): vx should be 0, got " .. tostring(vx))
            assert(math.abs(vy - 500) < 0.01, "applyGravity(global): vy should be 500, got " .. tostring(vy))
        )", "applyGravity global form");
        printf("  PASS: applyGravity global gravity (3-arg form)\n");
    }

    // ── applyGravity — override gravity form (5 args) ────────────────────────
    {
        runLua(eng, R"(
            -- Override ignores global gravity (0,500), uses (100, 0)
            local vx, vy = engine.physics.applyGravity(0, 0, 100, 0, 1.0)
            assert(math.abs(vx - 100) < 0.01, "applyGravity(override): vx should be 100, got " .. tostring(vx))
            assert(math.abs(vy - 0) < 0.01,   "applyGravity(override): vy should be 0, got " .. tostring(vy))
        )", "applyGravity override form");
        printf("  PASS: applyGravity override gravity (5-arg form)\n");
    }

    // ── applyGravity — Vec2 input form ───────────────────────────────────────
    {
        runLua(eng, R"(
            engine.physics.setGravity(0, 500)
            local v = Vec2(0, 0)
            local vx, vy = engine.physics.applyGravity(v, 1.0)
            assert(math.abs(vx - 0) < 0.01,   "applyGravity(Vec2): vx should be 0")
            assert(math.abs(vy - 500) < 0.01, "applyGravity(Vec2): vy should be 500, got " .. tostring(vy))
        )", "applyGravity Vec2 input");
        printf("  PASS: applyGravity Vec2 input form\n");
    }

    // ── bounce — perfect elastic bounce (restitution=1) ──────────────────────
    {
        runLua(eng, R"(
            -- velocity (10, 0) hitting a wall with normal (-1, 0), restitution=1
            local vx, vy = engine.physics.bounce(10, 0, -1, 0, 1.0)
            assert(math.abs(vx - (-10)) < 0.01, "bounce(r=1): vx should be -10, got " .. tostring(vx))
            assert(math.abs(vy - 0) < 0.01,    "bounce(r=1): vy should be 0, got " .. tostring(vy))
        )", "bounce elastic");
        printf("  PASS: bounce (perfect elastic, restitution=1)\n");
    }

    // ── bounce — partial restitution (restitution=0.8) ───────────────────────
    {
        runLua(eng, R"(
            -- velocity (10, 0) hitting normal (-1, 0), restitution=0.8 => vx = -8
            local vx, vy = engine.physics.bounce(10, 0, -1, 0, 0.8)
            assert(math.abs(vx - (-8)) < 0.01, "bounce(r=0.8): vx should be -8, got " .. tostring(vx))
            assert(math.abs(vy - 0) < 0.01,    "bounce(r=0.8): vy should be 0")
        )", "bounce partial restitution");
        printf("  PASS: bounce (partial restitution=0.8)\n");
    }

    // ── bounce — dead stop (restitution=0) ───────────────────────────────────
    {
        runLua(eng, R"(
            local vx, vy = engine.physics.bounce(10, 5, -1, 0, 0.0)
            assert(math.abs(vx) < 0.01, "bounce(r=0): vx should be 0")
            assert(math.abs(vy) < 0.01, "bounce(r=0): vy should be 0")
        )", "bounce dead stop");
        printf("  PASS: bounce (dead stop, restitution=0)\n");
    }

    // ── applyDrag — normal damping ────────────────────────────────────────────
    {
        // factor = 1 - drag*dt = 1 - 2*0.1 = 0.8
        runLua(eng, R"(
            local vx, vy = engine.physics.applyDrag(100, 50, 2, 0.1)
            assert(math.abs(vx - 80) < 0.01, "applyDrag: vx should be 80, got " .. tostring(vx))
            assert(math.abs(vy - 40) < 0.01, "applyDrag: vy should be 40, got " .. tostring(vy))
        )", "applyDrag normal");
        printf("  PASS: applyDrag (normal damping)\n");
    }

    // ── applyDrag — extreme drag clamps to 0 ─────────────────────────────────
    {
        // drag=100, dt=1 => factor=1-100=-99 => clamped to 0 => velocity = 0
        runLua(eng, R"(
            local vx, vy = engine.physics.applyDrag(100, 50, 100, 1.0)
            assert(math.abs(vx) < 0.01, "applyDrag(extreme): vx should be 0, got " .. tostring(vx))
            assert(math.abs(vy) < 0.01, "applyDrag(extreme): vy should be 0, got " .. tostring(vy))
        )", "applyDrag extreme clamp");
        printf("  PASS: applyDrag (extreme drag clamps to 0)\n");
    }

    // ── springForce — initial displacement ───────────────────────────────────
    {
        // pos=0, target=10, vel=0, k=100, d=10, dt=0.1
        // force = (10-0)*100 - 0*10 = 1000; outVel = 0 + 1000*0.1 = 100
        runLua(eng, R"(
            local vel = engine.physics.springForce(0, 10, 0, 100, 10, 0.1)
            assert(math.abs(vel - 100) < 0.1, "springForce: vel should be 100, got " .. tostring(vel))
        )", "springForce initial displacement");
        printf("  PASS: springForce (initial displacement)\n");
    }

    // ── springForce — at rest (no force) ─────────────────────────────────────
    {
        // pos=target => displacement=0, vel=0 => force=0, outVel=0
        runLua(eng, R"(
            local vel = engine.physics.springForce(10, 10, 0, 100, 10, 0.1)
            assert(math.abs(vel) < 0.01, "springForce at-rest: vel should be 0, got " .. tostring(vel))
        )", "springForce at-rest");
        printf("  PASS: springForce (at-rest state)\n");
    }

    // ── attract — basic attraction ────────────────────────────────────────────
    {
        // (0,0) toward (10,0), strength=100, maxForce=50
        // dist^2 = 100 + 1e-4 ~= 100; force = 100/100 = 1; capped at 50 => 1
        // normalized dir = (1,0); fx = 1, fy = 0
        runLua(eng, R"(
            local fx, fy = engine.physics.attract(0, 0, 10, 0, 100, 50)
            assert(fx > 0, "attract: fx should be positive, got " .. tostring(fx))
            assert(math.abs(fy) < 0.01, "attract: fy should be ~0, got " .. tostring(fy))
        )", "attract basic");
        printf("  PASS: attract (basic attraction)\n");
    }

    // ── attract — maxForce cap ────────────────────────────────────────────────
    {
        // Very strong attraction but capped at maxForce=1
        runLua(eng, R"(
            local fx, fy = engine.physics.attract(0, 0, 1, 0, 10000, 1.0)
            local mag = math.sqrt(fx*fx + fy*fy)
            assert(mag <= 1.01, "attract maxForce: magnitude should be <= 1, got " .. tostring(mag))
        )", "attract maxForce cap");
        printf("  PASS: attract (maxForce cap)\n");
    }

    // ── orbitVelocity — standard orbit ───────────────────────────────────────
    {
        // body at (10,0), center at (0,0), speed=5
        // dx = 10-0 = 10, dy = 0-0 = 0, len = 10
        // outVx = -dy/len * speed = 0, outVy = dx/len * speed = 5
        runLua(eng, R"(
            local vx, vy = engine.physics.orbitVelocity(10, 0, 0, 0, 5)
            assert(math.abs(vx - 0) < 0.01, "orbitVelocity: vx should be 0, got " .. tostring(vx))
            assert(math.abs(vy - 5) < 0.01, "orbitVelocity: vy should be 5, got " .. tostring(vy))
        )", "orbitVelocity standard");
        printf("  PASS: orbitVelocity (standard orbit)\n");
    }

    // ── orbitVelocity — degenerate (body at center) ──────────────────────────
    {
        runLua(eng, R"(
            local vx, vy = engine.physics.orbitVelocity(5, 5, 5, 5, 10)
            assert(math.abs(vx) < 0.01, "orbitVelocity(degenerate): vx should be 0")
            assert(math.abs(vy) < 0.01, "orbitVelocity(degenerate): vy should be 0")
        )", "orbitVelocity degenerate");
        printf("  PASS: orbitVelocity (degenerate — body at center)\n");
    }

    // ── applyVelocity — basic integration ────────────────────────────────────
    {
        // pos=(10,20), vel=(5,-3), dt=2.0 => pos=(10+5*2, 20+(-3)*2) = (20, 14)
        runLua(eng, R"(
            local nx, ny = engine.physics.applyVelocity(10, 20, 5, -3, 2.0)
            assert(math.abs(nx - 20) < 0.01, "applyVelocity: nx should be 20, got " .. tostring(nx))
            assert(math.abs(ny - 14) < 0.01, "applyVelocity: ny should be 14, got " .. tostring(ny))
        )", "applyVelocity basic integration");
        printf("  PASS: applyVelocity (basic integration)\n");
    }

    // ── applyVelocity — Vec2 input form ──────────────────────────────────────
    {
        runLua(eng, R"(
            local pos = Vec2(10, 20)
            local vel = Vec2(5, -3)
            local nx, ny = engine.physics.applyVelocity(pos, vel, 2.0)
            assert(math.abs(nx - 20) < 0.01, "applyVelocity(Vec2): nx should be 20, got " .. tostring(nx))
            assert(math.abs(ny - 14) < 0.01, "applyVelocity(Vec2): ny should be 14, got " .. tostring(ny))
        )", "applyVelocity Vec2 form");
        printf("  PASS: applyVelocity (Vec2 input form)\n");
    }

    // ── raycast — no scene returns false (null-guard) ─────────────────────────
    // Note: LuaBindings has no active scene set, so raycast returns false.
    {
        runLua(eng, R"(
            local hit = engine.physics.raycast(0, 0, 100, 100)
            assert(hit == false, "raycast with no scene should return false, got " .. tostring(hit))
        )", "raycast no-scene safety");
        printf("  PASS: raycast (no-scene null-guard returns false)\n");
    }

    // ── raycast — zero-length ray returns false ───────────────────────────────
    {
        runLua(eng, R"(
            local hit = engine.physics.raycast(0, 0, 0, 0)
            assert(hit == false, "raycast with zero-length ray should return false")
        )", "raycast zero-length ray");
        printf("  PASS: raycast (zero-length ray returns false)\n");
    }

    // ── Gravity reset check — applyGravity with global 0,0 returns unchanged ─
    {
        runLua(eng, R"(
            engine.physics.setGravity(0, 0)
            local vx, vy = engine.physics.applyGravity(5, 10, 1.0)
            assert(math.abs(vx - 5) < 0.01,  "applyGravity(0 gravity): vx unchanged, got " .. tostring(vx))
            assert(math.abs(vy - 10) < 0.01, "applyGravity(0 gravity): vy unchanged, got " .. tostring(vy))
        )", "applyGravity with zero global gravity");
        printf("  PASS: applyGravity (zero gravity leaves velocity unchanged)\n");
    }

    printf("=== physics_lua_test: ALL PASSED ===\n");
    return 0;
}
