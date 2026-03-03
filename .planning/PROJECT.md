# Enjin Migration

## What This Is

enjin2 is a lightweight, statically-allocated 2D graphics engine for embedded devices and WASM. It runs on ESP32, WebAssembly (Emscripten), and SDL3 desktop, with Lua scripting, a 16-color indexed palette system, multi-layer canvas composition, sprite sheets with frame animation, and F5 hot reload for rapid iteration. enjin2 powers Tomodachi — a portable MIDI/audio control gadget with a pixel display.

## Core Value

enjin2 renders pixel graphics efficiently across embedded and web platforms with zero dynamic allocation.

## Current State

**Shipped: v1.9 Tech Debt Resolved (2026-03-03)**
- `const T* getComponent() const` overload added — `hasComponent<T>() const` is now well-formed C++ (DEBT-01)
- `setLuaProxy()` debug-build warning for double-registration (DEBT-02)
- EventBus `m_L=nullptr` window documented at `emit()` with hot-reload ordering invariant (DEBT-03)
- `getPaletteRGB` snapshot semantics documented at WASM binding site (DEBT-04)
- WASM `setInputState()`+`updateFrame()` free functions — closes cross-platform input wiring gap (DEBT-05)
- ESP32 example upgraded to jitter-free FreeRTOS `vTaskDelayUntil` per-frame game loop (DEBT-05)
- 1 phase, 2 plans, 12 files changed

**Previously shipped: v1.0-v1.8** — See MILESTONES.md for full details

## Requirements

### Validated

