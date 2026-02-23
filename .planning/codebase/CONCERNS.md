# Codebase Concerns

**Analysis Date:** 2026-02-23

## Tech Debt

**Compat Layer is Untested and Undocumented:**
- Issue: `include/enjin2/compat/` provides an enjin1 compatibility shim but has never been cleaned up. The compat module group XML exists but is excluded from the Docusaurus documentation generator config.
- Files: `include/enjin2/compat/component.hpp`, `include/enjin2/compat/scene.hpp`, `include/enjin2/compat/types.hpp`, `include/enjin2/compat/module_group.hpp`
- Impact: Compat module is invisible in the published API docs. Users migrating from enjin1 cannot find it. Risk of silent bit-rot as no tests cover it.
- Fix approach: Either document and wire it into `generate-api-docs.js`, or formally deprecate and remove it. Milestone audit explicitly deferred this.

**Examples Directory Contains Dead enjin1-Referencing Files:**
- Issue: Several example files reference old enjin1 APIs and were explicitly deferred during v1.0 cleanup. `examples/enjin_comparison_benchmark.cpp` contains enjin1 references.
- Files: `examples/enjin_comparison_benchmark.cpp`, `examples/adafruit_benchmark.cpp`, `examples/real_adafruit_benchmark.cpp`, `examples/eisei_game_benchmark.cpp`
- Impact: Build confusion for new contributors who run examples. Inconsistent API usage visible in the repo.
- Fix approach: Audit and delete or rewrite stale example files. Confirmed as explicit deferred item in `STATE.md`.

**Canvas8 `fill()` Does Not Use Optimized Path:**
- Issue: `Canvas8::fill()` calls `setPixel()` in a nested loop rather than using `std::fill` or `memset` as `Canvas8::clear()` does. `Canvas4::fill()` also calls individual pixels instead of the optimised `drawHLine()` already present on the class.
- Files: `include/enjin2/graphics/canvas.hpp` lines 314-328 (`Canvas4::fill`), lines 434-448 (`Canvas8::fill`)
- Impact: Fill operations on large regions are significantly slower than clear operations of equivalent size. This affects any scene that fills rectangles frequently (backgrounds, UI panels).
- Fix approach: `Canvas8::fill` should use `memset` per row. `Canvas4::fill` should delegate to `drawHLine`.

**`getTextWidth()` Uses Fixed-Width Calculation Ignoring GFX Font Metrics:**
- Issue: `Canvas8::getTextWidth()` returns `strlen(text) * 6 * textsize_x`, which is the classic 5x7 built-in font width, even when a proportional GFX font is active. All text that uses a non-default font will return wrong width measurements.
- Files: `include/enjin2/graphics/canvas.hpp` lines 719-724
- Impact: Any text centering, layout, or clipping that calls `getTextWidth()` will be wrong for non-default fonts. `getTextBounds()` (lines 1324-1354) correctly walks glyphs and is the right model to follow.
- Fix approach: Replace `getTextWidth` body with a call to `getTextBounds` and return the width component.

**`println()` Uses Hardcoded Line Advance of 8:**
- Issue: `Canvas8::println()` advances `cursor_y` by a hardcoded `8` instead of using `gfx_font->yAdvance * textsize_y` as `write()` does for newlines.
- Files: `include/enjin2/graphics/canvas.hpp` lines 608-613
- Impact: `println()` produces incorrect line spacing when a GFX font with a different y-advance is active. Lines overlap or have inconsistent gaps.
- Fix approach: Use `(gfx_font ? gfx_font->yAdvance : 8) * textsize_y` for the advance amount.

**Doxygen Warning Count Greatly Exceeds Threshold:**
- Issue: CI threshold is set to 20 warnings but the current HEAD state has **304 warnings** in `doxygen-warnings.log`. Phase 9 verification incorrectly claimed 0 warnings. The milestone audit (`.planning/v1.1-MILESTONE-AUDIT.md`) classifies DOC-01 as `unsatisfied`.
- Files: `doxygen-warnings.log` (gitignored), `.planning/v1.1-MILESTONE-AUDIT.md`
- Impact: CI fails at the Doxygen warning gate (Phase 11). Blocks v1.3 milestone. Published documentation has missing or incorrect parameter docs, particularly in the scripting module.
- Fix approach: Phase 12 is currently in progress to fix this. Focus areas: `@param` mismatches in `include/enjin2/scripting/lua_engine.hpp` and graphics canvas headers.

