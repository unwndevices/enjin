# Phase 45: Optimized 2D Physics Engine - Research

**Researched:** 2026-03-01
**Domain:** C++ header-only physics helpers + Lua C API bindings (`engine.physics.*`)
**Confidence:** HIGH

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions

**Phase boundary:** A physics helper toolkit exposed as `engine.physics.*` Lua functions. Provides stateless physics calculations (gravity, drag, springs, attraction, bounce, orbiting, raycasting) that Lua game scripts call each frame. NOT a simulation-loop physics engine — scripts control when and how helpers are applied. Builds on existing `collision.hpp` primitives and `engine.collision.*` bindings.

**Physics model:**
- Helper function toolkit, not an auto-updating physics simulation
- Lua scripts call physics functions each frame in their update() callback
- Functions return computed values (new velocity, force vectors, positions) — scripts apply them
- General purpose: helpers should work for platformers, space games, shooters, puzzles — any 2D genre

**Gravity:**
- Global default gravity via `engine.physics.setGravity(gx, gy)` / `engine.physics.getGravity()`
- Per-call gravity application: `engine.physics.applyGravity(vx, vy, dt)` uses global default
- Override per-call: `engine.physics.applyGravity(vx, vy, gx, gy, dt)` ignores global

**Bounce / reflect:**
- `engine.physics.bounce(vx, vy, nx, ny, restitution)` — reflect velocity against surface normal scaled by restitution coefficient (0 = stop dead, 1 = perfect bounce)
- Builds on existing `collision::reflect` — adds restitution scaling on top

**Drag:**
- Single drag coefficient model: `engine.physics.applyDrag(vx, vy, drag, dt)`
- Simple velocity damping — scripts decide when to apply (air, water, surfaces)
- No separate surface friction model

**Springs:**
- Damped springs: takes position, target, velocity, stiffness, damping, dt
- Returns new velocity after spring force + damping applied
- Settles down over time (no infinite oscillation)

**Raycasting:**
- For lasers and hitscan weapon mechanics
- Returns hit position, distance, and what was hit

**Pre-computed trig tables:**
- Sin/cos lookup tables for embedded performance (ESP32 where float math is expensive)
- Used internally by orbit calculations and angle-based force helpers

**Lua API style:**
- Accept both Vec2 userdata and plain (x, y) number pairs — matching engine.collision.* convention
- Auto-detect input type (check for Vec2/Point userdata first, fall back to numbers)

### Claude's Discretion

- Whether to also include a C_PhysicsBody component that auto-integrates velocity/gravity each frame, or keep everything as pure helper functions
- Raycasting targets: tilemap grid (DDA), scene object colliders, or both — decide based on what's most useful for lasers/hitscan
- Attraction/repulsion: point-to-point functions vs field-based spatial queries — decide based on 128-object limit and static allocation
- Orbit helper: return tangential velocity vs full position+velocity update — decide based on composability with other helpers
- Broadphase strategy if any spatial queries are needed
- Trig table resolution and memory footprint
- Whether to add velocity integration helper (applyVelocity) for convenience

### Deferred Ideas (OUT OF SCOPE)

None — discussion stayed within phase scope
</user_constraints>

---

## Summary

Phase 45 implements a stateless physics helper toolkit as `engine.physics.*` Lua bindings. The work pattern is identical to what was done for Phase 44 (camera) and the existing `engine.collision.*` suite: write pure C++ math functions in a header-only style, expose them via static Lua C functions registered in `bindings_engine.cpp`, and add tests following the `camera_test.cpp` / `camera_lua_test.cpp` split pattern.

The codebase already has all required building blocks: `collision::reflect` for bounce, `math::TrigLUT` for lookup-table sin/cos (currently stubbed but the infrastructure exists), `Vec2` for vector math, and the `luaL_testudata` / `luaL_checknumber` dual-input pattern used throughout `engine.collision.*`. The only new C++ file is `physics.hpp` (header-only, mirrors `collision.hpp`) and a new source file `src/scripting/bindings_physics.cpp` (mirrors `bindings_engine.cpp` structure).