- ✓ enjin2 fully independent of enjin1 — v1.0 (verified via CMake graphviz, compiler tracking, AST analysis)
- ✓ Compatibility headers created — v1.0 (namespace enjin with type aliases and lifecycle wrappers)
- ✓ Memory mapping documented — v1.0 (shared_ptr to unique_ptr conversion guide)
- ✓ Component lifecycle working — v1.0 (awake/start/update methods)
- ✓ Scene management working — v1.0 (SceneStateMachine with transitions)
- ✓ Manual testing baseline — v1.0 (infrastructure created, user confirmed parity)
- ✓ enjin2 headers compile independently — v1.0 (verified in isolation)
- ✓ enjin2-only build system — v1.0 (all USE_ENJIN1 references removed)
- ✓ BMP export capability — v1.0 (stb_image_write integration)
- ✓ Documentation pipeline — v1.0 (Doxygen + Docusaurus + GitHub Pages)
- ✓ README provides clear project description — v1.1 (Phase 7, RDME-01)
- ✓ Features list highlighting key capabilities — v1.1 (Phase 7, RDME-02)
- ✓ Documentation links to API, guides, GitHub Pages — v1.1 (Phase 7, RDME-03)
- ✓ Lua dependency resolved — v1.1 (Phase 8, BLD-01)
- ✓ Dependencies documented — v1.1 (Phase 8, BLD-02)
- ✓ Doxygen warnings reduced to 0 (target was < 20) — v1.1 (Phase 12, DOC-01)
- ✓ Public APIs documented — v1.1 (Phases 9, 13, 14, DOC-02)
- ✓ Consistent documentation style — v1.1 (Phase 12, DOC-03)
- ✓ Module overviews added — v1.1 (Phases 9, 10, 13, DOC-04)
- ✓ Dead compat headers removed — v1.2 (Phase 16, DEAD-01)
- ✓ Dead benchmark examples removed — v1.2 (Phase 16, DEAD-02/DEAD-03)
- ✓ Dead file references cleaned up — v1.2 (Phase 16, DEAD-04)
- ✓ extractText() filters xml2js attribute objects — v1.2 (Phase 17, DOCG-01)
- ✓ formatMethod() eliminates const const duplication — v1.2 (Phase 17, DOCG-02)
- ✓ All API markdown files regenerated clean — v1.2 (Phase 17, DOCG-03)
- ✓ Cross-reference text renders correctly — v1.2 (Phase 17, DOCG-04)
- ✓ WASM build succeeds with LUA=OFF — v1.2 (Phase 18, BLDS-01)
- ✓ Generated LaTeX files removed from git — v1.2 (Phase 16, REPO-01)
- ✓ .gitignore updated for LaTeX exclusion — v1.2 (Phase 16, REPO-02)
- ✓ Canvas4 palette maps 16 indices to RGB at display time — v1.3 (Phase 19, PAL-01)
- ✓ Index 15 is transparent, indices 0-14 are user colors — v1.3 (Phase 19, PAL-02)
- ✓ Runtime palette swap via setPaletteColor without canvas re-render — v1.3 (Phase 19, PAL-03)
- ✓ Lua API exposes setPalette() and getPalette() for scripts — v1.3 (Phase 19, PAL-04)
- ✓ WASM bindings expose getPaletteRGB() for JavaScript renderer — v1.3 (Phase 19, PAL-05)
- ✓ CMake ENJIN2_BUILD_SDL=ON/OFF option with no impact on WASM or ESP32 builds — v1.3 (Phase 21, SDL-01)
- ✓ SDL3 window with Canvas4-to-RGB texture blit via palette lookup — v1.3 (Phase 21, SDL-02)
- ✓ Integer pixel scaling with nearest-neighbor filtering — v1.3 (Phase 21, SDL-03)
- ✓ Game loop with event polling, delta time, and clean shutdown — v1.3 (Phase 21, SDL-04)
- ✓ Lua scripting works in SDL3 runner (same scripts as WASM/ESP32) — v1.3 (Phase 22, SDL-05)
- ✓ Platform-agnostic input interface with zero platform types in headers — v1.3 (Phase 20, INP-01)
- ✓ InputState with button bitmask and float analog axes — v1.3 (Phase 20, INP-02)
- ✓ Edge detection (justPressed, held, justReleased) in shared layer — v1.3 (Phase 20, INP-03)
- ✓ SDL3 keyboard-to-button default mapping (arrows, Z/X, Enter) — v1.3 (Phase 21, INP-04)
- ✓ Lua input polling API (isButtonHeld, isButtonJustPressed, getAxis) — v1.3 (Phase 22, INP-05)
- ✓ API sidebar navigation renders correctly with all pages accessible — v1.4 (Phase 23, DOC-01)
- ✓ generate-api-docs.js escapes angle brackets for MDX-safe regenerations — v1.4 (Phase 23, DOC-02)
- ✓ Sprite class redesigned with clean zero-alloc API targeting ICanvas<Pixel4> — v1.4 (Phase 24, SPR-01)
- ✓ Sprite sheet loaded as uniform grid with cell dimensions — v1.4 (Phase 24, SPR-02)
- ✓ Frame addressed by linear index or (row, col) position — v1.4 (Phase 24, SPR-03)
- ✓ Frame animation with FPS rate and loop modes (once, loop, ping-pong) — v1.4 (Phase 24, SPR-04)
- ✓ C_Sprite component updated to use new SpriteSheet API — v1.4 (Phase 24, SPR-05)
- ✓ Lua API exposes sprite pool with newSprite/drawSprite/updateSprite/setFrame — v1.4 (Phase 24, SPR-06)
- ✓ Engine renders up to 4 independent Canvas4 layers composited in draw order — v1.4 (Phase 25, LAYER-01)
- ✓ Each drawable assigned to exactly one layer via buffer_index — v1.4 (Phase 25, LAYER-02)
- ✓ Layers composited at blit time using index 15 as passthrough transparency — v1.4 (Phase 25, LAYER-03)
- ✓ Layer count is compile-time configurable (default 4) — v1.4 (Phase 25, LAYER-04)
- ✓ SDL3 runner composites all layers before blitting to GPU texture — v1.4 (Phase 25, LAYER-05)
- ✓ Lua API exposes setLayer/clearLayer/getLayerCount/visibility — v1.4 (Phase 25, LAYER-06)
- ✓ F5 key in SDL3 runner triggers Lua script reload from disk — v1.4 (Phase 26, HOT-01)
- ✓ Reload performs full reset (Lua state destroyed and recreated) — v1.4 (Phase 26, HOT-02)
- ✓ Reload error displays message without crashing the runner — v1.4 (Phase 26, HOT-03)
- ✓ Scene-derived onRender(ICanvas<Pixel4>&) override called during Scene::render() — v1.5 (Phase 27, RENDER-01)
- ✓ float dt in seconds flows through Object, Component, Scene, SceneStateMachine — v1.5 (Phase 28, DT-01)
- ✓ All concrete Component subclasses compile with float dt; -Woverride enforced — v1.5 (Phase 28, DT-02, DT-03)
- ✓ Object name + 8-slot tag array with findByName/findAllWithTag — v1.5 (Phase 29, OBJ-01..04)
- ✓ Scene holds SceneStateMachine* back-pointer; deferred self-transition support — v1.5 (Phase 30, SCENE-01..03)
- ✓ engine.scene/input/time/lua/log Lua global table available before script loads — v1.5 (Phase 31, ENG-03..06)
- ✓ engine.scene.switch(id) reaches SceneStateMachine::switchTo() in live runtime — v1.5 (Phases 31+38, ENG-01)
- ✓ engine.scene.find(name) returns valid ObjectProxy for named Object — v1.5 (Phases 31+37+38, ENG-02)
- ✓ ScriptProxy userdata: self as first callback arg; __index/__newindex dispatch to C++ — v1.5 (Phases 32+33, PROXY-01..04)
- ✓ ScriptErrorPolicy (Disable/Log/Panic) on C_LuaScript with hot-reload reset — v1.5 (Phase 33, ERR-01..05)
- ✓ on_button_pressed/on_button_released fire on edge frames before update() — v1.5 (Phase 34, INPUT-01..03)
- ✓ engine.lua.collect() and engine.lua.memory() exposed to Lua — v1.5 (Phase 35, GC-01, GC-02)
- ✓ assertRequires<T>() on Component: debug assert / release log+disable for missing deps — v1.5 (Phase 35, DEP-01..03)
- ✓ ComponentProxy self:get() — typed proxy userdata with stale-safe invalidation — v1.6 (Phase 39, PROXY-01..04)
- ✓ C_Timer delayed/repeating Lua callbacks (after/every/cancel) — v1.6 (Phase 40, TIMER-01..05)
- ✓ C_StateMachine named states with deferred transitions — v1.6 (Phase 41, FSM-01..05)
- ✓ EventBus scene-scoped pub/sub (on/off/emit) — v1.6 (Phase 42, EVENT-01..05)
- ✓ C_Tilemap 64x64 grid with viewport-culled rendering and Lua proxy — v1.7 (Phase 43, TMAP-01..08)
- ✓ C_Camera with lerp follow, screen shake, bounds, drawWithOffset(), engine.camera.* API — v1.7 (Phase 44, CAM-01..09)
- ✓ Stateless engine.physics.* toolkit with TrigLUT pre-computed trig tables — v1.7 (Phase 45, PHYS-01..13)
- ✓ bindings.cpp split via bindings_internal.hpp, null safety guards — v1.7 (Phase 46, BIND-01, BIND-02)
- ✓ sprite_load_test fixed, overflow tests for event bus/sprite pool/component destruction — v1.7 (Phase 46, TEST-01, TEST-02)
- ✓ engine.debug.* top-layer debug canvas with zero-cost toggle — v1.7 (Phase 47, DEBUG-01..03)
- ✓ engine.camera.follow/stopFollow per-frame tracking — v1.7 (Phase 48, CAM-01, CAM-02)
- ✓ LuaStore SDL3 JSON I/O with engine.store.flush/path — v1.7 (Phase 48, STORE-01, STORE-02)
- ✓ engine.async.* 8-slot coroutine scheduler with wait/cancel, ESP32 library — v1.7 (Phase 49, ASYNC-01..04)
- ✓ engine.tween.* 8-slot pool with 4 inline easing functions — v1.7 (Phase 50, TWEEN-01..03)
- ✓ engine.scene.persist/unpersist with PersistentObjectRegistry — v1.7 (Phase 51, PERSIST-01..03)
- ✓ engine.ui.* stateless draw calls + internal component guide — v1.7 (Phase 52, UI-01..05)
- ✓ setup-dev.sh installs Emscripten 3.1.73 + ESP-IDF v5.5 to XDG paths — v1.8 (Phase 53, BLDINFRA-01)
- ✓ build.sh unified script for SDL3/WASM/ESP32 with $EMSDK graceful fallback — v1.8 (Phase 53, BLDINFRA-02/03)
- ✓ All v1.7 features compile under Emscripten (.js+.wasm output) and ESP-IDF (flashable firmware) — v1.8 (Phase 53, PLAT-01/02)
- ✓ ENJIN_LAYER_COUNT set and documented with PSRAM rationale for ESP32 target — v1.8 (Phase 53, PLAT-03)
- ✓ LuaStore::writeStoreToBuffer() allocation-free JSON serializer shared across all platforms — v1.8 (Phase 54, STORE-01)
- ✓ engine.store WASM localStorage bridge — flush-only persistence across page reloads — v1.8 (Phase 55, STORE-02)
- ✓ engine.store ESP32 NVS backend — persist across power cycles, nvs_commit() — v1.8 (Phase 55, STORE-03)
- ✓ NVS key validation rejects keys > 15 chars with Lua error — v1.8 (Phase 55, STORE-04)
- ✓ m_followTargetProxy cleared on scene change and hot reload — v1.8 (Phase 56, DEBT-01)
- ✓ engine.scene.persist() emits printf warning without SceneStateMachine context — v1.8 (Phase 56, DEBT-02)
- ✓ engine.tween.await(id) suspends coroutine until tween completes — v1.8 (Phase 57, QOL-01)
- ✓ engine.async.wait_frames(n) yields coroutine for exactly n frames — v1.8 (Phase 57, QOL-02)
- ✓ engine.camera.setDeadZone(w, h) with centered dead zone boundary — v1.8 (Phase 57, QOL-03)
- ✓ Getting Started guide updated for SDL3 runner API, no stale Canvas8 references — v1.8 (Phase 58, DOC-01)
- ✓ "Your First Script" Docusaurus tutorial with tamagotchi.lua walkthrough — v1.8 (Phase 58, DOC-02)
- ✓ "Async Coroutines" tutorial covering engine.async + engine.tween.await — v1.8 (Phase 58, DOC-03)
- ✓ Lua syntax highlighting enabled in Docusaurus (prism additionalLanguages) — v1.8 (Phase 58, DOC-04)