**Phase 11 Was Never Executed:**
- Issue: `.planning/phases/11-documentation-tracking-improvements/` exists but contains no plan files. The milestone audit confirms the phase as "unexecuted — no plans created, no work done." The CI gate it was meant to establish exists (Phase 11 output), but the phase produced no implementation plans.
- Files: `.planning/phases/11-documentation-tracking-improvements/`
- Impact: Process gap — the state machine shows Phase 11 complete but it produced no substantive deliverables.
- Fix approach: Either backfill a SUMMARY.md for the CI work done, or mark the phase as superseded by Phase 12.

---

## Known Bugs

**Lua Callback Registered with Dangling Pointer:**
- Symptoms: Calling a Lua function registered with `registerFunction(name, LuaCallback)` may crash or produce garbage results if the callback is invoked after the original registration site's stack frame is gone.
- Files: `src/scripting/lua_engine.cpp` lines 92-102
- Trigger: `lua_pushlightuserdata(L, reinterpret_cast<void*>(&callback))` takes the address of the local parameter `callback`. The Lua closure stores this as a light userdata pointer. If Lua calls the function after `registerFunction` returns, `callback` is destroyed and the pointer is dangling.
- Workaround: Register only with the `lua_CFunction` overload (line 104) which does not have this issue. The `LuaCallback` overload should store callbacks in a persistent registry.

**`Canvas8::setFont(nullptr)` Adjusts Cursor Incorrectly:**
- Symptoms: When switching from a GFX font back to the built-in font via `setFont(nullptr)`, `cursor_y` is adjusted by -6. But the initial state has `gfx_font` set to `&defaultFont8pt7b` (not nullptr), so the condition `!font && gfx_font` (line 1225) fires on first call to `setFont(nullptr)` even though the user never explicitly set a GFX font.
- Files: `include/enjin2/graphics/canvas.hpp` lines 1218-1231
- Trigger: Constructing a `Canvas8` (which defaults `gfx_font` to `&defaultFont8pt7b`) then calling `setFont(nullptr)`.
- Workaround: Avoid calling `setFont(nullptr)`. Use an explicit font pointer.

**`Scene::render()` Silently Skips Non-uint8_t Canvas:**
- Symptoms: Calling `scene.render(canvas4)` on a `Canvas4<>` (4-bit canvas) silently does nothing for `onRender()` and hits a `static_assert` in `renderObjects()` that only fires for drawable components, not for the scene's own render pass.
- Files: `include/enjin2/core/scene.hpp` lines 115-126, lines 319-329
- Trigger: Any attempt to use `Canvas4` with the `Scene` rendering pipeline.
- Workaround: Only use `Canvas8` (uint8_t) with `Scene`. `Canvas4` must be rendered manually outside the scene system.

---

## Security Considerations

**Lua File I/O Enabled on Desktop with No Sandboxing:**
- Risk: On VCV_RACK (desktop), `LuaPlatformConfig::ENABLE_FILE_IO = true` and `ENABLE_ALL_LIBS = true`. Lua scripts loaded at runtime have full access to the filesystem and OS libraries.
- Files: `include/enjin2/scripting/lua_platform.hpp` lines 46-49, `src/scripting/lua_platform.cpp`
- Current mitigation: `configureSecurityRestrictions()` exists as a method on `LuaPlatform` but its implementation for desktop is unknown without reading the `.cpp`. ESP32 disables file I/O.
- Recommendations: Verify `configureSecurityRestrictions()` actually removes `os.execute`, `io.popen`, and `require` for untrusted scripts on desktop. Document which Lua libraries are restricted.