The raycasting question is the most architecturally significant discretion item. For laser/hitscan use cases, the most practical target given the 128-object limit and static allocation is: (a) tilemap DDA raycast (for hitting walls), and (b) a simple segment-vs-AABB/circle scan over scene objects. Both are achievable without dynamic allocation. A C_PhysicsBody component is a reasonable add-on but increases scope — it can be deferred if needed.

**Primary recommendation:** Implement as pure header + `bindings_physics.cpp`. No new component required. Raycasting should support tilemap (DDA) and object colliders (linear scan). Add `applyVelocity` as a convenience helper — it's trivial and scripting ergonomics justify it.

---

## Standard Stack

### Core

| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| `physics.hpp` (new) | — | Header-only C++ math for all physics helpers | Follows `collision.hpp` pattern; no dep, embeddable |
| `collision.hpp` (existing) | — | `reflect()` reused by `bounce()`; `lineLine()` reused by raycast | Already in codebase, zero cost |
| `math.hpp` (existing) | — | `TrigLUT` for sin/cos table; `clamp`, `lerp` | Already in codebase |
| `Vec2` / `types.hpp` (existing) | — | 2D float vector for all physics math | Already exposed as Lua userdata |

### Supporting

| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| `bindings_physics.cpp` (new) | — | Lua binding implementations for `engine.physics.*` | Always; follows `bindings_engine.cpp` pattern |
| `C_PhysicsBody` component (discretion) | — | Optional: auto-integrates velocity/gravity each frame | Add if scripting convenience outweighs scope |

### Alternatives Considered

| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| Header-only `physics.hpp` | Box2D / Chipmunk2D | External libs violate zero-alloc, embedded constraints |
| Manual trig LUT | `std::sin/cos` every call | Acceptable on SDL3; problematic on ESP32 float pipeline |
| Linear scan for raycast objects | Spatial grid | Overkill for 128-object limit; static grid adds complexity |

**Installation:** No new dependencies. All new code is in-tree C++.

---

## Architecture Patterns

### Recommended Project Structure

```
include/enjin2/core/
├── collision.hpp          # existing — reused by physics.hpp
├── math.hpp               # existing — TrigLUT reused
└── physics.hpp            # NEW — header-only physics helpers

src/scripting/
├── bindings_engine.cpp    # existing — add engine.physics sub-table registration here
└── bindings_physics.cpp   # NEW — all engine.physics.* Lua binding implementations

tests/
├── physics_test.cpp       # NEW — C++ unit tests for physics.hpp functions
└── physics_lua_test.cpp   # NEW — Lua integration tests for engine.physics.*
```

### Pattern 1: Header-Only C++ Physics Helper

**What:** Pure stateless inline functions in `namespace enjin2::physics`, mirrors `collision.hpp` exactly.
**When to use:** All physics math lives here; no state, no allocation.