### Active

### Out of Scope

- [Keeping enjin1] — Target is enjin2-only
- Strangler Fig incremental migration — Pivoted to enjin2-only approach
- Dual-backend compile-time switching — Removed in Phase 5
- Usage examples in API documentation — Deferred to future milestone
- Getting started guide — Deferred to future milestone
- MIDI/audio integration — Tomodachi-side, not enjin2
- SDL2 (legacy) — SDL3 is stable since Jan 2025; SDL2 receives no new features
- Input libraries (Gainput, MPG) — Custom abstraction fits zero-alloc constraint
- Per-pixel alpha blending — Incompatible with 4-bit indexed palette
- Dynamic layer count at runtime — Violates zero-alloc constraint
- Sprite rotation at blit time — No FPU on ESP32; pre-rotate frames in sheet
- Non-uniform sprite sheet frames — Requires per-frame metadata, breaks grid math simplicity
- File-watch auto-reload — Platform-specific OS APIs; F5 manual reload is sufficient
- Partial Lua state hot-patch — Produces dangling references; full reset is correct semantic
- WASM/ESP32 hot reload — Developer tool for SDL3 runner only
- Integer layer system — Deferred to future (independent change)
- C_LuaScript::setInput() in WASM/ESP32 host paths — SDL runner done; platform wiring deferred
- Event bus data payload — Basic on/emit sufficient for target games
- C++ Signal<T> component signals — Lua event bus covers inter-object communication needs

