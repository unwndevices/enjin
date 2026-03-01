# Phase 45: Optimized 2D Physics Engine - Context

**Gathered:** 2026-03-01
**Status:** Ready for planning

<domain>
## Phase Boundary

A physics helper toolkit exposed as `engine.physics.*` Lua functions. Provides stateless physics calculations (gravity, drag, springs, attraction, bounce, orbiting, raycasting) that Lua game scripts call each frame. NOT a simulation-loop physics engine — scripts control when and how helpers are applied. Builds on existing `collision.hpp` primitives and `engine.collision.*` bindings.

</domain>

<decisions>
## Implementation Decisions

### Physics model
- Helper function toolkit, not an auto-updating physics simulation
- Lua scripts call physics functions each frame in their update() callback
- Functions return computed values (new velocity, force vectors, positions) — scripts apply them
- General purpose: helpers should work for platformers, space games, shooters, puzzles — any 2D genre

### Gravity
- Global default gravity via `engine.physics.setGravity(gx, gy)` / `engine.physics.getGravity()`
- Per-call gravity application: `engine.physics.applyGravity(vx, vy, dt)` uses global default
- Override per-call: `engine.physics.applyGravity(vx, vy, gx, gy, dt)` ignores global

### Bounce / reflect
- `engine.physics.bounce(vx, vy, nx, ny, restitution)` — reflect velocity against surface normal scaled by restitution coefficient (0 = stop dead, 1 = perfect bounce)
- Builds on existing `collision::reflect` — adds restitution scaling on top

### Drag
- Single drag coefficient model: `engine.physics.applyDrag(vx, vy, drag, dt)`
- Simple velocity damping — scripts decide when to apply (air, water, surfaces)
- No separate surface friction model

### Springs
- Damped springs: takes position, target, velocity, stiffness, damping, dt
- Returns new velocity after spring force + damping applied
- Settles down over time (no infinite oscillation)

### Raycasting
- For lasers and hitscan weapon mechanics
- Returns hit position, distance, and what was hit

### Pre-computed trig tables
- Sin/cos lookup tables for embedded performance (ESP32 where float math is expensive)
- Used internally by orbit calculations and angle-based force helpers

### Lua API style
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

</decisions>

<specifics>
## Specific Ideas

- User wants this for practical game mechanics: lasers, hitscan weapons, orbiting bodies, bouncing projectiles, spring-attached objects, gravity wells
- Pre-calculation emphasis: trig lookup tables to avoid runtime sin/cos on embedded targets
- "We can probably pre-calculate some stuff" — lean toward table-based approaches where applicable

</specifics>

<code_context>
## Existing Code Insights

### Reusable Assets
- `collision.hpp`: AABB, circle-circle, point-in-rect, line-line, line-circle detection + aabbOverlap response, circleCircleResponse (normals, depth), reflect (velocity reflection against normal)
- `Vec2` struct (types.hpp): Float-precision 2D vector with +, -, *, /, dot, cross, length, normalize, rotate, angle, distance
- `engine.collision.*` Lua bindings: All collision functions already exposed to Lua with Vec2/Point/Rect userdata support
- `C_Position`: Integer (int16_t) position component — physics helpers will work with float values, scripts convert when setting position

### Established Patterns
- `engine.*` sub-table pattern: engine.scene.*, engine.time.*, engine.collision.* — physics would be engine.physics.*
- Lua C API binding pattern: static functions registered via LuaFuncDef array + luaBindFunctions()
- Userdata type detection: luaL_testudata() to check for Vec2/Point/Rect before falling back to luaL_checknumber()
- Header-only inline functions for performance (collision.hpp pattern)
- Static allocation: no dynamic memory, fixed-size arrays with compile-time limits

### Integration Points
- `bindings_engine.cpp`: Where engine.physics.* sub-table would be registered alongside engine.collision.*
- `LuaBindings` class: Holds static methods for all Lua-callable functions
- `bindings.hpp`: Forward declarations for new binding functions
- Scene update loop: If C_PhysicsBody component is added, it hooks into Object::update(dt) pipeline

</code_context>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope

</deferred>

---

*Phase: 45-optimized-2d-physics-engine*
*Context gathered: 2026-03-01*