```cpp
// Source: mirrors collision.hpp pattern (include/enjin2/core/collision.hpp)
namespace enjin2 {
namespace physics {

// Global gravity state (stored in header as inline variable, C++17)
// Or stored in LuaBindings member (matches pattern for camera / time state)
static constexpr float DEFAULT_GRAVITY_X = 0.0f;
static constexpr float DEFAULT_GRAVITY_Y = 9.8f;  // pixels/s² — script-adjustable

/// Apply gravity: returns (vx + gx*dt, vy + gy*dt)
inline void applyGravity(float vx, float vy, float gx, float gy, float dt,
                         float* outVx, float* outVy) {
    if (outVx) *outVx = vx + gx * dt;
    if (outVy) *outVy = vy + gy * dt;
}

/// Bounce: reflect velocity against normal, scaled by restitution
/// Builds on collision::reflect — adds restitution factor
inline void bounce(float vx, float vy, float nx, float ny, float restitution,
                   float* outVx, float* outVy) {
    // collision::reflect gives perfect reflection; restitution scales the result
    float rx, ry;
    // v' = v - 2(v·n)n  then scale by restitution
    float dot = vx * nx + vy * ny;
    rx = vx - 2.0f * dot * nx;
    ry = vy - 2.0f * dot * ny;
    if (outVx) *outVx = rx * restitution;
    if (outVy) *outVy = ry * restitution;
}

/// Drag: exponential velocity decay  vNew = v * (1 - drag*dt)
/// Clamp to prevent overshoot: if drag*dt >= 1, velocity → 0
inline void applyDrag(float vx, float vy, float drag, float dt,
                      float* outVx, float* outVy) {
    float factor = 1.0f - drag * dt;
    if (factor < 0.0f) factor = 0.0f;
    if (outVx) *outVx = vx * factor;
    if (outVy) *outVy = vy * factor;
}

/// Damped spring: Hooke's law + velocity damping
/// Returns new velocity — position integration is caller's responsibility
inline void springForce(float pos, float target, float vel,
                        float stiffness, float damping, float dt,
                        float* outVel) {
    float force = (target - pos) * stiffness - vel * damping;
    if (outVel) *outVel = vel + force * dt;
}

/// Attraction: returns force vector from object toward attractor
/// F = strength / (dist² + epsilon) — clamped to maxForce
inline void attract(float x, float y, float ax, float ay,
                    float strength, float maxForce,
                    float* outFx, float* outFy) {
    float dx = ax - x;
    float dy = ay - y;
    float distSq = dx * dx + dy * dy + 1e-4f;  // epsilon prevents ÷0
    float force = strength / distSq;
    if (force > maxForce) force = maxForce;
    float invDist = 1.0f / std::sqrt(distSq);
    if (outFx) *outFx = dx * invDist * force;
    if (outFy) *outFy = dy * invDist * force;
}

/// Orbit: returns tangential velocity for circular orbit
/// Composable: caller applies as acceleration or direct velocity override
inline void orbitVelocity(float x, float y, float cx, float cy,
                          float speed,
                          float* outVx, float* outVy) {
    float dx = x - cx;
    float dy = y - cy;
    float len = std::sqrt(dx * dx + dy * dy);
    if (len < 1e-6f) { if (outVx) *outVx = 0; if (outVy) *outVy = 0; return; }
    // Tangent = perpendicular to radius: (-dy, dx) / len * speed
    if (outVx) *outVx = -dy / len * speed;
    if (outVy) *outVy =  dx / len * speed;
}

/// Velocity integration convenience helper
inline void applyVelocity(float x, float y, float vx, float vy, float dt,
                          float* outX, float* outY) {
    if (outX) *outX = x + vx * dt;
    if (outY) *outY = y + vy * dt;
}

} // namespace physics
} // namespace enjin2
```

### Pattern 2: Global Gravity State in LuaBindings

**What:** Store gravity (gx, gy) as members in `LuaBindings`, not as a global in physics.hpp. Matches how active camera is stored (`m_activeCamera`).
**When to use:** Always — this follows the established LuaBindings state management pattern exactly.

```cpp
// In bindings.hpp — add to LuaBindings private section:
float m_gravityX{0.0f};    ///< Global gravity X (default: 0)
float m_gravityY{980.0f};  ///< Global gravity Y (default: 980 px/s² — 9.8 × 100px/m scale)

// In bindings_physics.cpp:
int LuaBindings::lua_engine_physics_setGravity(lua_State* L) {
    LuaBindings* b = getBindings(L);
    if (!b) return 0;
    b->m_gravityX = static_cast<float>(luaL_checknumber(L, 1));
    b->m_gravityY = static_cast<float>(luaL_checknumber(L, 2));
    return 0;
}
```

### Pattern 3: Dual Vec2/Number Input (mirrors engine.collision.*)

**What:** Each Lua binding checks for Vec2 userdata first via `luaL_testudata`, falls back to `luaL_checknumber`.
**When to use:** For functions accepting position or velocity pairs.

```cpp
// Source: established pattern from bindings_engine.cpp lua_engine_collision_pointInCircle
int LuaBindings::lua_engine_physics_applyGravity(lua_State* L) {
    float vx, vy;
    auto* v = static_cast<Vec2*>(luaL_testudata(L, 1, "Vec2"));
    if (v) {
        vx = v->x; vy = v->y;
        // dt at arg 2, optional gravity override at args 3-5
    } else {
        vx = static_cast<float>(luaL_checknumber(L, 1));
        vy = static_cast<float>(luaL_checknumber(L, 2));
        // dt at arg 3, optional gravity override at args 4-5
    }
    // ... rest of implementation
}
```