## Context

**After v1.8:**
enjin2 is deployable on all 3 targets with a complete dev toolchain, persistent storage, and onboarding docs:
- Toolchain: setup-dev.sh (Emscripten 3.1.73 + ESP-IDF v5.5), build.sh unified entry point, all 3 platforms verified
- Storage: LuaStore works on SDL3 (JSON file), WASM (localStorage), and ESP32 (NVS blob) with shared serializer
- engine.* Lua sub-tables: scene, input, time, camera, physics, debug, async, tween, ui, store, event (12 sub-tables)
- QoL coroutine additions: tween.await(), wait_frames(), camera dead zone
- Documentation: Getting Started (SDL3), "Your First Script" tutorial, "Async Coroutines" tutorial, Lua highlighting
- ~61,700 LOC C++/hpp across 130+ source files, CMake multi-target
- ctests: 148 assertions in qol_test alone; all suites passing

**Known tech debt:**
- Single-proxy-per-component constraint: multiple self:get() calls overwrite proxy registration (last wins; debug warning fires in Phase 59)
- ESP32 LuaEngine → LuaScriptSystem migration: `setInput()` commented out in example pending migration (wiring pattern documented)

## Constraints

- **Structure**: Clean and intelligent organization, no fuss
- **Validation**: Manual testing + targeted unit tests (input, palette, sprite, compositor)
- **Memory**: No dynamic allocation (static arrays, no heap)
- **Platforms**: Must work on ESP32, WASM, and SDL3 desktop

