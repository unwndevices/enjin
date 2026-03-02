# Phase 44: 2D Camera System - Research

**Researched:** 2026-02-28
**Domain:** 2D camera system, C++ ECS component, Lua scripting bindings, engine-wide coordinate transforms
**Confidence:** HIGH

---

## Summary

Phase 44 adds a 2D camera system to enjin2. Phase 43 (tilemap) introduced a *tilemap-scoped* scroll offset — only the tilemap viewport moves. Phase 44 introduces an *engine-wide* camera that applies a transform to ALL drawable entities in the scene simultaneously: sprites, tiles, custom drawables. This was explicitly deferred in Phase 43's CONTEXT.md as "full engine-wide camera system (moves all objects/sprites, not just tilemap viewport) — future phase."

The engine currently renders all `C_Drawable` components by calling `draw(canvas)` where each component reads its position directly from `C_Position`. There is no intermediate coordinate transform — world position equals screen position. A camera system changes this by subtracting a camera offset from each drawable's world position at render time, producing a screen position. This is the central design problem for Phase 44.

There are two viable approaches within enjin2's existing architecture:
1. **Scene-level camera state** — a `Vec2 cameraPos` on the scene (or injected into the render pipeline) that is subtracted from world positions at draw time. Simplest, zero new component overhead.
2. **C_Camera component** on an Object — follows the existing component pattern, accessible from Lua via `self:get("C_Camera")` or as a scene-level singleton. Integrates cleanly with the ComponentProxy/Lua pattern already established.

The **C_Camera component approach** is strongly recommended because: it integrates naturally with the existing `self:get()` ComponentProxy lifecycle, it can be tracked alongside C_Tilemap for consistent scroll (camera.x === tilemap.scrollX), it follows the exact same pattern as C_Timer/C_StateMachine/C_Tilemap, and it keeps camera state under Lua script control.

**Primary recommendation:** Implement `C_Camera` as a scene-level singleton component on a dedicated camera Object. Thread a `C_Camera*` pointer into the scene render pipeline so each `C_Drawable::draw()` receives a screen-space offset. Expose `engine.camera.*` as a Lua global (or `self:get("C_Camera")` proxy) for set/get/lerp operations. Apply the camera offset inside `Scene::renderObjects()` before dispatching each drawable's `draw()` call.

---

## Standard Stack

### Core (all in-tree — no new dependencies)

| Component | Source | Purpose | Why |
|---|---|---|---|
| `C_Camera` (new) | `include/enjin2/components/camera.hpp` | Stores world-space camera position, target, lerp speed | Follows C_Timer/C_StateMachine pattern exactly |
| `Component` base | `include/enjin2/core/component.hpp` | Lifecycle (awake/update/lateUpdate), ComponentProxy invalidation | Inherited by all components |
| `C_Drawable` | `include/enjin2/components/drawable.hpp` | Base for scene-rendered entities; camera offset applied before draw() | All drawable entities go through this pipeline |
| `Scene::renderObjects()` | `include/enjin2/core/scene.hpp:342` | Iterates drawables for render; camera offset is injected here | Only camera-aware code lives here; drawables stay unaware |
| `Vec2` | `include/enjin2/core/types.hpp` | Float-precision camera position (smooth scrolling) | Existing type with full math operators |
| `Point` | `include/enjin2/core/types.hpp` | Integer screen offset applied to drawable positions | Existing type; camera truncated from Vec2 at draw time |
| `ComponentProxy` | `include/enjin2/scripting/component_proxy.hpp` | Lua proxy userdata for `self:get("C_Camera")` | Same lifecycle pattern as C_Timer/C_StateMachine |
| `LuaFuncDef` + `luaBindFunctions` | `include/enjin2/scripting/bind_helpers.hpp` | Registers `engine.camera.*` sub-table | Established pattern for all engine sub-tables |

### Supporting

