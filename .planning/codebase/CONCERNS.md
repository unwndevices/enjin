# Codebase Concerns

**Analysis Date:** 2026-03-01

## Compilation Errors

**Missing Header: lua_wrapper.hpp**
- Issue: `tests/sprite_load_test.cpp:3` references a non-existent header `../include/enjin2/scripting/lua_wrapper.hpp`
- Files: `tests/sprite_load_test.cpp`
- Impact: Build fails completely. This is a critical pre-existing issue blocking compilation.
- Fix approach: Either remove the include and fix the test, or create the missing header with necessary Lua wrapper utilities. The header appears to be intended for Lua script loading/execution patterns.

## Tech Debt

**Monolithic Bindings Files**
- Issue: Several binding files have grown to 900+ lines, making them difficult to maintain
- Files:
  - `src/scripting/bindings.cpp` (1390 lines)
  - `src/scripting/bindings_engine.cpp` (904 lines)
  - `src/scripting/bindings_physics.cpp` (514 lines)
  - `src/scripting/bindings_store.cpp` (512 lines)
- Impact: Hard to navigate, high cognitive load, increased merge conflict risk, difficult to test in isolation
- Fix approach: Consider extracting nested binding functions into smaller modules or using a factory pattern to generate binding code programmatically

**Global Pointer for Callback Management**
- Issue: `static LuaBindings* g_currentBindings = nullptr;` in `src/scripting/bindings.cpp:16` uses a global singleton pointer for static callback dispatch
- Files: `src/scripting/bindings.cpp`
- Impact: Thread-unsafe, difficult to test, violates encapsulation, could cause issues in multi-threaded contexts or multiple engine instances
- Fix approach: Implement a proper callback context mechanism (e.g., userdata-based closure, TLS, or callback wrapper objects)

**Manual Memory Management in Bindings**
- Issue: `new` allocations used in `src/bindings/emscripten_bindings.cpp:164,169,174,183,198` and elsewhere
- Files:
  - `src/bindings/emscripten_bindings.cpp`
  - `src/components/image_cache.cpp:55` (new uint8_t[packedSize])
  - `src/components/lua_script.cpp` (new LuaScriptSystem, new LuaCanvas)
  - `src/components/canvas.cpp` (new Canvas8 instances)
- Impact: Potential memory leaks if exceptions or error paths bypass cleanup, harder to reason about object lifetime
- Fix approach: Convert to std::unique_ptr or std::make_unique throughout for automatic cleanup

**Raw Pointer Chains in Lua Script Component**
- Issue: `C_LuaScript* comp = proxy->component` → `Object* owner = comp->getOwner()` with minimal null checks
- Files: `src/scripting/bindings.cpp` (lines 61-62) and similar patterns throughout
- Impact: A destroyed object/component can leave dangling proxy references that crash when dereferenced
- Fix approach: Implement weak reference counting or a validity check mechanism before dereferencing component pointers

## Known Bugs

**Lua Wrapper Header Missing**
- Symptoms: Compilation fails with "No such file or directory: lua_wrapper.hpp"
- Files: `tests/sprite_load_test.cpp:3`
- Trigger: Any build attempt
- Workaround: Comment out the include and the code that depends on it (if any)

**Tilemap Maximum Dimensions Constraint**
- Symptoms: Tilemaps can't exceed 64x64 tiles (4096 tile array), silently clamping larger maps
- Files: `src/components/tilemap.cpp:34-35` (clamping in setTiles), `include/enjin2/components/tilemap.hpp` (MAX_MAP_W=64, MAX_MAP_H=64)
- Trigger: Attempting to load a tilemap with width or height > 64
- Workaround: Split larger maps into multiple tilemap components, or implement multi-layer tilemap with dynamic allocation
- Note: This is by design for embedded targets but limits flexibility for larger games

