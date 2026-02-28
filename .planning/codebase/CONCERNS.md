# Codebase Concerns

**Analysis Date:** 2026-02-28

## Missing Game Engine Features (Priority Roadmap)

### Critical Priority #1: Tilemap System

**Status:** Not implemented — requires core addition

**Impact:** CRITICAL — Prevents level-based games and large-scale 2D worlds. Any game requiring a scrolling level, grid-based gameplay, or background maps cannot be built without this.

**Files affected:** None yet — new subsystem needed
- `include/enjin2/graphics/tilemap.hpp` (new)
- `include/enjin2/components/tilemap.hpp` (new)
- `src/graphics/tilemap.cpp` (new)

**What's missing:**
- No tilemap data structure for grid-based map rendering
- No tile-based collision support
- No Lua API for tilemap creation/manipulation
- No example showing how to render a tiled background or scrolling level

**Recommended approach:**
1. **Core data structure:** Fixed-capacity `Tilemap<W, H, TILE_SIZE>` holding:
   - `uint8_t tiles[W*H]` — tile indices (fixed 256-tile limit)
   - `uint8_t tileset_gfx[TILESET_SIZE]` — pixel data for 16×16 tiles (fits ~256 tiles at 4-bit)
2. **C_Tilemap component:** Draws tiles to assigned layer, supports per-tile collision rectangles
3. **Lua API:** `tilemap.new(width, height, tileSize)`, `tilemap:set(x, y, tileId)`, `tilemap:draw()`
4. **Collision:** `tilemap:getTileAt(x, y)` returns tile ID; scripts call `engine.collision.tileCollide()` if needed

**Why not already done:**
- v1.5 focused on Lua scripting foundation (engine.*, self proxy, input callbacks)
- Tilemap is a major new subsystem — candidates for v1.6
- Needs careful design for zero-allocation grid: fixed-size map, fixed-size tileset

**Risk:** Game developers trying to build level-based games are blocked without this.

---

### High Priority #2: Camera System

**Status:** Not implemented — requires new subsystem

**Impact:** HIGH — Prevents viewport control, parallax scrolling, and any game where the visible area doesn't match the world. Developers must implement their own offset tracking.

**Files affected:** None yet — new subsystem needed
- `include/enjin2/graphics/camera.hpp` (new)
- `src/graphics/camera.cpp` (new)

**What's missing:**
- No camera-world transform (viewport/world conversion)
- No viewport clipping for drawable objects
- No Lua API for camera control
- No parallax scrolling foundation

**Recommended approach:**
1. **Simple camera struct:**
   ```cpp
   struct Camera {
       float x, y;              // World position (center)
       float zoom{1.0f};        // Zoom multiplier
       bool follow(Object* obj); // Auto-follow target
   };
   ```
2. **World-to-screen transform:** Convert world coords to viewport before drawing
3. **Lua API:** `engine.camera.setPosition(x, y)`, `engine.camera.getPosition()`, `engine.camera.setZoom(z)`
4. **Layer-based rendering:** Each drawable receives transform before draw call

**Why not already done:**
- Current architecture assumes 1:1 world-to-canvas mapping
- Would require updating all drawable components to apply transform
- Parallax requires per-layer camera offset (separate feature)

**Risk:** Multi-screen levels, zoom gameplay, and smooth camera following cannot be implemented.

---

### High Priority #3: Physics System

**Status:** Partial — collision detection exists, but NO physics engine

**Impact:** HIGH — Collision detection (AABB, circle, line) is available via `engine.collision.*`, but gravity, velocity, impulse, and constraint solving do NOT exist. Scripts must implement physics manually.

**Files affected:**
- `include/enjin2/core/collision.hpp` — Provides only geometric test functions
- `src/core/collision.cpp` — No physics solvers

**What exists:**
- `engine.collision.aabb()`, `circleCircle()`, `pointInRect()`, `pointInCircle()`, `lineLine()`, `lineCircle()`, `aabbOverlap()`, `circleResponse()`, `reflect()`
- These are **read-only geometric queries**, not physics

**What's missing:**
- No velocity component (no C_RigidBody)
- No gravity simulation
- No impulse-based collision response
- No constraint solvers (joints, limits)
- No physics update loop integration

**Recommended approach:**
1. **Minimal C_RigidBody component:**
   - `vec2 velocity`
   - `float mass`, `float drag`
   - `update(self, dt)` applies gravity and integrates position
2. **Collision response:** `engine.physics.resolve(obj1, obj2)` applies impulses
3. **Lua physics callback:** `on_collision(self, other)` fires when objects collide
4. **No solver complexity:** Skip constraints/joints for v1.6; enable v1.7

**Why not already done:**
- Collision detection is cheaper than full physics — many embedded games don't need it
- Physics integration loops add per-frame cost on ESP32
- Requires careful integration with scene update order (physics before collision queries)

**Risk:** Platformer games, top-down shooters with knockback, and gravity-based puzzlers cannot be built without manual physics code in Lua.

---

### High Priority #4: Audio System

**Status:** Not implemented — explicitly deferred to Tomodachi platform

**Impact:** HIGH — No sound, music, or audio synthesis available. Tomodachi (the hardware consuming enjin2) handles audio separately via MIDI; games cannot trigger audio events.