| Tool | Purpose | When to Use |
|---|---|---|
| `engine.camera.*` Lua sub-table | Expose camera.setPosition/getPosition/lookAt/follow/shake | Scripts that manage camera directly |
| `C_Camera_Proxy` metatable | Lua proxy returned by `self:get("C_Camera")` | Scripts that want component-lifecycle-aware camera access |
| `Vec2::lerp` (manual) | Smooth camera tracking | Camera follow with configurable interpolation speed |

### Alternatives Considered

| Instead of | Could Use | Tradeoff |
|---|---|---|
| `C_Camera` component | Scene-level `Vec2 cameraPos` field | Scene field avoids the component overhead but breaks from ECS pattern; not Lua-accessible via self:get() |
| Camera offset applied in `Scene::renderObjects()` | Camera offset applied inside each `C_Drawable::draw()` | Per-drawable application requires threading camera into draw() signature or making it a global — both worse |
| `engine.camera.*` global Lua API | Only `self:get("C_Camera")` ComponentProxy | Global API is easier for scripts without a dedicated camera object; both can coexist |
| `Vec2` float position | `Point` integer position | Float required for smooth sub-pixel tracking; truncate to int16_t at draw dispatch |

**Installation:** No new packages. All dependencies are in-tree.

---

## Architecture Patterns

### Recommended Project Structure

```
include/enjin2/components/
└── camera.hpp              # C_Camera component declaration

src/components/
└── camera.cpp              # C_Camera update() — lerp toward target, shake accumulate/decay

src/scripting/
└── bindings.cpp            # Add engine.camera.* sub-table in registerEngineTable()
                            # Add C_Camera_Proxy metatable in registerComponentProxyMetatable()

tests/
└── camera_test.cpp         # C++ unit tests: position get/set, lerp, viewport clamping
```

### Pattern 1: Camera Offset Injection at Render Time

**What:** `Scene::renderObjects()` finds the active `C_Camera` (if any) and subtracts its integer position from each drawable's world position before calling `draw()`.

**Why:** Drawables remain world-coordinate-unaware. The camera is a single point of control. This is the standard 2D game camera pattern.

**How the existing pipeline works:**

```cpp
// From include/enjin2/core/scene.hpp:342 (renderObjects)
for (size_t i = 0; i < drawableCount; ++i) {
    drawables[i]->draw(canvas);   // currently: world pos == screen pos
}
```

**With camera, it becomes:**

```cpp
// Proposed modification to Scene::renderObjects()
Point camOffset(0, 0);
if (m_camera) {  // m_camera = C_Camera* injected by scene or found via objects
    camOffset = m_camera->getScreenOffset();
}
for (size_t i = 0; i < drawableCount; ++i) {
    drawables[i]->drawWithOffset(canvas, camOffset);
}
```

**Two sub-options for offset injection into C_Drawable:**

Option A — `drawWithOffset(canvas, offset)` virtual override:
```cpp
// C_Drawable adds:
virtual void drawWithOffset(ICanvas<Pixel4>& canvas, Point offset);
// Default: temporarily modifies anchor_offset, calls draw(), restores
```
- Pro: Backward compatible — `draw()` signature unchanged
- Con: Minor overhead per drawable

Option B — `anchor_offset` is pre-applied before draw dispatch:
```cpp
// Scene::renderObjects temporarily sets an offset on each drawable
drawables[i]->AddOffset(camOffset);
drawables[i]->draw(canvas);
drawables[i]->AddOffset({-camOffset.x, -camOffset.y}); // restore
```
- Pro: Uses existing `AddOffset()` mechanism
- Con: Mutates and restores state; not re-entrant safe (acceptable — single-threaded engine)

**Recommendation:** Option A (`drawWithOffset`) is cleaner and avoids state mutation. The base `C_Drawable` implementation can call `draw()` internally with a temporary offset adjustment via `GetOffsetPosition()`.

### Pattern 2: C_Camera Component Data