**Event Bus Channel Overflow**
- Symptoms: When MAX_CHANNELS (16) is exceeded, new event subscriptions silently fail and print to stdout
- Files: `src/scripting/lua_event_bus.cpp:36,49`
- Trigger: Engine scripts emit more than 16 distinct event types (e.g., 'player.jump', 'enemy.die', 'ui.show', etc.)
- Workaround: Consolidate events into fewer channels (e.g., use a single 'event' channel with type field)
- Impact: Lua scripts silently lose event handlers without clear error feedback

**Event Bus Subscriber Overflow**
- Symptoms: When MAX_SUBS_PER_CH (8) is exceeded per channel, new handlers silently fail
- Files: `src/scripting/lua_event_bus.cpp:49`
- Trigger: Attaching more than 8 listeners to a single event (e.g., 10 UI panels listening to 'gamestate.paused')
- Workaround: Use a broadcast pattern or combine multiple listeners into one
- Impact: Loss of handler registration with only a printf() warning

**Coordinate Type Coercion Issues**
- Symptoms: Drawing functions silently truncate float coordinates to integers, causing position discrepancies
- Files: `src/scripting/bindings_draw.cpp` (and related) — casting lua_Number to int without explicit flooring
- Trigger: Passing float coordinates (e.g., 10.7, 20.3) to drawing functions expecting integers
- Workaround: Manually call math.floor() before drawing
- Impact: Positions shift unexpectedly when using floating-point coordinates; anti-aliasing/smoothness lost
- Note: Documented in project feedback as a known friction point

## Performance Bottlenecks

**Tilemap Rendering with Large Maps**
- Problem: Rendering entire 64x64 tilemap every frame without viewport culling
- Files: `src/components/tilemap.cpp` (render implementation not explicitly visible, but implied in drawable)
- Cause: No spatial optimization — renders all tiles regardless of camera viewport
- Improvement path: Implement viewport-based culling in render() to skip tiles outside visible region, cache tile quad geometry

**Sprite Pool Fixed Size**
- Problem: LUA_SPRITE_POOL_SIZE = 16 (defined in `include/enjin2/scripting/bindings.hpp:381`) severely limits dynamic sprite creation in Lua
- Files: `include/enjin2/scripting/bindings.hpp:381`, `src/scripting/bindings_sprite_load.cpp`
- Cause: Fixed-size zero-alloc pool prevents more than 16 concurrent sprite instances
- Improvement path: Implement pool resizing or fallback allocation; document pool limits clearly in API; add pool exhaustion callbacks

**Event Bus Linear Search**
- Problem: findChannel() and unsubscribe() linearly search through all channels/subscribers
- Files: `src/scripting/lua_event_bus.cpp:6-13, 88-105`
- Cause: O(n) lookups for every event emit with 16+ channels or 8+ subscribers per channel
- Improvement path: Hash table for channel lookup (e.g., map<string, Channel*>) or pre-computed channel indices

**Lua Stack Exhaustion Risk**
- Problem: Multiple nested Lua bindings push stack frames without clear depth management
- Files: `src/scripting/bindings.cpp` (1390 lines of stack manipulation)
- Cause: MAX_RECURSION_DEPTH = 32 (small limit on embedded targets) easily exceeded by deep script calls
- Improvement path: Add lua_checkstack() assertions before multi-push operations; profile typical call depth

## Fragile Areas

**ScriptProxy Lifetime Management**
- Files: `src/scripting/bindings.cpp` (ScriptProxy metatable at line 22+)
- Why fragile:
  - ScriptProxy holds raw C++ pointers to Components that can be destroyed without proxy invalidation
  - Accessing proxy after owner destruction causes crashes (luaL_error at line 45)
  - No strong reference counting from Lua to C++
- Safe modification:
  - Always check proxy->valid and proxy->component before use
  - Add Lua GC finalizers to explicitly clear dead references
  - Implement a weak-reference proxy pool that invalidates on component destruction
- Test coverage: `tests/script_proxy_lifetime_test.cpp` (174 lines) exists but may have gaps