**`LuaEngine::getState()` Exposes Raw `lua_State*`:**
- Risk: Public method `getState()` (line 216 in `lua_engine.hpp`) hands out the raw Lua state pointer. Any caller can push/pop values, corrupt the stack, or call unsafe functions.
- Files: `include/enjin2/scripting/lua_engine.hpp` line 216
- Current mitigation: None — documented as "for advanced operations".
- Recommendations: Restrict access pattern or add a comment warning about stack integrity requirements.

---

## Performance Bottlenecks

**`ObjectCollection::findObject<T>()` Uses `dynamic_cast` Every Frame:**
- Problem: `findObject<T>()`, `findObjectWithComponent<T>()`, and `hasComponent<T>()` on `Object` all use `dynamic_cast` on every element in the collection. In a game loop calling these per-frame, this is O(n) RTTI lookup per call.
- Files: `include/enjin2/core/object_collection.hpp` lines 149-158, `include/enjin2/core/object.hpp` lines 146-155
- Cause: No component type ID indexing — components are searched by dynamic cast rather than by pre-computed type ID. The `ComponentBase` does define `getComponentTypeID<T>()` but it is not used for lookup.
- Improvement path: Replace `dynamic_cast` in component lookup with type-ID map or sorted array indexed by `getStaticComponentID()`.

**`Canvas8::fillRect()` Does Not Clip Negative Coordinates Before Loop:**
- Problem: `fillRect()` in `Canvas8` (lines 734-746) checks `px >= 0 && py >= 0` inside the inner loop for every pixel. This wastes cycles on the common hot path where rectangles are fully in-bounds.
- Files: `include/enjin2/graphics/canvas.hpp` lines 734-746
- Cause: Defensive in-loop check instead of pre-clamp and loop from 0.
- Improvement path: Pre-clamp `x` and `y` start values to `std::max(0, ...)` before entering the loop.

**Dual-Core Render Queue Allocates Heap Inside `submitRenderCommands()`:**
- Problem: `Canvas4_ESP32S3::submitRenderCommands()` calls `new std::vector<RenderCommand>(commands)` on every frame submission (line 372 in `canvas_esp32s3.hpp`). On an ESP32, dynamic allocation in a render path causes fragmentation and potential OOM.
- Files: `include/enjin2/graphics/canvas_esp32s3.hpp` lines 369-374
- Cause: FreeRTOS queue transfers a pointer to a heap-allocated vector. No pool for command batches.
- Improvement path: Use a fixed-size ring buffer of pre-allocated command arrays instead of heap-allocating per submission.

**`C_ImageCache` Allocates Temporary Heap Buffer for Image Unpacking:**
- Problem: `AddImage()` allocates `new uint8_t[packedSize]` (line 55 in `image_cache.cpp`) to hold a temporary read buffer, then deletes it. On ESP32, every image load causes a heap allocation and deallocation.
- Files: `src/components/image_cache.cpp` lines 54-77
- Cause: Temporary unpacking buffer is not pooled.
- Improvement path: Use a `StackAllocator` (already defined in `include/enjin2/core/memory.hpp`) for the temporary buffer.

---

## Fragile Areas

**`LuaCanvas` Uses Untyped `void*` for Canvas Pointer:**
- Files: `include/enjin2/scripting/bindings.hpp` lines 27-29, `src/scripting/bindings.cpp` lines 14-42
- Why fragile: `LuaCanvas` holds `void* canvasPtr` and an `is4Bit` flag. Every method does a manual `static_cast` to either `ICanvas<Pixel4>*` or `ICanvas<uint8_t>*`. There is no type safety — passing the wrong canvas type silently reinterprets the pointer. Template constructors set the flag correctly at construction but nothing prevents misuse after the fact.
- Safe modification: Do not add new canvas methods without updating all branches. Consider replacing with `std::variant<ICanvas<Pixel4>*, ICanvas<uint8_t>*>`.
- Test coverage: No unit tests for `LuaCanvas` dispatch correctness.

**`g_currentBindings` Global Pointer in `bindings.cpp`:**
- Files: `src/scripting/bindings.cpp` line 8
- Why fragile: `static LuaBindings* g_currentBindings = nullptr` is a module-level singleton. Multiple `LuaBindings` instances (or re-initialization) will silently overwrite the global. Any Lua callback that uses it will access the last-registered instance.
- Safe modification: Ensure only one `LuaBindings` is active at a time. Add an assertion if a second instance is created.
- Test coverage: None.