```cpp
// Source: codebase analysis (no external library needed)
class C_Camera : public Component {
public:
    explicit C_Camera(Object* owner);
    void update(float dt) override;  // lerp toward target, apply shake decay

    // World-space camera position (float for smooth sub-pixel movement)
    void setPosition(float x, float y);
    Vec2 getPosition() const { return m_pos; }

    // Smooth follow: sets target; lerps m_pos toward target each update()
    void lookAt(float x, float y, float lerpSpeed = 1.0f);

    // Follow an Object's C_Position (target updated each frame in update())
    void follow(Object* target, float lerpSpeed = 0.1f);

    // Screen shake: accumulates an offset decaying over duration seconds
    void shake(float intensity, float duration);

    // Returns the integer screen offset to subtract from world positions
    // = -(m_pos + m_shakeOffset) + screenCenter
    Point getScreenOffset() const;

    // Viewport bounds clamping (optional; 0,0,0,0 = unclamped)
    void setBounds(int16_t minX, int16_t minY, int16_t maxX, int16_t maxY);

private:
    Vec2 m_pos{0.f, 0.f};          // Current camera world position
    Vec2 m_target{0.f, 0.f};       // Lerp target
    float m_lerpSpeed{1.0f};       // 0 = instant, 0.1 = smooth follow, 1.0 = instant
    Object* m_followTarget{nullptr}; // Non-owning follow target
    float m_followLerp{0.1f};

    // Screen shake state
    float m_shakeIntensity{0.f};
    float m_shakeDuration{0.f};
    float m_shakeElapsed{0.f};
    Vec2  m_shakeOffset{0.f, 0.f};

    // Viewport clamping
    bool  m_hasBounds{false};
    int16_t m_minX{0}, m_minY{0}, m_maxX{0}, m_maxY{0};
};
```

### Pattern 3: Lua API Surface

Two complementary access paths:

```lua
-- Path 1: engine.camera.* global (easiest for simple scenes)
engine.camera.setPosition(64, 32)          -- move camera to world (64, 32)
engine.camera.lookAt(64, 32)               -- instant; no lerp
engine.camera.lookAt(64, 32, 0.1)         -- smooth: lerp speed 0.1
engine.camera.follow(obj, 0.1)             -- follow object with lerp
engine.camera.shake(3.0, 0.4)             -- intensity=3px, duration=0.4s
local x, y = engine.camera.getPosition()
engine.camera.setBounds(0, 0, 512, 256)   -- clamp to world bounds

-- Path 2: self:get("C_Camera") ComponentProxy
local cam = self:get("C_Camera")
cam:setPosition(64, 32)
cam:follow(player, 0.08)
```

### Pattern 4: Integration with C_Tilemap Scroll

The CONTEXT.md for Phase 43 distinguished tilemap-scoped scroll (built into C_Tilemap) from engine-wide camera (this phase). In Phase 44, these should be unified:

```cpp
// C_Tilemap::draw() with camera awareness
void C_Tilemap::draw(ICanvas<Pixel4>& canvas) override;
void C_Tilemap::drawWithOffset(ICanvas<Pixel4>& canvas, Point camOffset) override {
    // scrollX/Y from C_Tilemap's own offset PLUS camera offset
    int16_t effectiveScrollX = m_scrollX + camOffset.x;  // or subtract, depending on convention
    int16_t effectiveScrollY = m_scrollY + camOffset.y;
    // render tiles using effectiveScrollX/Y
}
```

This means C_Tilemap's `setScroll()` remains for parallax/independent tilemap scroll, and the camera offset is additive on top of it.

### Anti-Patterns to Avoid

- **Making camera a global singleton pointer**: Breaks scene encapsulation. Two scenes cannot have independent cameras. Use scene-level camera pointer injection instead.
- **Applying camera offset inside each C_Drawable::draw()**: Requires each drawable to know about the camera — spreads camera logic everywhere. Apply once in `Scene::renderObjects()`.
- **Integer-precision camera position**: Sub-pixel movement causes jitter in smooth follow. Store as `Vec2` (float), truncate to `Point` (int16_t) only at draw dispatch.
- **Modifying C_Position during camera update**: Camera should not mutate the C_Position of objects it follows. Camera reads position; it never writes to it.
- **Drawing UI/HUD elements with camera offset**: UI elements (labels, health bars) should be in screen-space. Implement a `setScreenSpace(bool)` flag on C_Drawable — screen-space drawables skip the camera offset.