**Component Proxy Dispatch System**
- Files: `src/scripting/bindings.cpp` (lua_proxy_get_component_impl at line 30+, used for self:get("TypeName"))
- Why fragile:
  - Type dispatch is string-based (strcmp) with no validation of component existence
  - Adding new component types requires manual updates to get() dispatch logic
  - No type registry or reflection system
- Safe modification:
  - Add type checking before dispatch
  - Create a ComponentFactory registry for safer type lookups
  - Document all supported component types in a centralized list
- Test coverage: `tests/component_proxy_test.cpp` (299 lines) exists

**Camera Lerp with Extreme Values**
- Files: `src/components/camera.cpp:45-55` (update function with factor clamping)
- Why fragile:
  - lerpSpeed * 10.0f * dt can produce factor > 1.0f if dt is large (frame drops)
  - No protection against negative lerpSpeed values
  - Shake offset uses hardcoded sine frequency (40.0f, 30.0f) which may cause visual artifacts at different dt values
- Safe modification:
  - Clamp lerpSpeed input in setter
  - Document dt expectations (assumes < 0.1 seconds)
  - Make shake frequency data-driven or adaptive
- Test coverage: `tests/camera_lua_test.cpp` (424 lines) and `tests/camera_test.cpp` (367 lines)

**Tilemap Coordinate Conversion**
- Files: `src/components/tilemap.cpp:74-85` (floorDiv function handling negative coordinates)
- Why fragile:
  - Custom floor division logic to handle C++ truncation toward zero
  - Conversion between screen/world/tile coordinates is error-prone
  - Scroll offset can produce unexpected tile indices with edge cases
- Safe modification:
  - Add comprehensive unit tests for boundary cases (negative coords, wrap-around)
  - Consider using a separate CoordinateConverter utility
  - Document coordinate system semantics
- Test coverage: `tests/tilemap_test.cpp` (454 lines) and `tests/tilemap_lua_test.cpp` (545 lines) exist

## Security Considerations

**String Length Limits in Event Bus**
- Risk: MAX_NAME_LEN = 64 in LuaEventBus could allow event name truncation without warning
- Files: `include/enjin2/scripting/lua_event_bus.hpp:18`, `src/scripting/lua_event_bus.cpp:23`
- Current mitigation: strncpy() with explicit null termination
- Recommendations:
  - Add length validation and error returns for names > MAX_NAME_LEN
  - Log warnings when truncation occurs
  - Consider dynamic string allocation for event names

**Store Key/Value Length Limits**
- Risk: Keys truncated at STORE_MAX_KEY=64, values at STORE_MAX_STRING=128, tables at MAX_TABLE_ENTRIES=16
- Files: `include/enjin2/scripting/bindings.hpp:269-272`, `src/scripting/bindings_store.cpp`
- Current mitigation: None — silent truncation
- Recommendations:
  - Add Lua error returns when limits exceeded
  - Document limits prominently in API docs
  - Consider dynamic sizing for non-embedded targets

**Lua Stack Overflow Protection**
- Risk: MAX_RECURSION_DEPTH = 32 is very low; deep recursion crashes with buffer overflow
- Files: `include/enjin2/scripting/lua_platform.hpp:59`
- Current mitigation: LuaPlatformConfig::MAX_RECURSION_DEPTH constant
- Recommendations:
  - Add runtime checks using lua_checkstack()
  - Return Lua errors instead of crashing
  - Document safe call depth limits per platform

**Null Pointer Dereference in Bindings**
- Risk: Many binding functions check `if (!proxy)` or `if (!owner)` but don't validate all chain operations
- Files: `src/scripting/bindings.cpp` (patterns throughout)
- Current mitigation: Sporadic null checks
- Recommendations:
  - Use RAII guards or scope-based validation
  - Centralize null-check logic in helper functions
  - Add assertions for debug builds

## Incomplete/Missing Lua Bindings

**No Global State Manager Binding**
- Problem: C_StateMachine is exposed per-object, but there's no global gamestate manager for Lua
- Files: No implementation exists; feedback in `project/feedback.md` documents this gap
- Impact: Scripts must manually manage global state variables instead of using a structured state machine
- Fix approach: Expose a global engine.state or engine.gamestate sub-table with state transitions and callbacks