## Key Decisions

| Decision | Rationale | Outcome |
|----------|-----------|---------|
| Fully independent enjin2 | User wants to keep only enjin2 in the end | ✓ Complete - v1.0 |
| Namespace enjin for compatibility | Separates migration code from enjin2 core | ✓ Implemented - kept as artifact |
| Compile-time backend selection | Allow gradual migration | Removed - pivot to enjin2-only |
| xml2js for Doxygen XML parsing | Handles C++ templates, namespaces, overloads | ✓ Working - Phase 6 |
| Module-based API organization | Better navigation than alphabetical A-Z | ✓ Working - Phase 6 |
| Docusaurus dual-plugin setup | Separate guides and API reference | ✓ Working - Phase 6 |
| Optional Lua via CMake | find_package(Lua QUIET) allows ENJIN2_BUILD_LUA=OFF | ✓ Working - Phase 8 |
| Essential-level doc standard | @brief, @param, @return only — no verbose descriptions | ✓ Good - achieves 0 warnings |
| CI Doxygen warning gate | Prevents regression above 20 warnings | ✓ Working - Phase 11 |
| classNameToXmlFilename encoding | Encodes underscores before _1_1 join for Doxygen XML | ✓ Working - Phase 13 |
| extractText() $ filter | Skip xml2js attribute objects in text extraction | ✓ Working - Phase 14 |
| xml2js ordered parsing | explicitChildren + preserveChildrenOrder + charsAsChildren for correct mixed-content traversal | ✓ Working - Phase 17 |
| extractText() $$ array traversal | Object.entries() loses document order for mixed-content nodes | ✓ Working - Phase 17 |
| formatMethod() const dedup | Strip trailing ' const' from argsstring when $.const=yes | ✓ Working - Phase 17 |
| CMake generator expressions for WASM Lua | $<$<BOOL:${ENJIN2_BUILD_LUA}>:...> consistent with existing target pattern | ✓ Working - Phase 18 |
| ENJIN2_BUILD_LUA compile definition | CMake injects ENJIN2_BUILD_LUA=1 so C++ preprocessor gates Lua code | ✓ Working - Phase 18 |
| Index 15 = transparent, 0-14 user colors | Preserves Colors::BLACK = Pixel4(0); transparent as highest index | ✓ Working - Phase 19 |
| SDL3 (not SDL2) for desktop runner | SDL3 stable since Jan 2025; SDL2 receives no new features | ✓ Working - Phase 21 |
| WASM palette bindings outside Lua guard | Palette is core graphics, not Lua-only | ✓ Working - Phase 19 |
| getPaletteRGB uses static buffer | typed_memory_view for zero-copy; snapshot semantics documented | ✓ Working - Phase 19 |
| input_platform_poll declared not defined in core | Each platform provides exactly one definition | ✓ Working - Phase 20 |
| InputState uses uint16_t bitmask + float axes[8] | Matches INP-02 spec, no heap | ✓ Working - Phase 20 |
| SDL_SetRenderScale(4,4) instead of logical presentation | Workaround for SDL3 bug #11335 (logical presentation ignores SCALEMODE_NEAREST) | ✓ Working - Phase 21 |
| input_advance_frame before input_platform_poll each frame | advance clears current, poll writes new state — correct frame sequence | ✓ Working - Phase 21 |
| InputState* initialized to nullptr in LuaBindings | Null guard in all input bindings prevents crash before setInput() call | ✓ Working - Phase 22 |
| lua_type(L,1)==LUA_TSTRING for string detection | lua_isstring is too permissive (numbers coerce to strings) | ✓ Working - Phase 22 |
| enjin2_sdl conditional Lua link via generator expressions | Zero impact on non-Lua builds; consistent with Phase 18 pattern | ✓ Working - Phase 22 |
| escapeForMdx at extraction time in generate-api-docs.js | All downstream uses of prose fields are automatically safe | ✓ Working - Phase 23 |
| SpriteSheet inline draw() in header (no .cpp) | Matches codebase header-only pattern for simple structs | ✓ Working - Phase 24 |
| Compile-time transparency index 15 in SpriteSheet::draw() | No matte parameter; consistent with LayerCompositor | ✓ Working - Phase 24 |
| C_Drawable::draw() signature ICanvas<Pixel4>& (not uint8_t) | Enables 4-bit palette pipeline; C_Canvas draw() stubbed for ENG-01 | ✓ Working - Phase 24 |
| lua_drawSprite via LuaCanvas::setPixel (type-erased) | Avoids ICanvas<Pixel4> cast in binding; works with any canvas type | ✓ Working - Phase 24 |
| DrawLayer enum deleted; uint8_t buffer_index direct slot | Removes abstraction layer causing legacy naming bugs | ✓ Working - Phase 25 |
| LayerCompositor composite() uses getBufferSize() not BUFFER_SIZE | BUFFER_SIZE is private on Canvas4; public accessor equivalent | ✓ Working - Phase 25 |
| setLayers() replaces setCanvas() in SDL3 runner | Sets currentCanvas=layers[0] internally; single call wires all layers | ✓ Working - Phase 25 |
| Lua layer indices 1-indexed; cpp_idx = lua_idx - 1 | Lua convention; out-of-range clamped to [0, layerCount-1] | ✓ Working - Phase 25 |
| LuaCallback overload neutered to no-op (not removed) | Preserves ABI; all bindings use lua_CFunction exclusively | ✓ Good - Phase 26 |
| performReload() encapsulates full Lua lifecycle | shutdown+initialize+setLayers+setInput+loadScript; initial and F5 share path | ✓ Working - Phase 26 |
| lua_ok gate pattern for error recovery | false=paused (error state), F5 retries; runtime errors also set false | ✓ Working - Phase 26 |
| if constexpr two-branch for Pixel4/uint8_t onRender | Single onRender(canvas) without guards fails overload resolution at compile time | ✓ Working - Phase 27 |
| float dt in seconds at source (no /1000) | Eliminates all conversion sites; accumulator arithmetic is natural | ✓ Working - Phase 28 |
| const char* name/tag with no allocation | Matches zero-alloc constraint; string literals have static lifetime | ✓ Working - Phase 29 |
| Deferred scene transitions (last-wins queue) | Prevents re-entrant SSM corruption during onUpdate() | ✓ Working - Phase 30 |
| engine.* pointers via pointer-to-pointer registry | Post-registerAll injection survives registry snap; no re-registerAll needed | ✓ Working - Phases 31+38 |
| ScriptProxy as full userdata (not lightuserdata) | Metatable metamethods require full userdata; lightuserdata has no per-object metatable | ✓ Working - Phase 32 |
| callWithProxy() for all Lua callbacks | Single dispatch path for init/update/draw/callbacks; proxy-first convention | ✓ Working - Phase 32 |
| ScriptErrorPolicy enum (Disable/Log/Panic) | Cleanly separates concerns; Disable is safe default for embedded targets | ✓ Working - Phase 33 |
| dispatchInputCallbacks() before update() each frame | Callbacks precede update; no stale input; frame-correct semantics | ✓ Working - Phase 34 |
| engine.lua.collect() incremental (not full GC) | Prevents mid-frame spike on embedded targets with small heaps | ✓ Working - Phase 35 |
| assertRequires<T>() on Component base class | Single declaration site; debug abort / release log+disable pattern | ✓ Working - Phase 35 |
| getComponents<T>() via dynamic_cast (no cache) | Zero-allocation; correct for scene.hpp/animation.hpp call sites | ✓ Working - Phase 36 |
| ObjectProxy invalidated in Object destructor hook | Prevents dangling pointer access; luaL_error on stale proxy | ✓ Working - Phase 37 |
| stale ScriptProxy raises luaL_error (not nil) | Silent nil produces confusing bugs; explicit error directs author to fix | ✓ Good - Phase 37 |
| loadScriptFile() creates ScriptProxy before callWithProxy | init(self) for file-loaded scripts matches loadScript() string path | ✓ Working - Phase 38 |
| ComponentProxy standalone header (mirrors ObjectProxy) | Avoids circular includes between component.hpp and bindings.hpp | ✓ Working - Phase 39 |
| self:get() checked FIRST in ScriptProxy.__index | Prevents name collision with future property names | ✓ Working - Phase 39 |
| fireCallback(cbRef) takes ref as parameter | One-shot timers set callbackRef=LUA_NOREF before pcall (re-entrancy safe) | ✓ Working - Phase 40 |
| clearTimers() sets m_L=nullptr sentinel | Prevents double-unref in C_Timer destructor | ✓ Working - Phase 40 |
| C_LuaScript destructor calls C_Timer::clearTimers() before shutdown() | Handles component array destruction order (C_LuaScript[0] before C_Timer[1]) | ✓ Working - Phase 40 |
| C_StateMachine deferred transitions (setState queues m_pendingState) | Applied at END of update() matching SceneStateMachine pattern | ✓ Working - Phase 41 |
| LuaEventBus fixed-capacity arrays (Channel[16], Subscriber[8]) | Zero heap allocation; same sentinel pattern (m_L=nullptr after clearHandlers) | ✓ Working - Phase 42 |
| emit() snapshots refs to local array before pcall loop | Re-entrant safety: off()/on() inside handler doesn't corrupt iteration | ✓ Working - Phase 42 |
| EVENT-05 hot-reload in executeScript() not registerAll() | registerAll() runs once at initialize(); hot-reload goes through executeScript() | ✓ Working - Phase 42 |