### Pattern 4: Raycasting via DDA + Linear Object Scan

**What:** Two-target raycast:
1. Tilemap DDA (Digital Differential Analyzer) — walks grid cells, finds first non-zero tile
2. Linear scan of scene objects — checks ray vs each object's AABB using `collision::lineLine` or ray-AABB intersection

**When to use:** For `engine.physics.raycast(x1,y1, x2,y2)` which returns `hit, hitX, hitY, dist, what`.

```cpp
// DDA tilemap raycast — integer grid traversal, no sqrt on hot path
// Algorithm: Amanatides & Woo (1987) — standard grid traversal
// Returns: (hit, tileX, tileY, hitX, hitY, dist)
// Source: well-established algorithm; collision::lineLine already available for fallback

// Object scan: iterate ObjectCollection (max 128 objects), test ray vs C_Position + bounds
// Returns: closest hit object's ObjectProxy
```

### Pattern 5: Trig LUT Integration

**What:** `math::TrigLUT` already exists in `math.hpp` but is currently a stub (delegates to `std::sin/cos`). For orbit and angle helpers, wrap the LUT calls to get the ESP32 benefit.
**When to use:** Internally in `orbitVelocity` and any angle-based helper. Not exposed directly to Lua (Lua already has `math.sin/cos`).

The existing `TrigLUT` uses a 256-step table (uint8_t angle index). Resolution: 256 steps = ~1.4° per step. Adequate for orbiting bodies. Memory: 256 × int16_t = 512 bytes for sine (cosine derives by offset).

**Important:** The LUT implementation in `math.cpp` needs to be backed by an actual table — currently `getSineValue()` is declared but the implementation is a stub. Phase 45 should complete the LUT backing array.

```cpp
// Verify: math.cpp should provide the lookup table backing getSineValue()
// If it doesn't: add static int16_t s_sineTable[256] = { ... } in math.cpp
// Using: TrigLUT::sin(TrigLUT::angleToIndex(radians)) in orbit helper
```

### Pattern 6: Sub-table Registration in bindings_engine.cpp

**What:** Add `engine.physics` sub-table registration in `LuaBindings::registerEngineTable()`, identical to how `engine.camera`, `engine.collision`, etc. are added.

```cpp
// Source: bindings_engine.cpp LuaBindings::registerEngineTable() — established pattern
static const LuaFuncDef kPhysicsFuncs[] = {
    {"setGravity",    lua_engine_physics_setGravity},
    {"getGravity",    lua_engine_physics_getGravity},
    {"applyGravity",  lua_engine_physics_applyGravity},
    {"bounce",        lua_engine_physics_bounce},
    {"applyDrag",     lua_engine_physics_applyDrag},
    {"springForce",   lua_engine_physics_springForce},
    {"attract",       lua_engine_physics_attract},
    {"orbitVelocity", lua_engine_physics_orbitVelocity},
    {"applyVelocity", lua_engine_physics_applyVelocity},
    {"raycast",       lua_engine_physics_raycast},
};
lua_newtable(L);
luaBindFunctions(L, -1, kPhysicsFuncs, ENJIN_ARRAY_LEN(kPhysicsFuncs));
lua_setfield(L, -2, "physics");
```

### Anti-Patterns to Avoid

- **Dynamic allocation in helpers:** All arrays (object scan results, raycast candidates) must be stack-local or fixed-size. The engine is zero-alloc.
- **Storing physics state outside LuaBindings:** Gravity (gx, gy) lives in LuaBindings members, not a file-scope global or separate singleton.
- **Infinite oscillation in spring:** Must include damping term — undamped spring `force = (target - pos) * k` oscillates forever. Damping term `- vel * d` is mandatory.
- **Division by zero in attraction/orbit:** Always add epsilon to `distSq`. Objects at exact same position produce NaN without it.
- **applyGravity ignoring dt:** Velocity integrations must all scale by `dt` — framerate-independent.
- **Implementing a full simulation loop:** This is explicitly out of scope. No internal update, no body list, no constraint solver.