**`C_Drawable::abs_center` Is a Mutable Static:**
- Files: `include/enjin2/components/drawable.hpp` line 59
- Why fragile: `static Point abs_center` is a class-level mutable (non-const) static shared across all `C_Drawable` instances. Any code that mutates it affects all drawables globally. The value is documented as "(63, 63 for 128x128)" implying it depends on canvas size — but canvas size is not statically known at this point.
- Safe modification: Do not mutate `abs_center` without understanding all code that reads it. It should ideally be passed as a parameter rather than a global.
- Test coverage: None.

**`HandlePool<T>::create()` Has a Double-Scan Inefficiency:**
- Files: `include/enjin2/core/memory.hpp` lines 186-202
- Why fragile: `create()` calls `pool.allocate()` (O(n) scan for free slot) and then does a second O(n) scan through `objects[]` to find a null slot. These two scans could be conflated but are not. Additionally, the comment "This shouldn't happen if pool allocation succeeded" (line 198) indicates the code relies on an invariant that is not enforced — if it fires, the allocated object is silently leaked.
- Safe modification: The dual-scan is safe to call but inefficient. The leak path on the unreachable branch should be documented with a panic/assert.
- Test coverage: None.

---

## Scaling Limits

**`ObjectCollection` Hard Cap of 128 Objects:**
- Current capacity: `MAX_OBJECTS = 128` per `ObjectCollection`
- Limit: Creating the 129th object via `addObject<T>()` returns `nullptr` silently. No error is signaled.
- Files: `include/enjin2/core/object_collection.hpp` line 18
- Scaling path: Increase `MAX_OBJECTS` as a template parameter, or add a user-visible error log when the cap is hit.

**`Object` Hard Cap of 16 Components:**
- Current capacity: `MAX_COMPONENTS = 16` per `Object`
- Limit: Adding a 17th component returns `nullptr` silently.
- Files: `include/enjin2/core/object.hpp` line 39
- Scaling path: Same as above — parametrize capacity.

**`Signal<>` Hard Cap of 16 Connections:**
- Current capacity: `MAX_CONNECTIONS = 16` per signal
- Limit: Connecting a 17th callback returns `-1` with no warning emitted.
- Files: `include/enjin2/core/signal.hpp` line 17
- Scaling path: Return value should be checked by callers; add a debug-mode assertion.

**`LuaEngine` Static Memory Pool is Global and Shared:**
- Current capacity: `MEMORY_LIMIT = 1MB` (desktop) / `64KB` (ESP32)
- Limit: `LuaEngine::memoryPool` and `LuaEngine::memoryUsed` are `static` class members. Instantiating multiple `LuaEngine` objects (or re-initializing) resets the pool (`memoryUsed = 0`) and zeroes shared memory, which would corrupt any other active Lua state.
- Files: `include/enjin2/scripting/lua_engine.hpp` lines 56-57, `src/scripting/lua_engine.cpp` lines 10-11, 25-27
- Scaling path: The static pool design assumes a single `LuaEngine` instance. Enforce this with a singleton guard or document the restriction explicitly.

---

## Dependencies at Risk

**`Arduino.h` Included Unconditionally Inside a `#ifndef VCV_RACK` Guard:**
- Risk: `include/enjin2/graphics/canvas.hpp` includes `<Arduino.h>` when building outside VCV_RACK (line 11). This means any non-VCV_RACK, non-Arduino build (e.g., bare Linux desktop test) will fail to find `Arduino.h` unless the include path is configured. A third target platform (e.g., Emscripten/WASM) would need to either fake `Arduino.h` or add itself to the ifdef ladder.
- Files: `include/enjin2/graphics/canvas.hpp` lines 10-12
- Impact: Limits portability to exactly two platforms (VCV_RACK and Arduino/ESP32).
- Migration plan: Create an `enjin2/platform/arduino_shim.hpp` that can be included from non-Arduino desktop builds, or make the include fully conditional on `defined(ARDUINO)`.