---

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---|---|---|---|
| Float camera position | int16_t position | Vec2 (already in types.hpp) | Sub-pixel tracking requires float; int causes jitter on smooth follow |
| Lerp math | Custom interpolation | `Vec2::operator*` + scalar | Already have Vec2 with all math ops; lerp = pos + (target - pos) * speed * dt |
| Screen shake | Random accumulator | Simple sin/noise using engine.random.* or fixed offset pattern | Full noise library is overkill; signed alternating offset decays fine |
| Camera bounds | Custom clamp | `std::max`/`std::min` on Vec2 components | Trivial; no library needed |
| Object follow | Polling C_Position each frame | Direct C_Position* cached in C_Camera | `getComponent<C_Position>()` is O(n) — cache the pointer in follow() call |

**Key insight:** The camera system needs no new math libraries. Vec2, Point, std::max/min, and the existing random PRNG for shake are sufficient.

---

## Common Pitfalls

### Pitfall 1: Camera Convention Mismatch (Position vs Offset)

**What goes wrong:** Camera "position" means different things in different conventions.
- Convention A: Camera position = the world-space point AT THE CENTER of the screen. Screen pos = world pos - camera pos + screenCenter.
- Convention B: Camera position = a pixel offset SUBTRACTED from world coordinates. Screen pos = world pos - cameraPos.

Mixing conventions in Lua API vs C++ internals produces inverted or doubled movement.

**Why it happens:** "Follow player at (100, 50)" — does setting camera.position to (100, 50) put the player at the center of the screen or at (0,0)?

**How to avoid:** Document and test the convention explicitly. Recommend Convention A (camera position = world point visible at screen center). This matches how Lua game devs think: "camera is at the player."

**Warning signs:** Camera appears to move in the wrong direction, or twice as fast as expected.

### Pitfall 2: UI Elements Moving with Camera

**What goes wrong:** Score display, health bar, dialogue box — all move off-screen when camera scrolls.

**Why it happens:** All C_Drawable components receive the camera offset by default.

**How to avoid:** Add `bool m_screenSpace{false}` flag to C_Drawable. When true, `drawWithOffset()` ignores the camera offset and passes `Point(0,0)`. Expose `setScreenSpace(bool)` from Lua.

**Warning signs:** UI elements appear to "swim" as the player moves.

### Pitfall 3: C_Camera Follows Destroyed Object

**What goes wrong:** `cam:follow(obj)` stores a raw Object* pointer. If `obj` is destroyed (via `engine.scene.destroy()`), the C_Camera dereferences a dangling pointer in `update()`.

**Why it happens:** C_Camera::update() calls `m_followTarget->getPosition()` each frame with no validity check.

**How to avoid:** Two options:
1. Store an `ObjectProxy*` (like existing proxy pattern) and check `valid` flag before dereference.
2. Store Object* and an `Object::generation` counter (requires Object to expose a generation).

**Simplest safe approach:** Store `Object*` and add a guard in C_Camera::follow() call — Lua script is responsible for calling `cam:clearFollow()` before destroying the target. Document this requirement clearly. (Same trade-off as the single-proxy-per-component limitation already accepted in STATE.md.)

**Warning signs:** Crash in C_Camera::update() when a followed object is destroyed.

### Pitfall 4: Camera Jitter from Integer Truncation

**What goes wrong:** Smooth lerp produces float positions like (64.3, 31.7). Truncating to int16_t each frame causes 1-pixel jitter when the fractional part crosses 0.5.

**Why it happens:** `Point` uses `int16_t` — the draw offset must be integer pixels. Naive truncation loses sub-pixel state.

**How to avoid:** Store `Vec2 m_pos` (float) as the camera's authoritative state. Apply truncation only at dispatch: `Point offset = Point(static_cast<int16_t>(m_pos.x), static_cast<int16_t>(m_pos.y))`. The float accumulates correctly across frames; the integer is only used for screen-pixel snapping.