---

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Velocity reflection | custom dot-product reflect | `collision::reflect` (already exists) | Tested, handles edge cases |
| Damped spring settling | custom integrator | Standard Hooke's law + damping | Well-understood, provably stable with dt scaling |
| Grid traversal for raycast | naive cell-by-cell loop | DDA (Amanatides & Woo) | O(n) steps = n tiles crossed, not n² |
| Sin/cos for orbits | runtime std::sin per frame | `math::TrigLUT` (already exists) | Avoids FPU pipeline on ESP32 |
| Vec2 input detection | custom type switch | `luaL_testudata(L, idx, "Vec2")` | Established engine pattern |

**Key insight:** The entire C++ side is 6-8 small inline functions. The real work is the Lua binding layer and the raycasting implementation.

---

## Common Pitfalls

### Pitfall 1: applyGravity Overload Disambiguation

**What goes wrong:** Two signatures with different argument counts: `applyGravity(vx, vy, dt)` (3 args, uses global) and `applyGravity(vx, vy, gx, gy, dt)` (5 args, override). Lua has no overloading — must disambiguate by `lua_gettop(L)` or optional-argument pattern.

**Why it happens:** The design decision exposes both forms. Lua C functions receive all args on the stack.

**How to avoid:** Check `lua_gettop(L)`: if 3 args (or Vec2 + 1 arg), use global gravity; if 4+ args (or Vec2 + 3 args), use provided override.

**Warning signs:** Script passes 3 args but binding reads arg 4 and gets nil → `luaL_checknumber` raises error.

```cpp
int LuaBindings::lua_engine_physics_applyGravity(lua_State* L) {
    // Check for Vec2 at arg 1
    auto* v = static_cast<Vec2*>(luaL_testudata(L, 1, "Vec2"));
    float vx, vy, dt, gx, gy;
    int dtArgIdx;
    if (v) { vx = v->x; vy = v->y; dtArgIdx = 2; }
    else    { vx = static_cast<float>(luaL_checknumber(L, 1));
              vy = static_cast<float>(luaL_checknumber(L, 2)); dtArgIdx = 3; }

    // dt always follows vx,vy
    dt = static_cast<float>(luaL_checknumber(L, dtArgIdx));

    // Optional gravity override
    LuaBindings* b = getBindings(L);
    if (lua_gettop(L) > dtArgIdx) {
        gx = static_cast<float>(luaL_checknumber(L, dtArgIdx + 1));
        gy = static_cast<float>(luaL_checknumber(L, dtArgIdx + 2));
    } else {
        gx = b ? b->m_gravityX : 0.0f;
        gy = b ? b->m_gravityY : 980.0f;
    }

    float outVx, outVy;
    physics::applyGravity(vx, vy, gx, gy, dt, &outVx, &outVy);
    lua_pushnumber(L, static_cast<lua_Number>(outVx));
    lua_pushnumber(L, static_cast<lua_Number>(outVy));
    return 2;
}
```

### Pitfall 2: Spring Instability at Large dt

**What goes wrong:** With very large `dt` (e.g., first frame, debugger pause), `force * dt` overshoots target and spring diverges.

**Why it happens:** Euler integration of spring force is conditionally stable: requires `dt < 2 / (stiffness)`. Large `dt` or high stiffness breaks this.

**How to avoid:** Document that callers should clamp `dt` to a maximum (e.g., `math.min(dt, 0.05)` at script level). Alternatively, clamp internally: `dt = min(dt, 0.05f)` in the spring helper. The codebase already uses `dt` from `engine.time.delta()` which is well-controlled in the SDL host.

**Warning signs:** Spring "explodes" — position and velocity grow without bound after a pause.

### Pitfall 3: Raycast with No Active Scene

**What goes wrong:** `engine.physics.raycast()` needs access to the active scene's object list to scan for hits. When no scene is active, `m_activeScene == nullptr`.

**Why it happens:** `engine.camera.*` already handles this with silent no-op. Raycast must do the same.