**`lua_platform.hpp` Errors on Unknown Platform:**
- Risk: `#error "Platform not supported for Lua integration"` fires if neither `VCV_RACK` nor `ESP32` is defined (lines 36-37). The Emscripten bindings file (`src/bindings/emscripten_bindings.cpp`) and TypeScript type definitions (`src/bindings/enjin2.d.ts`) indicate a WASM target exists, but the Lua platform layer has no path for it.
- Files: `include/enjin2/scripting/lua_platform.hpp` lines 35-37
- Impact: Emscripten / WASM builds that include the scripting module fail at compile time.
- Migration plan: Add `#elif defined(EMSCRIPTEN)` branch or wrap Lua scripting behind a CMake feature flag for WASM builds.

---

## Missing Critical Features

**No Runtime Error Reporting When Hard Caps Are Exceeded:**
- Problem: `ObjectCollection::addObject`, `Object::addComponent`, `Signal::connect`, and `HandlePool::create` all return `nullptr` or `-1` on exhaustion with no log, assert, or callback. In release firmware builds, these silent failures are impossible to diagnose.
- Blocks: Reliable scene composition at the limits of capacity.
- Files: `include/enjin2/core/object_collection.hpp` line 108, `include/enjin2/core/object.hpp` line 108, `include/enjin2/core/signal.hpp` line 48, `include/enjin2/core/memory.hpp` line 188

**`C_Drawable` Only Draws to `ICanvas<uint8_t>` — No 4-bit Canvas Support:**
- Problem: `C_Drawable::draw()` is declared as `virtual void draw(ICanvas<uint8_t>& canvas) = 0`. The entire drawable/scene pipeline is wired exclusively to 8-bit canvas. `Canvas4` (4-bit, memory-efficient format used on ESP32) cannot be used with the scene/object system.
- Blocks: Using the ECS/scene system on the ESP32 target where `Canvas4` is the primary format.
- Files: `include/enjin2/components/drawable.hpp` line 87, `include/enjin2/core/scene.hpp` lines 319-329

---

## Test Coverage Gaps

**No Unit Tests for Core ECS System:**
- What's not tested: `Object`, `ObjectCollection`, `Scene`, `SceneStateMachine`, `Signal`, component lifecycle (`awake`/`start`/`update`/`lateUpdate`)
- Files: `include/enjin2/core/object.hpp`, `include/enjin2/core/object_collection.hpp`, `include/enjin2/core/scene.hpp`, `include/enjin2/core/signal.hpp`
- Risk: Silent failures in add/remove/find operations, lifecycle ordering bugs, capacity overflows — none are caught automatically.
- Priority: High

**No Unit Tests for Canvas Drawing Primitives:**
- What's not tested: `Canvas4`, `Canvas8`, `CanvasExtended`, pixel packing correctness, fill/clear optimisations, text rendering
- Files: `include/enjin2/graphics/canvas.hpp`, `include/enjin2/graphics/canvas_extended.hpp`
- Risk: The known bugs in `getTextWidth`, `println`, `fill` (vs `clear`) are undetectable without tests. Rendering regressions are invisible.
- Priority: High

**Only Two Test Files Exist — Both Are Integration-Level:**
- What's not tested: All unit-level behavior. `tests/image_comparison.cpp` compares output BMP files. `tests/shadow_mode_test.cpp` is a shadow-mode integration test.
- Files: `tests/image_comparison.cpp`, `tests/shadow_mode_test.cpp`
- Risk: No regression safety for any individual function. All issues must be large enough to affect rendered output to be caught.
- Priority: High

**No Tests for Lua Scripting Layer:**
- What's not tested: `LuaEngine::registerFunction` (including the dangling pointer bug), `LuaCanvas` dispatch, `LuaBindings`, error propagation
- Files: `src/scripting/lua_engine.cpp`, `src/scripting/bindings.cpp`, `include/enjin2/scripting/lua_engine.hpp`
- Risk: Scripting regressions and the known dangling-pointer bug in `registerFunction(LuaCallback)` are undetectable.
- Priority: Medium

---

*Concerns audit: 2026-02-23*