**Files affected:** None — out of scope
- Audio handled by Tomodachi MIDI system, not enjin2

**What's missing:**
- No audio mixer
- No WAV/MP3 playback
- No sound effect triggering from Lua
- No music sequencing

**Current status:** PICO-8 and Playdate have built-in audio APIs. enjin2 targets embedded displays (ESP32 with small screens); audio is application-specific. Tomodachi uses external MIDI/synth.

**Recommended approach:**
- Define a minimal audio interface (`engine.audio.play(sampleId)`, `engine.audio.stopAll()`)
- Leave implementation to platform: Tomodachi hooks MIDI, desktop SDL uses a simple mixer
- **Priority: v1.6 or v1.7** — not blocking games without audio

**Risk:** Games requiring audio feedback are unsatisfying without sound.

---

### Medium Priority #5: Particle Effects System

**Status:** Partially implemented — visual effects exist, but NOT scriptable particles

**Impact:** MEDIUM — PostFx effects (CRT scanlines, noise, blur, glow) are available in C++ and work well, but Lua has NO API to create/destroy particles or emission patterns. Developers cannot script visual effects.

**Files affected:**
- `include/enjin2/effects/postfx.hpp` — Visual effects only (not particles)
- `src/effects/postfx.cpp` — No particle system

**What exists:**
- CRT scanlines, moving scanlines, barrel distortion, noise, blur, glow, dither, contrast, brightness
- Applied at canvas level in C++
- No Lua bindings

**What's missing:**
- No particle pool or emitter system
- No scriptable emission patterns
- No Lua API for particle creation
- No example of particles in game logic

**Recommended approach:**
1. **Simple C_ParticleEmitter component:**
   - Fixed-size particle pool (e.g., 64 particles per emitter)
   - Position, velocity, lifetime, color per particle
   - `emit(count, spread_angle, speed)` method
2. **Lua API:**
   ```lua
   local emitter = engine.scene.spawn("particle_burst")
   emitter:emit(10, math.pi/4, 5.0)  -- 10 particles, π/4 spread, 5 units/sec
   ```
3. **Auto-cleanup:** Particles fade at lifetime expiry; emitter auto-destroys when empty

**Why not already done:**
- PostFx effects satisfy visual variety for many games
- Particle allocation is complex; particle pools require careful design for zero-alloc constraint
- Lua emission patterns are a nice-to-have, not critical

**Risk:** Games with explosions, magic effects, or weather cannot express visual polish without particle effects.

---

### Medium Priority #6: UI System Enhancements

**Status:** Partial — basic widgets exist, but missing modern UI patterns

**Impact:** MEDIUM — UI is built on individual components (C_Label, C_Button, C_Slider, C_Gauge). No layout system, no modal dialogs, no screen state management.

**Files affected:**
- `include/enjin2/ui/component.hpp` — Base widget
- `include/enjin2/ui/components.hpp` — Label, Button, Slider, Gauge, etc.
- `include/enjin2/components/label.hpp` — Text rendering widget

**What exists:**
- C_Label (text widget with optional background)
- C_Button (rect with click detection, no visual feedback)
- C_Slider (linear slider with min/max)
- C_Gauge (circular dial)
- C_Tickmarks (scale marks)
- C_FillUpGauge (progress bar)

**What's missing:**
- No layout engine (no flex/grid; manual positioning required)
- No theme/style system (colors hardcoded per component)
- No modal dialog system
- No input focus management
- No UI state machine (active screen vs. paused screen)
- No Lua bindings for UI creation/destruction

**Recommended approach:**
1. **Layout helpers:** `ui.hbox()`, `ui.vbox()` for simple alignment
2. **Theme system:** Define color palettes once, apply to all widgets
3. **Focus tracking:** Track which widget receives button input
4. **Lua UI builder:**
   ```lua
   local button = engine.ui.createButton("Start", x, y, w, h)
   button:onPressed(function(self) engine.scene.switch(1) end)
   ```
5. **Screen state machine:** `engine.scene.isPaused()`, `engine.scene.showMenu(screenId)`

**Why not already done:**
- Embedded displays are small; simple manual positioning is often sufficient
- UI framework design is complex; defer until gameplay features mature
- PICO-8 and Playdate both have minimal built-in UI (text buttons only)

**Risk:** Games with complex menus, inventories, or HUD overlays require custom C++ UI code.

---

### Medium Priority #7: Input System Completeness

**Status:** Mostly complete — polling and edge callbacks exist, but missing advanced features

**Impact:** MEDIUM — Button polling (`isButtonHeld`, `justPressed`) and callbacks (`on_button_pressed`, `on_button_released`) work. Missing: touch input, analog stick deadzones, gamepad rumble, mouse support.

**Files affected:**
- `include/enjin2/input/input_state.hpp` — Button bitmask and axes array
- `src/input/` — Platform-specific implementations (SDL, ESP32, WASM)

**What exists:**
- 16 buttons (bitmask) with edge detection
- 8 analog axes (normalized -1.0 to 1.0)
- SDL3: keyboard mapping (arrows, Z/X, Enter)
- Lua polling API: `engine.input.held()`, `engine.input.justPressed()`, callbacks