**How to avoid:** Check `b->m_activeScene == nullptr` at start; if null, return `false` (no hit). Mirror the `engine.scene.find()` pattern exactly.

### Pitfall 4: Raycasting Against Tilemap Without C_Tilemap Access

**What goes wrong:** Raycasting into a tilemap requires reading tile data, but `C_Tilemap` is on an Object in the scene — the physics binding needs to find it.

**Why it happens:** Unlike camera (one active camera pointer stored in LuaBindings), there's no `m_activeTilemap` stored.

**How to avoid:** Options:
1. Add `m_activeTilemap` pointer to LuaBindings (matches camera pattern; host calls `setActiveTilemap()`).
2. Scan scene for first object with C_Tilemap on raycast call (linear scan, acceptable for 128 objects).
3. Require caller to pass a tilemap proxy — most composable but changes API.

**Recommendation:** Add `m_activeTilemap` to LuaBindings and `engine.physics.setTilemap(proxy)` or auto-detect from scene at first raycast call. This is a Claude's Discretion area.

### Pitfall 5: TrigLUT Stub Not Backed by Real Table

**What goes wrong:** `math::TrigLUT::getSineValue()` is declared in `math.hpp` but the implementation in `math.cpp` may not have the actual lookup table — currently it uses `std::sin/cos` in the method body (confirmed: lines 126-129 in math.hpp show the fallback). The private `getSineValue()` is a stub that's never used by the public API.

**Why it happens:** The LUT infrastructure was added but not completed — `getSineValue()` is private and unused; the public `sin()`/`cos()` methods call `std::sin/cos` directly.

**How to avoid:** Phase 45 should complete the LUT: add a static `constexpr int16_t s_sineTable[256]` array and implement `getSineValue()` to index it. Alternatively, accept that `TrigLUT::sin/cos` delegates to `std::sin/cos` and document this — it still provides the right interface for future optimization.

### Pitfall 6: Gravity Default Value Units

**What goes wrong:** `9.8 m/s²` is real-world gravity, but the engine works in pixel-space. Pixel scale matters: if 1 pixel = 1 cm, gravity = 980 px/s²; if 1 pixel = 1 mm, gravity = 9800 px/s². Using raw `9.8` as default feels wrong in most games.

**Why it happens:** Physics units are not defined by the engine spec.

**How to avoid:** Default gravity to a game-friendly value (e.g., `{0, 500}` px/s² or `{0, 0}` requiring explicit set). Document that scripts must call `engine.physics.setGravity(0, 500)` for a typical platformer. The Arkanoid and Tamagotchi scripts will guide the right default.

---

## Code Examples

### applyDrag — Framerate-Independent Velocity Damping

```cpp
// Source: standard physics formulation — exponential decay approximation
// Exponential exact: v * exp(-drag * dt), but linear approx (1 - drag*dt) is used
// because it's cheaper and sufficient for typical drag values (< 5/s)
inline void applyDrag(float vx, float vy, float drag, float dt,
                      float* outVx, float* outVy) {
    float factor = 1.0f - drag * dt;
    if (factor < 0.0f) factor = 0.0f;  // prevent sign flip
    if (outVx) *outVx = vx * factor;
    if (outVy) *outVy = vy * factor;
}
```

### springForce — Damped Hooke's Law

```cpp
// Source: standard damped spring formulation
// F = k * (target - pos) - d * vel
// vel' = vel + F * dt
// This is semi-implicit Euler: works well for game springs
inline void springForce(float pos, float target, float vel,
                        float stiffness, float damping, float dt,
                        float* outVel) {
    float displacement = target - pos;
    float force = displacement * stiffness - vel * damping;
    if (outVel) *outVel = vel + force * dt;
}
```

### DDA Raycast Pseudocode

```cpp
// Source: Amanatides & Woo "A Fast Voxel Traversal Algorithm" (1987)
// Standard grid traversal — O(n) where n = tiles crossed
// Returns first non-zero tile hit along ray from (x1,y1) to (x2,y2)
struct RayHit {
    bool hit;
    float hitX, hitY;
    float dist;
    int tileX, tileY;
    uint8_t tileId;
};

RayHit raycastTilemap(const C_Tilemap& tm, float ox, float oy, float dx, float dy, float maxDist) {
    // Convert origin to tile space
    // Walk with DDA — no sqrt, just integer stepping
    // Return first tile where tileId != 0
}
```

