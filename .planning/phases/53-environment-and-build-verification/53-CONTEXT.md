# Phase 53: Environment and Build Verification - Context

**Gathered:** 2026-03-02
**Status:** Ready for planning

<domain>
## Phase Boundary

Developer can set up the full toolchain and verify all three platform builds (SDL3, WASM, ESP32) succeed. Includes a dev setup script, unified build script, and ESP32 layer count configuration. No runtime features — strictly build infrastructure and compilation verification.

</domain>

<decisions>
## Implementation Decisions

### Setup script behavior
- Scope: Toolchains only (Emscripten 3.1.73 + ESP-IDF v5.5) — do not install system packages via pacman
- Install location: `~/.local/share/enjin2/` (XDG standard)
- Idempotent: Skip installation if toolchain already present at expected path, print "already installed at X"
- Activation: Install only, then print shell commands the developer needs to add to .bashrc/.zshrc (no auto-modifying shell config)

### Build script design
- Replace `build_wasm.sh` entirely — single `build.sh` entry point for all targets
- Build output: `build/<target>/` (e.g., `build/wasm/`, `build/esp32/`, `build/sdl3/`)
- Default target: `sdl3` when `build.sh` is called with no `--target` flag
- Build only — no `--run` flag, no launching binaries. Keep the script focused on compilation.
- Must detect missing `$EMSDK` for WASM target and print actionable error with setup instructions

### ESP32 layer count
- Target hardware: ESP32-S3 with PSRAM (8MB)
- Display resolution: 320x240 4-bit (~38KB per layer buffer)
- Layer count: 4 layers (`ENJIN_LAYER_COUNT = 4`)
- Configuration: Hardcoded `#define` in a platform header (not CMake variable) — document PSRAM rationale in code comment alongside the define

### Claude's Discretion
- Exact error message wording for missing toolchains
- Build script internal structure (functions vs linear flow)
- Whether to add a `--clean` flag to build.sh
- ESP32 platform header file location and naming

</decisions>

<specifics>
## Specific Ideas

- Existing `build_wasm.sh` copies output to a DROP project directory — preserve this behavior or document how to replicate it manually with the new build.sh
- ESP-IDF example at `examples/esp32_idf_example/` should still work with the new build flow

</specifics>

<code_context>
## Existing Code Insights

### Reusable Assets
- `build_wasm.sh`: Contains working Emscripten build flow (emcmake cmake + emmake make), can be refactored into build.sh
- `examples/esp32_idf_example/CMakeLists.txt`: Working ESP-IDF component registration with enjin2 libraries
- `CMakeLists.txt`: Already has `ENJIN2_BUILD_WASM`, `ENJIN2_BUILD_SDL` options, modular library targets

### Established Patterns
- CMake-based build system with modular library targets (enjin2_core, enjin2_graphics, enjin2_ui, enjin2_lua)
- Platform defines: `VCV_RACK` (desktop), `ESP32` (embedded), `EMSCRIPTEN` (web)
- EMSDK path convention: Currently hardcoded to `../emsdk` in build_wasm.sh — will change to `~/.local/share/enjin2/emsdk`

### Integration Points
- `CMakeLists.txt` root: Where build options and platform defines are set
- `include/enjin2/scripting/lua_platform.hpp`: Platform-specific Lua config — similar pattern for layer count config
- `.gitignore`: Needs `build/` entries for new build directory structure

</code_context>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope

</deferred>

---

*Phase: 53-environment-and-build-verification*
*Context gathered: 2026-03-02*
