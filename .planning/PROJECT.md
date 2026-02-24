# Enjin Migration

## What This Is

enjin2 is a lightweight, statically-allocated 2D graphics engine for embedded devices and WASM. It runs on ESP32, WebAssembly (Emscripten), and SDL3 desktop, with Lua scripting support and a 16-color indexed palette system. enjin2 powers Tomodachi — a portable MIDI/audio control gadget with a pixel display.

## Core Value

enjin2 renders pixel graphics efficiently across embedded and web platforms with zero dynamic allocation.

## Current Milestone: v1.4 Engine Capabilities

**Goal:** Rework the sprite system, add multi-layer canvas composition, fix Docusaurus navigation, and add Lua hot reload to SDL3.

**Target features:**
- Multi-layer composition (4 independent Canvas4 buffers composited at blit)
- Sprites system rework (clean API, sprite sheets, frame animation)
- Docusaurus MDX navigation fix
- Lua hot reload in SDL3 runner (F5 full reset)

## Current State

**Shipped: v1.3 Tomodachi Readiness (2026-02-24)**
- 16-color PICO-8 indexed palette with transparent index 15 and runtime swap
- Platform-agnostic input abstraction (InputState, bitmask, axes, edge detection)
- SDL3 opt-in runner with Canvas4→RGB24 blit, 4× nearest-neighbor scaling, game loop
- Lua input polling API + e2e_parity.lua confirming cross-platform script parity
- Same Lua scripts run unmodified on SDL3, WASM, and ESP32
- 4 phases, 7 plans, 39 files changed, ~67k LOC C++

**Previously shipped: v1.2 Tech Debt Cleanup (2026-02-23)**
- Dead enjin1 compat headers, benchmarks, and CMake references removed
- extractText() rewritten with xml2js ordered parsing for correct document-order traversal
- formatMethod() const const duplication eliminated, 84 API pages regenerated clean
- WASM build made Lua-optional via CMake generator expressions and C++ preprocessor guards
- Generated LaTeX files untracked from git
- 3 phases, 5 plans, 330 files changed, -33k net lines (cleanup)

**Previously shipped: v1.1 Project Infrastructure & Documentation Enhancement (2026-02-23)**
- Professional README with badges, features, and documentation links
- Lua build dependency resolved with CMake options
- 0 Doxygen warnings (down from 372) with CI threshold gate
- 76+ clean API pages across 9 modules with module overviews
- Documentation pipeline fully operational: Doxygen XML → generate-api-docs.js → Docusaurus → GitHub Pages

**Previously shipped: v1.0 Migration + Documentation (2026-02-01)**
- enjin2 fully independent with zero enjin1 dependencies
- 28,271 LOC C++ codebase
- Documentation pipeline: Doxygen + Docusaurus (59 initial API pages)
- All 14 v1 requirements validated

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

### Active

- [ ] LAYER-01–06: Multi-layer Canvas4 composition with Lua API
- [ ] DOC-01–02: Docusaurus API navigation fix
- [ ] SPR-01–06: Sprites system rework (clean API, sprite sheets, frame animation)
- [ ] HOT-01–03: Lua hot reload in SDL3 runner (F5)

### Out of Scope

- [Keeping enjin1] — Target is enjin2-only
- Strangler Fig incremental migration — Pivoted to enjin2-only approach
- Dual-backend compile-time switching — Removed in Phase 5
- Usage examples in API documentation — Deferred to future milestone
- Getting started guide — Deferred to future milestone
- Multi-layer composition — Deferred to v1.4+
- MIDI/audio integration — Tomodachi-side, not enjin2
- SDL2 (legacy) — SDL3 is stable since Jan 2025; SDL2 receives no new features
- Input libraries (Gainput, MPG) — Custom abstraction fits zero-alloc constraint
- Full alpha blending per pixel — Chroma-key transparency (index 15) sufficient for 4-bit canvas

## Context

**After v1.3:**
enjin2 is Tomodachi-ready with:
- 16-color PICO-8 indexed palette (index 15 transparent, runtime swap, no re-render)
- Platform-agnostic input abstraction compiling clean on ESP32, WASM, SDL3
- SDL3 desktop runner as third platform for rapid Lua iteration before ESP32 deployment
- Lua scripts portable across all three platforms without modification
- ~67k LOC C++, CMake multi-target (enjin2_core, enjin2_graphics, enjin2_input, enjin2_lua, enjin2_wasm, enjin2_sdl)

**Known tech debt:**
- `getPaletteRGB()` snapshot semantics (re-invoke after palette mutation; SDL runner unaffected)
- API navigation disabled in Docusaurus due to MDX syntax issues (carried from v1.0)
- Full Emscripten toolchain build not verified in dev environment

## Constraints

- **Structure**: Clean and intelligent organization, no fuss
- **Validation**: Manual testing (no automated test suite)
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

---
*Last updated: 2026-02-24 after v1.4 milestone start*