### Lua usage pattern (from CONTEXT.md game descriptions)

```lua
-- Laser hitscan
function fireLaser(sx, sy, dirX, dirY)
    local hit, hx, hy, dist, what = engine.physics.raycast(
        sx, sy, sx + dirX * MAX_RANGE, sy + dirY * MAX_RANGE
    )
    if hit then
        -- draw laser from (sx,sy) to (hx,hy)
        engine.graphics.line(sx, sy, hx, hy, COLOR.RED)
    end
end

-- Bouncing projectile
function update_projectile(self, dt)
    -- Apply gravity
    ball.vx, ball.vy = engine.physics.applyGravity(ball.vx, ball.vy, dt)
    -- Apply drag
    ball.vx, ball.vy = engine.physics.applyDrag(ball.vx, ball.vy, 0.1, dt)
    -- Move
    ball.x, ball.y = engine.physics.applyVelocity(ball.x, ball.y, ball.vx, ball.vy, dt)
    -- Bounce off floor
    if ball.y > FLOOR_Y then
        ball.y = FLOOR_Y
        ball.vx, ball.vy = engine.physics.bounce(ball.vx, ball.vy, 0, -1, 0.75)
    end
end

-- Spring-attached object
function update_spring_object(self, dt)
    spring.vx = engine.physics.springForce(spring.x, anchor.x, spring.vx, 200, 8, dt)
    spring.vy = engine.physics.springForce(spring.y, anchor.y, spring.vy, 200, 8, dt)
    spring.x = spring.x + spring.vx * dt
    spring.y = spring.y + spring.vy * dt
end

-- Orbiting satellite
function update_orbit(self, dt)
    local ovx, ovy = engine.physics.orbitVelocity(body.x, body.y, center.x, center.y, orbitSpeed)
    body.x = body.x + ovx * dt
    body.y = body.y + ovy * dt
end

-- Gravity well
function update_gravity_well(self, dt)
    local fx, fy = engine.physics.attract(ship.x, ship.y, well.x, well.y, 5000, 200)
    ship.vx = ship.vx + fx * dt
    ship.vy = ship.vy + fy * dt
end
```

---

## Discretion Recommendations

### C_PhysicsBody Component

**Recommendation: Skip for this phase.** The helper-only model is more composable. Scripts that want auto-integration can trivially write their own `applyVelocity` wrapper. Adding a component adds: new header, component factory registration, Lua proxy with proxy metatable, additional test coverage. Defer to a future phase if multiple scripts show the boilerplate pattern.

### Raycasting Targets

**Recommendation: Support both tilemap (DDA) and scene objects (linear scan).**

- Tilemap DDA: store `m_activeTilemap` in LuaBindings (parallel to `m_activeCamera`). Host calls `bindings.setActiveTilemap(tm)` each scene activation. Silent no-op if null.
- Object scan: iterate `m_activeScene->getObjects()` (linear scan, max 128), skip objects without AABB (i.e., no C_Position + size info). Return closest hit.
- Return: `hit, hitX, hitY, dist, "tilemap"|"object"` — string type discriminator avoids returning userdata.

The API: `engine.physics.raycast(x1, y1, x2, y2) → hit, hitX, hitY, dist, what`

### Attraction/Repulsion

**Recommendation: Point-to-point function (`engine.physics.attract`).** With 128-object limit, field-based spatial queries are overkill. Scripts iterate objects themselves if needed. `attract(x, y, ax, ay, strength, maxForce)` covers gravity wells and repulsion (negative strength).

### Orbit Helper

**Recommendation: Return tangential velocity only.** Composability with drag, attraction, and springs is highest when orbit returns velocity — callers apply with `applyVelocity`. Full position+velocity update reduces composability and breaks the helper-function model.

### Trig Table Resolution