| Tile ID 0 transparent sentinel with frameIndex pass-through | No subtract-1 in hot path; tileset frame 0 is wasted | ✓ Working - Phase 43 |
| C_Drawable::drawWithOffset() virtual + m_screenSpace flag | Camera offset skipped for UI elements | ✓ Working - Phase 44 |
| TrigLUT 256-entry precomputed sine table (not std::sin) | ESP32 has no FPU; LUT is O(1) lookup | ✓ Working - Phase 45 |
| bindings_internal.hpp static constexpr metatable constants | TU-local, ODR-safe, no companion .cpp needed | ✓ Working - Phase 46 |
| ENJIN_LAYER_COUNT=5 with layer 4 reserved for debug | Debug canvas excluded from Lua setLayer(); only engine.debug.* writes to it | ✓ Working - Phase 47 |
| tickCameraFollow/tickCoroutines/tickTweens order in SDL runner | Camera updates before coroutines; coroutines before tweens | ✓ Working - Phases 48-50 |
| Coroutine scheduler resumes via lua_resume outside pcall scope | Avoids yield-across-pcall boundary | ✓ Working - Phase 49 |
| TweenEasing as private enum cast to uint8_t | File-scope tweenEase() in separate TU avoids private-access error | ✓ Working - Phase 50 |
| PersistentObjectRegistry owned by SceneStateMachine (not LuaBindings) | Object ownership is C++ level; SSM owns lifecycle | ✓ Working - Phase 51 |
| engine.ui.* bypasses C++ Label/FillUpGauge entirely | std::string incompatible with zero-alloc Pixel4 pipeline | ✓ Working - Phase 52 |
| writeStoreToBuffer placed before #if !defined(ESP32) guard | Shared JSON serializer available on all 3 platforms | ✓ Working - Phase 54 |
| wasm_storage.cpp holds only EM_JS declarations | loadFromFile needs access to static JSON parser in bindings_store.cpp | ✓ Working - Phase 55 |
| flush() WASM/ESP32 branch calls saveToFile(nullptr) | Bypasses desktop empty-path guard; correct for platform paths | ✓ Working - Phase 55 |
| m_followTargetProxy clear inside if (scene != m_activeScene) guard | Only clear on actual scene change, not on every registerAll() call | ✓ Working - Phase 56 |
| printf() for persist() warning (not lua_warning, not stderr) | Matches project-wide diagnostic pattern across all platforms | ✓ Working - Phase 56 |
| waitTweenId gate in tickCoroutines skips frame/time check | Resume fires from tickTweens — avoids double-resume race | ✓ Working - Phase 57 |
| Coroutine resume in tickTweens placed before done_cb pcall | Avoids yield-across-pcall boundary error | ✓ Working - Phase 57 |
| Dead zone rectangle centered on camera position (not target) | Classic platformer feel; target re-enters zone from center outward | ✓ Working - Phase 57 |
| printf() for setLuaProxy debug warning (not fprintf/stderr) | Matches existing component.hpp style in release branch | ✓ Working - Phase 59 |
| setLuaProxy warning condition: old && new && old != new | Clearing via nullptr and idempotent re-registration are both silent | ✓ Working - Phase 59 |
| setInputState/updateFrame as free functions in WASM bindings | JS host owns LuaScriptSystem; free functions match existing WASM architecture | ✓ Working - Phase 59 |
| s_wasm_input as function-local static in WASM lambda | Zero heap allocation; persists across calls; safe for single-instance WASM | ✓ Working - Phase 59 |
| ESP32 example stays on LuaEngine (not migrated to LuaScriptSystem) | Example purpose is wiring pattern; migration documented in commented setInput call | — Pending |

---
*Last updated: 2026-03-03 after v1.9 milestone*