**What's missing:**
- No gamepad API (Xbox/PlayStation controller detection)
- No touch input (for devices with touchscreen)
- No analog stick deadzone filtering
- No mouse support (desktop only)
- No rumble/haptic feedback
- No input remapping (hardcoded per-platform)

**Recommended approach:**
1. **Deadzone filtering:** `InputState::applyDeadzone(axis, deadzone)` per-axis
2. **Gamepad detection:** `InputState::getGamepadCount()`, `InputState::isGamepadConnected(index)`
3. **Touch input:** Separate `TouchState` struct added to `InputState` (list of active touches)
4. **Mouse support:** X/Y position + button bitmask in `InputState`
5. **Input mapping:** Runtime remappable button→action binding system (Lua-configurable)

**Why not already done:**
- Embedded platforms (ESP32) rarely have touch or gamepads
- SDL3 keyboard mapping is sufficient for desktop development
- Touch is WASM-specific and not critical for v1.5

**Risk:** Games targeting mobile/web cannot be built without custom input code. Gamepad support missing for console-like experiences.

---

### Medium Priority #8: Networking System

**Status:** Not implemented — no network communication

**Impact:** MEDIUM — Single-player games work fine. Multiplayer, leaderboards, or cloud save are impossible. Not a priority for Tomodachi (single-device app).

**Files affected:** None — future subsystem

**What's missing:**
- No HTTP client
- No WebSocket support
- No serialization framework
- No P2P communication

**Recommended approach:**
- ESP32: Enable HTTP via LwIP (add `engine.http.post()`, `engine.http.get()`)
- WASM: Use `fetch()` API directly from Lua via native bindings
- **Out of scope for v1.6**: Focus on single-player game features first
- **Candidate for v1.7+**: After physics and camera systems mature

**Risk:** Only single-player games can be built. Leaderboards, cloud saves, and multiplayer require external platforms (Firebase, etc.).

---

### Medium Priority #9: Animation System Expansion

**Status:** Partial — sprite sheet animation exists, but missing tween/state machine systems

**Impact:** MEDIUM — C_Animation component handles sprite sheet timing (FPS, loop modes). Missing: easing functions, property tweens, state machines for complex sequences.

**Files affected:**
- `include/enjin2/animation/animation_track.hpp` — Keyframe-based animation
- `include/enjin2/animation/keyframe.hpp` — Keyframe struct
- `include/enjin2/components/animation.hpp` — Animation component

**What exists:**
- SpriteSheet with frame animation (Loop, Once, PingPong modes)
- 16-slot Lua sprite pool with FPS/frame control
- Keyframe-based animation for custom properties

**What's missing:**
- No easing functions (linear, ease-in, ease-out, etc.)
- No property tweens (animate position, scale, rotation over time)
- No animation state machine (blend animations, detect completion)
- No Lua API for tweening
- No example of complex animation sequences

**Recommended approach:**
1. **Easing library:**
   ```cpp
   float easeInQuad(float t);
   float easeOutQuad(float t);
   float easeInOutCubic(float t);
   // ... standard easing functions
   ```
2. **C_Tween component:**
   - Target property (position, rotation, color, etc.)
   - Start/end values, duration, easing function
   - `onComplete()` callback
3. **Lua tween API:**
   ```lua
   engine.tween.to(obj, {x=100, y=50}, 1.0, "easeInQuad")
   ```

**Why not already done:**
- Sprite animation covers most cases
- Tweens are nice-to-have, not critical for gameplay
- Complex animation sequences can be written in Lua manually (more verbose but flexible)

**Risk:** Smooth UI transitions, camera pans, and sequence animations require manual frame-by-frame code in Lua.

---

### Low Priority #10: Debugging & Profiling Tools

**Status:** Limited — basic logging exists, but no profiler or visual debug tools

**Impact:** LOW — `engine.log()` works. Missing: performance profiler, memory tracker, visual collision debugger, frame timing breakdown.

**Files affected:**
- `include/enjin2/scripting/bindings.hpp` — Has `lua_engine_log` function
- No profiling infrastructure

**What exists:**
- `engine.log(msg)` — Print to console
- `engine.lua.memory()` — Get Lua heap size
- `engine.lua.collect()` — Trigger GC

**What's missing:**
- No frame time breakdown (physics, render, script time)
- No memory profiler (allocation hot spots)
- No visual collision debugger (draw AABBs/circles for debugging)
- No network throttling simulation
- No sprite atlas usage stats

**Recommended approach:**
1. **Simple frame profiler:**
   ```lua
   engine.profiler.start("physics")
   -- physics code
   engine.profiler.stop("physics")
   engine.profiler.report()  -- Print breakdown
   ```
2. **Visual debug mode:**
   ```lua
   engine.debug.drawColliders(true)  -- Draw all AABBs
   engine.debug.drawGridLines(true)  -- Draw tilemap grid
   ```
3. **Memory breakdown:** `engine.profiler.memory()` shows per-system usage

**Why not already done:**
- Logging is sufficient for early development
- Profiling is a nice-to-have; most games don't need it until performance problems arise
- Visual debug adds complexity to rendering pipeline

**Risk:** Performance optimization is difficult without profiler data. Developers must manually add logging throughout their code.

---

## Tech Debt & Fragile Areas

### Area 1: Static Allocation Hard Limits