**Warning signs:** Smooth `cam:follow()` appears to stutter at 1-pixel granularity.

### Pitfall 5: C_Tilemap Scroll + Camera Double-Offset

**What goes wrong:** C_Tilemap has its own `scrollX/Y` offset (from Phase 43). Camera also applies an offset. If both are applied independently, the tilemap scrolls twice as fast as sprites.

**Why it happens:** C_Tilemap::draw() uses its internal scrollX/Y without knowing about the camera. The camera offset is then also applied via `drawWithOffset()`. Net effect: tiles move at 2× camera speed.

**How to avoid:** Two design options:
1. `C_Tilemap::drawWithOffset()` adds camera offset to its internal scroll: `effectiveScrollX = m_scrollX - camOffset.x`. The tilemap scroll is then RELATIVE to the camera; to keep tiles stationary relative to the world, Lua script sets `tilemap:setScroll(0, 0)` and lets the camera move everything.
2. C_Tilemap gets a `setFollowCamera(bool)` flag — when true, it uses the camera offset as its scroll origin.

**Recommended:** Option 1 — consistent with how the convention should work. `C_Tilemap::setScroll()` becomes a parallax offset on top of the camera.

**Warning signs:** Tilemap background scrolls faster or slower than sprites during camera movement.

---

## Code Examples

### Scene-Level Camera Pointer Storage

The `Scene` class needs a way to hold the active camera. Since Scene doesn't allocate dynamically, a raw pointer (non-owning) cached after `findObjectWithComponent<C_Camera>()` is simplest:

```cpp
// Proposed addition to scene.hpp or inside Scene::renderObjects()
// Source: codebase analysis (scene.hpp renderObjects() at line 342)

C_Camera* activeCamera = nullptr;
objects.forEach([&](Object* obj) {
    if (!activeCamera) {
        activeCamera = obj->getComponent<C_Camera>();
    }
});

Point camOffset(0, 0);
if (activeCamera) {
    camOffset = activeCamera->getScreenOffset();
}

for (size_t i = 0; i < drawableCount; ++i) {
    drawables[i]->drawWithOffset(canvas, camOffset);
}
```

Note: `findObjectWithComponent<C_Camera>()` is called inside the render loop each frame. Performance concern: ObjectCollection iteration is O(n) over all objects. For small scenes (< 128 objects), this is negligible. If needed, Scene can cache `m_activeCamera` and update it when objects are added/removed.

### C_Drawable::drawWithOffset() Default Implementation

```cpp
// Proposed addition to C_Drawable base class
// Source: codebase analysis (drawable.hpp:74 — draw() pure virtual)

// Default: apply offset via anchor_offset temporarily and call draw()
virtual void drawWithOffset(ICanvas<Pixel4>& canvas, Point offset) {
    if (m_screenSpace) {
        draw(canvas);  // screen-space: skip camera offset
        return;
    }
    Point saved = anchor_offset;
    anchor_offset -= offset;  // subtract camera pos from drawable's offset
    draw(canvas);
    anchor_offset = saved;
}
```

### C_Camera getScreenOffset() Calculation

```cpp
// Convention A: camera position = world point at screen center
// getScreenOffset() = -(cameraWorldPos - screenCenter)
// Net: drawableScreenPos = drawableWorldPos - cameraWorldPos + screenCenter

Point C_Camera::getScreenOffset() const {
    // Screen center (half the canvas dimensions)
    // Canvas dimensions are not available here directly — inject at construction or use constants
    int16_t halfW = 64;  // CANVAS_W / 2 — injected from host or from getWidth()
    int16_t halfH = 64;  // CANVAS_H / 2

    float screenX = halfW - (m_pos.x + m_shakeOffset.x);
    float screenY = halfH - (m_pos.y + m_shakeOffset.y);
    return Point(static_cast<int16_t>(screenX), static_cast<int16_t>(screenY));
}
```