**Missing Canvas Constants Binding**
- Problem: No engine.colors.* or engine.buttons.* constant tables in Lua
- Files: No implementation exists
- Impact: Scripts duplicate magic constants; breakage if host mappings change
- Fix approach: Generate lua tables from header enums (e.g., generate from COLOR_* #defines)

**Missing Text Formatting Helpers**
- Problem: No getTextWidth(), setTextAlignment(), or centering support in Lua
- Files: `src/scripting/bindings_layers_text.cpp` (251 lines, but lacks high-level helpers)
- Impact: Manual text width calculations and centering math required in every script
- Fix approach: Bind text measurement functions; add alignment parameter to text() function

**Missing UI Component Bindings**
- Problem: No progress bar, stat bar, or simple UI drawing helpers
- Files: UI system exists at `src/ui/` but not bound to Lua
- Impact: Scripts implement UI from scratch; not reusable across games
- Fix approach: Create engine.ui.* sub-table with common shapes (bar, box, gauge)

**Incomplete Physics Lua Bindings Overloads**
- Problem: Physics functions accept multiple signatures (plain numbers vs Vec2 userdata) but error handling is minimal
- Files: `src/scripting/bindings_physics.cpp` (514 lines of overload dispatch)
- Impact: Type mismatches silently return defaults or throw unclear Lua errors
- Fix approach: Validate argument counts/types explicitly; return clear error messages

**Missing Camera Bounds Debugging**
- Problem: No Lua binding to query current camera bounds
- Files: `src/components/camera.cpp` has setBounds/clearBounds but no getBounds
- Impact: Scripts can't verify bound configuration at runtime
- Fix approach: Add engine.camera.getBounds() to return (minX, minY, maxX, maxY)

## Scaling Limits

**Fixed Object Count Per Scene**
- Current capacity: MAX_OBJECTS = 128 (in `include/enjin2/core/object_collection.hpp:18`)
- Limit: Attempting to spawn the 129th object returns nullptr silently
- Scaling path: Replace fixed array with dynamic allocation (vector) or implement pooling with reuse

**Fixed Drawable Count Per Scene**
- Current capacity: MAX_DRAWABLES = 256 (in `include/enjin2/core/scene.hpp:346`)
- Limit: Drawing >256 objects results in dropping later drawables
- Scaling path: Implement spatial partitioning or streaming to render in chunks

**Fixed Component Count Per Object**
- Current capacity: MAX_COMPONENTS = 16 per Object (in `include/enjin2/core/object.hpp:41`)
- Limit: Attaching >16 components to one object fails silently
- Scaling path: Use dynamic vector; implement component groups/composites

**Fixed Scene Count**
- Current capacity: MAX_SCENES = 32 (in `include/enjin2/core/scene_state_machine.hpp:41`)
- Limit: Loading the 33rd scene fails
- Scaling path: Implement scene streaming/unloading or hot-reload pattern

**Tilemap Size Limit**
- Current capacity: 64x64 = 4096 tiles (in `include/enjin2/components/tilemap.hpp`)
- Limit: Larger maps silently clamp
- Scaling path: Implement chunked tilemap loading or multi-layer system

**Sprite Pool Exhaustion**
- Current capacity: LUA_SPRITE_POOL_SIZE = 16 concurrent sprites
- Limit: 17th sprite creation fails silently
- Scaling path: Implement dynamic sprite allocation or pooling with reuse/recycling

**Event Bus Limits**
- Current capacity: MAX_CHANNELS = 16, MAX_SUBS_PER_CH = 8
- Limit: 17th event channel or 9th subscriber per channel silently fails
- Scaling path: Hash-based channel lookup; dynamic subscriber lists; or broadcast pattern

**Lua Memory Limits**
- Current capacity:
  - Desktop: 1MB (MEMORY_LIMIT in `include/enjin2/scripting/lua_platform.hpp:46`)
  - ESP32: 256KB (line 51)
- Limit: Large scripts or tables exceed memory; Lua allocation fails
- Scaling path: Profile and optimize hot paths; lazy-load modules; increase limit on targets with more RAM

## Test Coverage Gaps

**Untested Tilemap Edge Cases**
- What's not tested: Boundary conditions for tile coordinate conversion with negative scroll offsets, viewport culling, and wrapping
- Files: `tests/tilemap_test.cpp` (454 lines), `tests/tilemap_lua_test.cpp` (545 lines)
- Risk: Tilemap rendering could fail silently with extreme scroll values
- Priority: Medium

**Untested Event Bus Overflow**
- What's not tested: Behavior when MAX_CHANNELS or MAX_SUBS_PER_CH limits are exceeded
- Files: `tests/eventbus_test.cpp` (445 lines)
- Risk: Silent failures when event system is overloaded
- Priority: High

**Untested Sprite Pool Exhaustion**
- What's not tested: Creating >16 sprites and verifying pool exhaustion handling
- Files: `tests/sprite_load_test.cpp` (fails to compile), `tests/sprite_test.cpp` (249 lines)
- Risk: 17th sprite creation behavior undefined
- Priority: High

**Untested Physics With Large Velocities**
- What's not tested: Extreme velocity values (negative, very large) in physics helper functions
- Files: `tests/physics_test.cpp` (216 lines), `tests/physics_lua_test.cpp` (278 lines)
- Risk: Numerical instability or unexpected behavior with edge case values
- Priority: Medium

**Untested Memory Limits**
- What's not tested: Lua memory allocation under heavy load; behavior at MEMORY_LIMIT threshold
- Files: Test infrastructure exists but memory stress tests are absent
- Risk: Out-of-memory crashes without graceful degradation
- Priority: Medium

**Untested Component Destruction During Iteration**
- What's not tested: Destroying components or objects while iterating over them in callbacks
- Files: `tests/` (no specific test for this race condition)
- Risk: Use-after-free in component iteration, proxy invalidation, event callbacks
- Priority: High

**Untested Camera Shake Edge Cases**
- What's not tested: Extreme intensity/duration values, rapid shake() calls, NaN values
- Files: `tests/camera_test.cpp` (367 lines)
- Risk: Visual glitches or undefined shake behavior
- Priority: Low

**Untested Negative Coordinates**
- What's not tested: Drawing/collision at negative coordinates; sprite positions < 0
- Files: Various drawing/collision tests
- Risk: Rendering or collision bugs at screen edges
- Priority: Medium

## Missing Critical Features

**No Reload Without Full Restart**
- Problem: Modifying Lua scripts requires a full engine restart; no hot-reload in released builds
- Impact: Iterative development is slow; testing gameplay changes requires recompilation
- Potential mitigation: Implement a reload() function that clears old state and reloads script files

**No Debug Draw Bindings**
- Problem: Debug visualization (bounding boxes, collision shapes, grid) not exposed to Lua
- Impact: Difficult to debug collision/physics issues in-game
- Potential mitigation: Add engine.debug.* sub-table for drawing debug shapes

**No Async/Promise Pattern**
- Problem: All Lua code is synchronous; long operations (loading, network) block
- Impact: Games with loading screens or network communication must use manual state machines
- Potential mitigation: Implement coroutine-based async helpers or yield-based scheduling

**No Viewport/Camera Lock-On Helpers**
- Problem: Camera:lookAt() requires manual target calculation for common patterns (follow player, lead ahead)
- Impact: Camera scripting is verbose for standard follow mechanics
- Potential mitigation: Add engine.camera.follow(target) and engine.camera.leadShot(target, velocity)

**No Save/Load Serialization Helper**
- Problem: Persisting game state (player progress, inventory) requires manual table-to-JSON conversion
- Impact: Every game implements save/load from scratch
- Potential mitigation: Bind a JSON serializer or implement a savegame abstraction

---

*Concerns audit: 2026-03-01*