**Files:** `include/enjin2/core/object_collection.hpp`, `include/enjin2/components/lua_script.hpp`

**Issue:** Fixed-capacity arrays throughout mean games can exceed limits at runtime with silent failure.

**Examples:**
- `ObjectCollection::MAX_OBJECTS = 128` — scripts cannot spawn more objects
- `LuaBindings::LUA_SPRITE_POOL_SIZE = 16` — only 16 simultaneous sprites
- `LuaStore::STORE_MAX_KEYS = 16` — persistent store caps at 16 key-value pairs
- `MAX_LUA_LAYERS = 8` — hard ceiling on canvas layers

**Current behavior:**
- Sprite pool: `loadSprite()` returns -1 on overflow; Lua code must check
- Objects: `addComponent()` triggers `assert()` in debug builds; release builds silently fail
- Store: Overflow returns false but doesn't log why

**Risk:** Games hitting these limits will exhibit undefined behavior — crashes on embedded targets, silent failures on desktop.

**Mitigation:**
- Add clear error messages at overflow points
- Document limits in README and Lua API docs
- Consider growable arrays for desktop, fixed for embedded (compile-time option)

**Fix approach:**
1. Add `getCapacity()` methods to all collections
2. Emit warnings when 80% capacity is reached
3. Provide `assert(result.error == nullptr)` wrappers for C++ code
4. Lua: Add `engine.limits` table with current usage: `engine.limits.objectCount()`, `engine.limits.spritePoolUsed()`

---

### Area 2: Lua Memory Management Under Pressure

**Files:** `include/enjin2/scripting/lua_engine.hpp`, `src/scripting/lua_platform.cpp`

**Issue:** Fixed Lua memory pool (256 KB default) can run out without clear recovery path.

**Details:**
- Lua heap is pre-allocated once at startup: `lua_newstate(LuaAllocator, this)`
- When OOM: Lua calls the allocator with size 0, expecting free; if no pool space exists, allocation fails
- Current behavior: Lua disables the script with error policy `ScriptErrorPolicy::Disable` — game continues but script is gone
- No recovery mechanism: The script stays dead until scene reload

**Risk:**
- Long-running scripts (scene lives 5+ minutes) can accumulate garbage despite GC
- `engine.lua.collect()` helps but requires developer discipline
- If called in wrong place (mid-loop) it can cause frame drops

**Related code:**
- `src/scripting/lua_platform.cpp:96` — LUA allocator implementation
- Memory pool allocation in `LuaEngine` constructor

**Mitigation:**
1. Default memory cap should warn at 70% usage
2. Add `engine.lua.getAllocationStats()` to return {used, total}
3. Document collection strategy: call `collect()` in scene `onDeactivate()`, not `onUpdate()`

**Fix approach:**
1. Track peak memory usage per frame
2. Log warning if peak > 70% of pool
3. Provide Lua API to inspect allocations (not supported by standard Lua; would require custom allocator instrumentation)

---

### Area 3: ScriptProxy Lifetime Safety

**Files:** `include/enjin2/scripting/bindings.hpp` (ScriptProxy struct), `src/components/lua_script.cpp`

**Issue:** ScriptProxy objects returned from `engine.scene.find()` can become invalid if the underlying Object is destroyed.

**Details:**
- `engine.scene.find("name")` returns a `ScriptProxy` userdata (C++ struct with pointer)
- If the Object is destroyed (via `engine.scene.destroy()`), the proxy pointer becomes dangling
- Current safeguard: `valid` flag checked in `__index`/`__newindex`; if false, raises Lua error
- Problem: Flag is only set false during Lua state destruction, not when Object is destroyed

**Risk:**
- Script holds reference to destroyed object: calling methods on it raises error "Object was destroyed"
- The error is correct, but happens at runtime — no way to prevent it at compile time
- Complex scripts managing object lifetimes can have hard-to-debug proxy invalidation errors

**Related code:**
- `include/enjin2/scripting/object_proxy.hpp` — ObjectProxy class
- `src/scripting/bindings_engine.cpp:lua_engine_scene_find()` — returns proxy

**Current safeguards:**
- ObjectProxy invalidated when Object destructor fires (destructor hook implemented)
- Invalidation checked every `__index` call

**Remaining risk:**
- If Lua garbage collection runs between object destruction and proxy access, behavior is undefined (depends on GC timing)
- No way for script to know if a proxy is still valid without calling a method and catching the error

**Mitigation:**
1. Add `proxy:isValid()` method to allow defensive checks
2. Document: "Do not store proxies across scene transitions; store names instead and call find() again"
3. Consider adding weak references (Lua metatables) to automatically invalidate proxies on object destruction

**Fix approach:**
1. Add weak reference support to ScriptProxy
2. Implement `__gc` metamethod to clean up stale proxies
3. Add logging if proxy accessed after invalidation (for debugging)

---

### Area 4: Input per-Frame Wiring in WASM/ESP32

**Files:** `src/components/lua_script.cpp`, `src/platform/sdl/sdl_main.cpp`

**Issue:** Input callbacks (`on_button_pressed`, `on_button_released`) require `C_LuaScript::setInput()` to be called every frame, but this is only wired in SDL3 runner.

**Details:**
- SDL3 runner: Calls `setInput()` after polling (line ~180 in `sdl_main.cpp`)
- WASM: No integration yet — input not wired to Lua scripts
- ESP32: No integration yet — input not wired to Lua scripts