**Note on canvas dimensions:** C_Camera needs to know the canvas dimensions to compute screen center. Options: pass them in the constructor, use `CANVAS_W`/`CANVAS_H` constants (128 in sdl_main.cpp), or inject via a setter. Prefer constructor injection to avoid hidden global dependency.

### Lua engine.camera.* Sub-Table Registration

Following the exact pattern from `bindings_engine.cpp:registerEngineTable()`:

```cpp
// In registerEngineTable() after the engine.event block
static const LuaFuncDef kCameraFuncs[] = {
    {"setPosition",  lua_engine_camera_setPosition},
    {"getPosition",  lua_engine_camera_getPosition},
    {"lookAt",       lua_engine_camera_lookAt},
    {"follow",       lua_engine_camera_follow},
    {"clearFollow",  lua_engine_camera_clearFollow},
    {"shake",        lua_engine_camera_shake},
    {"setBounds",    lua_engine_camera_setBounds},
    {"clearBounds",  lua_engine_camera_clearBounds},
};
lua_newtable(L);
luaBindFunctions(L, -1, kCameraFuncs, ENJIN_ARRAY_LEN(kCameraFuncs));
lua_setfield(L, -2, "camera");
```

Camera pointer is stored in Lua registry (pointer-to-pointer pattern, same as `enjin_ssm`, `enjin_active_scene`):

```cpp
// In setActiveScene() or a new setActiveCamera() call:
C_Camera** camPP = static_cast<C_Camera**>(
    lua_newuserdata(L, sizeof(C_Camera*)));
*camPP = nullptr;  // updated each frame or when camera Object is found
lua_setfield(L, LUA_REGISTRYINDEX, "enjin_active_camera");
```

### C_Camera_Proxy Metatable Pattern

Following `bindings.cpp:1011` (`registerComponentProxyMetatable()`), add after C_StateMachine_Proxy:

```cpp
// In registerComponentProxyMetatable() — following C_StateMachine_Proxy block
static constexpr const char* CCAMERA_PROXY_METATABLE = "C_Camera_Proxy";

luaL_newmetatable(L, CCAMERA_PROXY_METATABLE);

// __index dispatch table
lua_newtable(L);
// Register: setPosition, getPosition, lookAt, follow, clearFollow, shake, setBounds
// ... (same pattern as C_Timer_Proxy methods at line 354)
lua_setfield(L, -2, "__index");

lua_pop(L, 1);  // pop metatable
```

---

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|---|---|---|---|
| No camera (world pos = screen pos) | Engine-wide camera transform in renderObjects() | Phase 44 | All drawables participate in world-space rendering; UI needs screen-space flag |
| Tilemap-scoped scroll only | Tilemap scroll = parallax offset on top of camera | Phase 44 | Consistent scrolling: camera moves everything, tilemap.setScroll() = parallax |
| No follow | Camera follow via C_Camera::update() lerp | Phase 44 | Smooth player-following camera like classic platformers |

**Deprecated after Phase 44:**
- Using `C_Drawable::AddOffset()` manually in Lua scripts to simulate camera movement — replace with `engine.camera.setPosition()`.
- Using `tilemap:setScroll()` as a camera — replace with `engine.camera.follow()` + tilemap scroll for parallax only.

---

## Open Questions

1. **Canvas dimensions in C_Camera::getScreenOffset()**
   - What we know: `CANVAS_W = 128, CANVAS_H = 128` in `sdl_main.cpp`. The canvas dimensions are fixed at compile time for a given platform.
   - What's unclear: Should C_Camera hardcode 128×128, or should the canvas size be injected? Multiple canvas sizes may exist (WASM might differ from ESP32 target resolution).
   - Recommendation: Inject canvas width/height into C_Camera at construction time or via a setter. The Scene (which holds the compositor) knows its dimensions. Alternatively, add a `uint16_t C_Camera::getCanvasWidth()` that reads from the active canvas pointer stored in LuaBindings.