**Recommendation: Keep 256-step table (existing TrigLUT resolution).** ~1.4° per step is sufficient for smooth orbiting at 60fps. Memory cost: 512 bytes (sine table only; cosine = sine with phase offset). Complete the `getSineValue()` implementation in `math.cpp` with a constexpr table — this is a natural part of the LUT "pre-calculation" the user requested.

### applyVelocity Helper

**Recommendation: Add it.** It's trivial (4 lines) and scripting ergonomics improve meaningfully — the Arkanoid-style script above shows how natural the pattern becomes. Three of the likely scripts (platformer, space shooter, pinball) all need this pattern every frame.

---

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| Script-level manual math | Helper toolkit `engine.physics.*` | Phase 45 | Reduces per-game boilerplate, improves correctness |
| std::sin/cos every call | TrigLUT (256-step, 512B) | Phase 45 (completing LUT) | ESP32 FPU pipeline avoidance |
| Raycast via pure Lua loops | C++ DDA + object scan | Phase 45 | Performance on 60fps embedded target |

---

## Open Questions

1. **What is the object bounds representation for raycasting?**
   - What we know: `C_Position` stores int16_t (x, y) position. No size component exists.
   - What's unclear: How does the raycast know the bounding box of a "hittable" object? Does it use a fixed radius? A tag? A C_Drawable AABB?
   - Recommendation: Use a fixed default radius (e.g., 8px) or require a special tag `"collidable"`. The simplest approach: cast against the object's position as a point (radius 0), or against C_Drawable's pixel AABB if available.

2. **Should `engine.physics.setTilemap(proxy)` exist, or auto-detect from scene?**
   - What we know: Camera uses `setActiveCamera()` called by host. Tilemap has no equivalent host-side pointer.
   - What's unclear: Is auto-detection (find first C_Tilemap in scene on each raycast call) acceptable overhead?
   - Recommendation: For 128-object scenes, linear scan at raycast time costs ~128 pointer checks — acceptable. Avoids a new binding function and host-side integration work.

3. **Gravity default value for typical games?**
   - What we know: Engine uses pixel coordinates. Typical game feel varies widely.
   - Recommendation: Default to `{0, 0}` (no gravity) — require explicit `setGravity()` call. This is safer and avoids confusing behavior when scripts forget to call setGravity.

---

## Sources

### Primary (HIGH confidence)

- `/home/unwn/dev/enjin/include/enjin2/core/collision.hpp` — complete reflect, lineLine implementations verified
- `/home/unwn/dev/enjin/include/enjin2/core/math.hpp` — TrigLUT, lerp, clamp verified
- `/home/unwn/dev/enjin/include/enjin2/core/types.hpp` — Vec2, Point, Rect verified
- `/home/unwn/dev/enjin/src/scripting/bindings_engine.cpp` — all binding patterns (sub-table registration, luaL_testudata dual-input, getBindings) verified
- `/home/unwn/dev/enjin/src/scripting/bindings_math.cpp` — pushVec2/checkVec2 helpers verified
- `/home/unwn/dev/enjin/tests/CMakeLists.txt` — test registration pattern verified
- `/home/unwn/dev/enjin/CMakeLists.txt` — enjin2_lua STATIC sources list verified
- `/home/unwn/dev/enjin/.planning/phases/45-optimized-2d-physics-engine/45-CONTEXT.md` — user decisions

### Secondary (MEDIUM confidence)

- DDA (Amanatides & Woo 1987) raycast algorithm — well-established, widely verified in game development literature
- Damped spring semi-implicit Euler formulation — standard game physics reference (Game Programming Gems series)
- Hooke's law + velocity damping — standard textbook formulation, high confidence

### Tertiary (LOW confidence)

- Gravity default value recommendation — based on common game feel patterns, not engine-specific. Needs validation with first target game script.

---

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH — all files read, patterns confirmed from existing codebase
- Architecture: HIGH — follows established collision/camera precedent exactly
- Physics math: HIGH — standard formulas, verified in literature
- Raycasting: MEDIUM — DDA algorithm is well-established; object bounds representation is an open question
- Pitfalls: HIGH — derived from direct code inspection of existing patterns

**Research date:** 2026-03-01
**Valid until:** 2026-04-01 (stable codebase, no external dependencies)