**Risk:**
- Input callbacks only work on desktop; WASM/ESP32 scripts won't receive edge events
- Developers might assume callbacks work across platforms (PICO-8 style) and be surprised

**Related code:**
- `include/enjin2/components/lua_script.hpp:setInput()` — should be called per-frame
- `src/platform/sdl/sdl_main.cpp:~250` — SDL does it correctly
- Emscripten bindings: No `setInput()` call found

**Mitigation:**
1. Document in API reference: "Input callbacks only available in SDL3 desktop runner; use polling on WASM/ESP32"
2. Add #ifdef guards to disable input callbacks on non-SDL platforms
3. Plan WASM integration for v1.6 (after JavaScript input event handling)

**Fix approach:**
1. WASM: Wire `input_platform_poll()` to JavaScript keydown/keyup events
2. ESP32: Call `setInput()` after `input_platform_poll()` in host application (document required pattern)
3. Add testing for all three platforms to verify callbacks fire

---

### Area 5: Scene Self-Transition Edge Case

**Files:** `include/enjin2/core/scene_state_machine.hpp`, `src/core/scene.cpp`

**Issue:** Restarting the same scene requires special handling; `SceneStateMachine::switchTo()` currently deactivates then activates, which may not be intuitive.

**Details:**
- Calling `engine.scene.switch(current_scene_id)` triggers deactivate then activate
- This fires `onDeactivate()` followed by `onActivate()` — scripts and objects are reset
- For a "restart" operation, this is correct, but it's not immediately obvious
- No explicit `restart()` function; users must call `switch(self.id)`

**Risk:**
- Scripts expecting to persist state across "restart" will be surprised
- No way to distinguish "reload scene" from "switch to different scene" in callbacks

**Related code:**
- `include/enjin2/core/scene_state_machine.hpp:switchTo()` — handles self-transition
- `src/core/scene.cpp:onActivate()` — re-initializes state

**Current behavior:** Correct — restarting is a full reload

**Mitigation:**
1. Add explicit `engine.scene.restart()` function (alias for `switch(current_id)`)
2. Document: "Restarting destroys and recreates all objects in the scene"
3. Provide hook: Allow scripts to save state before deactivate if restart is imminent

**Fix approach:**
1. Add `engine.scene.restart()` function for clarity
2. Add `onBeforeRestart()` callback (optional) so scripts can save state
3. Document pattern: Use `engine.store.*` to persist data across scene reloads

---

### Area 6: Component Assertion Overhead

**Files:** `include/enjin2/core/component.hpp:assertRequires<T>()`, `include/enjin2/scripting/bindings.hpp`

**Issue:** `assertRequires<T>()` only fires in debug builds; release builds log once and disable component. This asymmetry can hide bugs.