2. **Where to store the active C_Camera pointer for Lua registry access**
   - What we know: `enjin_ssm` and `enjin_active_scene` use pointer-to-pointer pattern stored in LuaBindings and written to Lua registry during `setActiveScene()`.
   - What's unclear: C_Camera lives on an Object in the scene. It's found via `findObjectWithComponent<C_Camera>()`. Should LuaBindings cache it, or should `engine.camera.*` functions search each call?
   - Recommendation: Cache in LuaBindings as `C_Camera* m_activeCamera{nullptr}`. Update in `setActiveScene()` via a scene search. Accept that dynamically spawning a camera Object after scene activation won't automatically register — Lua must call `engine.camera.activate(obj)` or set the camera via `engine.camera.setPosition()` which triggers a search.

3. **Screen-space C_Drawable flag placement**
   - What we know: C_Drawable has `buffer_index`, `blend_mode`, `anchor`, `is_visible`, `width`, `height`.
   - What's unclear: Adding `m_screenSpace` flag adds 1 byte to every C_Drawable instance. On ESP32 with 128 drawables maximum, this costs 128 bytes — acceptable.
   - Recommendation: Add `bool m_screenSpace{false}` to C_Drawable. Expose `setScreenSpace(bool)` and wire to Lua via ScriptProxy `__newindex` (same as `visible` and `layer` properties).

4. **Camera shake implementation approach**
   - What we know: Engine has seeded xorshift32 RNG in LuaBindings. C_Camera doesn't have access to LuaBindings directly.
   - What's unclear: Should shake use random offsets (requires PRNG in C_Camera), or deterministic alternating offsets (simpler, no PRNG needed)?
   - Recommendation: Alternating signed offset pattern (no PRNG needed): `shakeOffset = sin(elapsed * 40) * intensity * (1 - elapsed/duration)`. `std::sin` is available from `<cmath>`. No new dependencies.

---

## Sources

### Primary (HIGH confidence)

- Codebase analysis: `include/enjin2/core/scene.hpp` (renderObjects() loop at line 342-382)
- Codebase analysis: `include/enjin2/components/drawable.hpp` (C_Drawable base class, AddOffset, GetOffsetPosition)
- Codebase analysis: `include/enjin2/components/position.hpp` (C_Position, Point-based world coordinates)
- Codebase analysis: `include/enjin2/components/timer.hpp` and `state_machine.hpp` (ComponentProxy pattern reference)
- Codebase analysis: `src/scripting/bindings_engine.cpp` (engine.* sub-table registration pattern)
- Codebase analysis: `src/scripting/bindings.cpp:1011` (registerComponentProxyMetatable pattern)
- Codebase analysis: `.planning/phases/43-tilemap-system/43-CONTEXT.md` (explicit deferral of engine-wide camera to Phase 44)
- Codebase analysis: `include/enjin2/core/types.hpp` (Vec2, Point, Rect structures and operators)
- Codebase analysis: `src/platform/sdl/sdl_main.cpp` (CANVAS_W=128, CANVAS_H=128 constants)

### Secondary (MEDIUM confidence)

- Phase 43 RESEARCH.md: Documents tilemap-scoped scroll design and its distinction from engine-wide camera
- STATE.md Decisions: Pointer-to-pointer registry pattern, single-proxy-per-component constraint, EventBus injection model

### Tertiary (LOW confidence)

- General 2D camera system knowledge (verified consistent with codebase constraints): screen-space convention, camera follow lerp, shake via sin oscillation

---

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH — all components are in-tree; no external libraries
- Architecture: HIGH — renderObjects() modification and C_Drawable::drawWithOffset() pattern are directly verifiable from existing code
- Pitfalls: HIGH — tile/camera double-offset and UI screen-space issues are deterministic consequences of the design; verified from codebase
- Lua API: HIGH — follows established LuaFuncDef/luaBindFunctions pattern exactly; engine.camera.* mirrors engine.event.* registration

**Research date:** 2026-02-28
**Valid until:** Stable (engine architecture is stable; no fast-moving dependencies)