**Details:**
- Debug: Calls `assert(false)` with message → crashes at startup, catches missing dependencies
- Release: Calls `printf()` once and sets `enabled = false` → component silently disabled, game continues
- This is intentional (embedded targets can't crash), but creates debug-vs-release divergence

**Risk:**
- A dependency missing in release (but caught in debug) could cause subtle game bugs
- No way to enforce "fail-fast" behavior on ESP32

**Related code:**
- `include/enjin2/core/component.hpp:46-52` — The divergent implementations

**Current behavior:** Correct for embedded targets, but potentially unsafe for shipped games

**Mitigation:**
1. Log with higher visibility (not just `printf()`) — use `engine.log()` from Lua
2. Add `ScriptErrorPolicy` concept to components (currently only on C_LuaScript)
3. Provide compile-time option to treat missing deps as fatal even in release

**Fix approach:**
1. Add `COMPONENT_STRICT_DEPS` CMake option (default OFF for ESP32, ON for desktop)
2. When enabled: missing components fire platform panic handler in release builds
3. Document this in README

---

## Security Concerns in Lua Sandbox

### Sandbox #1: Unrestricted File Access

**Files:** `src/scripting/bindings.cpp` (sprite loading), `src/scripting/bindings_store.cpp` (store load/save)

**Issue:** Lua scripts can read/write any file accessible to the process.

**Details:**
- `engine.sprite.load("../../../etc/passwd")` — nothing prevents directory traversal
- `engine.store.load("/tmp/malicious.json")` — can read arbitrary files
- No sandbox restrictions; no allowlist of permitted directories

**Risk:** LOW for embedded devices (usually just one admin user), HIGH for web/multi-user scenarios.

**Current safeguard:** None — relies on OS file permissions.

**Recommended fix:**
1. Add `ASSET_ROOT_PATH` constant; all file opens must be relative to it
2. Reject paths containing `..` or absolute paths starting with `/`
3. Whitelist permitted directories in CMake config

**Implementation:**
```cpp
std::string sanitizePath(const char* path, const char* rootPath) {
    if (path[0] == '/' || strstr(path, "..") != nullptr) {
        return "";  // Reject absolute/traversal paths
    }
    // Concatenate and check result is under rootPath
}
```

**Priority:** Medium (enforce for v1.6; critical if using enjin2 for user-uploaded scripts)

---

### Sandbox #2: Infinite Loop Crash

**Files:** `src/scripting/bindings.cpp`, all callback dispatch in `src/components/lua_script.cpp`

**Issue:** Lua script can hang the engine with infinite loop.

**Example:**
```lua
function update(self, dt)
    while true do end  -- Engine hangs forever
end
```

**Risk:** HIGH for untrusted scripts; LOW if you control game code. No protection.

**Current behavior:** Engine freezes; requires hard reboot (embedded) or Ctrl-C (desktop).

**Recommended fix:**
1. Add timeout to Lua script execution: wrap `lua_call()` with signal handler or timeout thread
2. Platform-specific: SDL can use timeout check in event loop; ESP32 can use watchdog timer
3. **Hard problem:** Can't interrupt Lua safely mid-execution without breaking internal state

**Practical approach for v1.6:**
- Document: "Do not use infinite loops; use game loop frame updates instead"
- Provide loop guard: `if engine.time.totalTime > MAX_FRAME_TIME then error("Timeout") end`
- Add lint rule: Flag `while true` in analysis tools

**Priority:** Low (solved by good development practices, not critical for shipped games)

---

### Sandbox #3: Stack Overflow / Stack Exhaustion

**Files:** `src/scripting/lua_engine.cpp` (Lua stack management)

**Issue:** Deeply recursive Lua functions can exhaust the Lua stack, causing crash.

**Risk:** Medium — malicious scripts or complex recursion can trigger this.

**Current safeguard:** Lua has `LUA_MAXCALLS` limit; default is usually 20,000 depth. This is deep enough for most games.

**Mitigation:**
1. Lua already prevents stack overflow in `lvm.c` via call depth check
2. Document: "Avoid deep recursion; use iteration instead"

**Priority:** Low (Lua built-in protection is sufficient)

---

## Performance Bottlenecks

### Bottleneck #1: Linear Sprite Lookup

**Files:** `src/scripting/bindings_sprite_load.cpp:lua_loadSprite()`, `include/enjin2/scripting/bindings.hpp:LuaBindings`

**Issue:** Finding a free sprite slot in the 16-sprite pool is O(n) linear scan.

**Code:**
```cpp
for (int i = 0; i < LUA_SPRITE_POOL_SIZE; i++) {
    if (!spritePool[i].active) {
        // Use this slot
    }
}
```

**Impact:** NEGLIGIBLE at 16 sprites (average 8 comparisons). Not a bottleneck.

**Mitigation:** Not needed unless pool grows beyond 64 slots.

**Priority:** Low (not a real problem at current scale)

---

### Bottleneck #2: Object Lookup by Name

**Files:** `src/core/object_collection.cpp:findByName()`

**Issue:** Finding an object by name is O(n) linear scan over all objects.

**Code:**
```cpp
for (int i = 0; i < objectCount; i++) {
    if (strcmp(objects[i].name, name) == 0) {
        return &objects[i];
    }
}
```

**Impact:** SIGNIFICANT for scenes with 100+ named objects. Average case: 50 comparisons per find. This happens frequently in scripts.

**Risk:** Frame drops if scripts call `engine.scene.find()` many times per frame.

**Recommended fix:**
1. Add optional `NameMap<Object*>` (hash table or binary search tree) on ObjectCollection
2. Use only if `enableNameIndexing = true` in scene config
3. Cost: ~64 bytes per name, 2-4 comparisons to look up vs. 50

**Implementation:** `std::array<Object*, MAX_OBJECTS>` sorted by name; use binary search

**Priority:** Medium (profile to confirm actual impact; premature optimization risk)

---

### Bottleneck #3: Layer Composition Cost

**Files:** `src/graphics/layer_compositor.cpp`, `src/platform/sdl/sdl_main.cpp`

**Issue:** Compositing 4 layers into final framebuffer is done at blit time.

**Cost per frame:**
- Read from Canvas4 layer buffers: 128×64 × 4 layers = 32,768 pixels
- Compose with index-15 transparency: 32K comparisons
- Write to final buffer: 32K writes
- Total: ~100K operations per 60 FPS = 6M ops/sec

**Impact:** On ESP32-S3 (240 MHz), this is ~0.03% of total CPU. Negligible.

**Mitigation:** None needed. Current implementation is optimal for static allocation.

---

### Bottleneck #4: Lua Garbage Collection Pause

**Files:** `src/scripting/lua_engine.cpp`, `src/scripting/lua_platform.cpp`

**Issue:** Lua GC can pause the game frame for 1-5 ms (observable on ESP32).

**Symptom:** Frame times spike randomly; scrolling stutters.

**Cause:** Lua runs generational GC by default. Full collection stops the world.

**Current mitigation:** `engine.lua.collect()` lets scripts schedule collection explicitly.

**Recommended improvement:**
1. Use incremental GC mode: `lua_gc(L, LUA_GCINC, ...)` (Lua 5.3+; need LuaJIT patch for 5.1)
2. Configure small step size: `lua_gc(L, LUA_GCINC, 10)` (collect 10% of heap per frame instead of all-at-once)
3. Document: Call `engine.lua.collect()` at scene boundaries, not in update

**Implementation:**
```cpp
// In LuaEngine::initialize()
lua_gc(L, LUA_GCINC, 10);  // Incremental mode, 10% steps
```

**Priority:** Medium (improves ESP32 frame stability in long-running scenes)

---

### Bottleneck #5: Drawable Component Type Checking

**Files:** `include/enjin2/components/drawable.hpp`, `src/components/drawable.cpp`

**Issue:** `C_Drawable::draw()` dispatch uses `dynamic_cast` on every frame.

**Cost:** One RTTI check per drawable per frame. With 50 drawables: 50 checks per 60 FPS = 3K RTTI ops/sec. Negligible.

**Impact:** None at current scale. Not a concern.

---

## Test Coverage Gaps

### Gap #1: WASM Input Integration Not Tested

**Files:** Emscripten bindings, `include/enjin2/input/input_state.hpp`

**What's not tested:**
- `input_platform_poll()` on WASM (converts JS keyboard to `InputState`)
- Input callbacks (`on_button_pressed`) on WASM
- Gamepad input on web browser

**Risk:** Input works on SDL3/desktop; WASM code path may have bitwise logic errors.

**Recommendation:** Add integration test:
```cpp
// tests/wasm_input_test.cpp
#ifdef __EMSCRIPTEN__
TEST(WasmInput, ButtonPressConversion) {
    InputState state;
    // Simulate JavaScript event arrival
    // Verify state->buttons has correct bit set
}
#endif
```

**Priority:** Medium (critical for WASM adoption)

---

### Gap #2: Scene Transition With Active Lua Scripts

**Files:** `src/core/scene_state_machine.cpp`, `src/components/lua_script.cpp`

**What's not tested:**
- What happens if a script calls `engine.scene.switch()` mid-update?
- Does `onDeactivate()` fire before the switch?
- Are all script instances properly destroyed?

**Risk:** Scripts might be in inconsistent state during transition; callbacks might fire out of order.

**Recommendation:** Add test:
```cpp
TEST(SceneTransition, SwitchDuringLuaUpdate) {
    // Script A calls engine.scene.switch(1) in its update()
    // Verify: onDeactivate() fires, Script A is destroyed, Scene 1 activates
}
```

**Priority:** Medium (affects game stability if scripts trigger transitions)

---

### Gap #3: ObjectProxy Lifetime After Scene Reload

**Files:** `include/enjin2/scripting/object_proxy.hpp`, tests for Lua proxies

**What's not tested:**
- Script holds a proxy to object A
- Scene transitions (objects destroyed)
- Script accesses the proxy after transition
- Does proxy invalidation work correctly?

**Risk:** Use-after-free if invalidation logic is incorrect.

**Recommendation:** Add test:
```cpp
TEST(ObjectProxy, InvalidationAfterSceneSwitch) {
    // Script gets proxy to object in Scene 0
    // Switch to Scene 1 (destroys all objects in Scene 0)
    // Script tries to access proxy; should raise "Object was destroyed" error
}
```

**Priority:** High (critical for Lua safety; currently code-inspected but not integration-tested)

---

### Gap #4: Tilemap Collision Performance

**Files:** Not yet written (tilemap system missing)

**What's not tested:**
- Would be: tilemap grid queries under load (100 tiles per frame)
- Collision detection against tilemap

**Risk:** No real-world performance data; implementation choices might be suboptimal.

**Priority:** Deferred (after tilemap system implemented in v1.6)

---

## Known Issues (Minor)

### Issue #1: getPaletteRGB() Returns Snapshot

**Files:** `include/enjin2/graphics/palette.hpp:getPaletteRGB()`

**Problem:** Function returns snapshot buffer; if palette is modified after call, buffer is stale.

**Code:**
```cpp
const uint8_t* rgb = canvas.getPaletteRGB();  // rgb points to internal buffer
canvas.setPaletteColor(0, 255, 0, 0);        // Modifies buffer! rgb is now stale
```

**Workaround:** Re-call `getPaletteRGB()` after palette mutation.

**Impact:** Low — only affects code that modifies palette mid-frame. Documented in API.

**Status:** Known limitation, not a bug. Document in API reference.

---

### Issue #2: const const Duplication (Cosmetic)

**Files:** Generated API documentation (cosmetic only, not in code)

**Problem:** Doxygen output sometimes produces "const const" in parameter names.

**Impact:** Zero — purely cosmetic in HTML docs.

**Status:** Known, fixed in v1.2 doc generation. Closed.

---

### Issue #3: LUA_SPRITE_POOL_SIZE Hardcoded

**Files:** `include/enjin2/scripting/bindings.hpp:342`, `include/enjin2/components/lua_script.hpp`

**Problem:** Sprite pool size is compile-time constant (16); cannot be changed at runtime.

**Impact:** Games needing >16 simultaneous sprites must rebuild C++. Not ideal.

**Recommendation:**
1. Increase default to 32 (memory cost: ~2 KB)
2. Add CMake option: `-DLUA_SPRITE_POOL_SIZE=64`

**Priority:** Low (16 sprites is usually sufficient for small displays)

---

## Architecture-Level Concerns

### Concern #1: Static Allocation Limits Scalability

**Files:** Throughout (object pools, arrays, component limits)

**Issue:** Zero-allocation design means hard ceilings. Cannot grow gracefully; must recompile to increase limits.

**Impact:** Long-term maintenance burden; every shipped game needs custom recompile for higher limits.

**Mitigation:**
1. Document all limits clearly in README and generated API docs
2. Provide CMake options for common limits (object count, sprite pool, layers, Lua memory)
3. Add `engine.limits.remaining()` introspection functions
4. Consider hybrid approach: static for embedded, growable for desktop/WASM (separate build profiles)

**Example recompile command:**
```bash
cmake -DMAX_OBJECTS=256 -DLUA_SPRITE_POOL_SIZE=32 -DLUA_MEMORY_KB=512 ..
```

**Priority:** Medium (important for shipping real games, less important for prototyping)

---

### Concern #2: Per-Lua-State Isolation vs. Shared State Trade-Off

**Files:** `src/scripting/lua_engine.cpp`, `src/components/lua_script.cpp`

**Issue:** All C_LuaScript instances share one global Lua state. Advantages: memory efficient. Disadvantages: scripts can clobber each other's globals.

**Current design decision:** Shared state, documented convention (use unique function names per script file).

**Trade-off:**
- **Shared:** 1×Lua state, all scripts in it. Cost: ~256 KB. Scripts must be careful not to pollute globals.
- **Isolated:** One state per script. Cost: 16×256 KB = 4 MB (if 16 scripts). Scripts are safer, but memory explosion.

**Impact:** Developers unfamiliar with the constraint might write scripts that interfere.

**Mitigation:**
1. Document clearly: "All scripts run in shared Lua state; wrap state in local tables to avoid conflicts"
2. Provide example: `local scriptState = {}; function scriptState.init(self) ... end`
3. Add linter rule: Flag global variable assignments in script analysis
4. **Do not change:** Isolation would break embedded memory budgets

**Priority:** Low (architectural constraint, not a bug)

---

### Concern #3: Component Dependency Order Not Guaranteed

**Files:** `include/enjin2/core/object.hpp:getComponents<T>()`, `src/core/object.cpp`

**Issue:** Components are updated in the order they were added, not in dependency order. If A depends on B, but B is added after A, B will update after A in the same frame.

**Example:**
```cpp
obj->addComponent<C_LuaScript>();     // Script runs first (updates game logic)
obj->addComponent<C_Position>();      // Position updated after (would use stale data in next frame)
```

**Impact:** Subtle bugs if components read each other's state mid-frame.

**Mitigation:**
1. Document: "Add components in dependency order: Position first, then Physics, then Draw, then Script"
2. Provide `setUpdatePriority<T>(int)` to explicit sort (deferred to v1.6)
3. Add runtime warning if component added after higher-priority component

**Priority:** Low (developers should follow convention; serious issues are rare)

---

### Concern #4: No Event System Between Objects

**Files:** Core architecture

**Issue:** Objects cannot easily communicate without shared globals or C_LuaScript reference passing.

**Current pattern:** `engine.scene.find("target")->broadcast("message")` — no such API exists.

**Workaround:** Developers must pass object references explicitly or use global tables.

**Impact:** Complex games need custom event/signal system in Lua.

**Recommendation:** **Deferred to v1.6** as a separate "event bus" system.

**Why not in v1.5:** Orthogonal to Lua scripting foundation; can be added as optional subsystem.

**Priority:** Medium (add in v1.6 for richer game logic patterns)

---

## Security & Stability Summary

| Concern | Severity | Impact | Status | Fix Priority |
|---------|----------|--------|--------|--------------|
| Missing tilemap system | CRITICAL | Level-based games blocked | Not implemented | P1 (v1.6) |
| Missing camera system | HIGH | Viewport control unavailable | Not implemented | P1 (v1.6) |
| Missing physics system | HIGH | Manual physics code required | Partial (collision only) | P1 (v1.6) |
| Missing audio system | HIGH | No sound feedback | Not implemented | P2 (v1.7) |
| ScriptProxy lifetime safety | MEDIUM | Use-after-free risk | Code inspection verified | Medium (add tests) |
| Input per-frame wiring (WASM/ESP32) | MEDIUM | Callbacks only work on SDL3 | Wired only on desktop | Medium (v1.6) |
| Static allocation limits | MEDIUM | Hard ceilings, recompile needed | By design | Medium (document) |
| Lua unrestricted file access | MEDIUM | Directory traversal possible | No safeguard | Medium (add guards) |
| Object name lookup O(n) | MEDIUM | Frame drops with 100+ objects | Not optimized | Medium (profile first) |
| Lua GC pause spikes | MEDIUM | Frame drops on ESP32 | Documented workaround | Medium (incremental GC) |
| Missing UI layout system | LOW | Manual widget positioning | UI widgets exist | Low (v1.6+) |
| Missing particle effects | LOW | Visual polish limited | PostFx only | Low (v1.6+) |
| Missing animation tweening | LOW | Manual animation code | Sprite animation exists | Low (v1.6+) |
| Infinite loop crash risk | LOW | Untrusted scripts only | Documentation | Low (doc + lint) |
| Missing debugging tools | LOW | Manual profiling | Logging exists | Low (v1.7+) |

---

*Concerns audit: 2026-02-28 — enjin2 v1.5 Lua Scripting Foundation (127 commits)*

**Next steps:** Address P1 missing features in v1.6 (tilemap, camera, physics). Address medium security concerns (input wiring, file access guards). Update test coverage for Lua integration edge cases.
