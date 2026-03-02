# Phase 53: Environment and Build Verification - Research

**Researched:** 2026-03-02
**Domain:** Build infrastructure — shell scripting, CMake, Emscripten emsdk, ESP-IDF v5.5
**Confidence:** HIGH (primary evidence is codebase inspection; toolchain behaviors verified against prior ARCHITECTURE.md research with official-doc citations)

---

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions

**Setup script behavior:**
- Scope: Toolchains only (Emscripten 3.1.73 + ESP-IDF v5.5) — do not install system packages via pacman
- Install location: `~/.local/share/enjin2/` (XDG standard)
- Idempotent: Skip installation if toolchain already present at expected path, print "already installed at X"
- Activation: Install only, then print shell commands the developer needs to add to .bashrc/.zshrc (no auto-modifying shell config)

**Build script design:**
- Replace `build_wasm.sh` entirely — single `build.sh` entry point for all targets
- Build output: `build/<target>/` (e.g., `build/wasm/`, `build/esp32/`, `build/sdl3/`)
- Default target: `sdl3` when `build.sh` is called with no `--target` flag
- Build only — no `--run` flag, no launching binaries. Keep the script focused on compilation.
- Must detect missing `$EMSDK` for WASM target and print actionable error with setup instructions

**ESP32 layer count:**
- Target hardware: ESP32-S3 with PSRAM (8MB)
- Display resolution: 320x240 4-bit (~38KB per layer buffer)
- Layer count: 4 layers (`ENJIN_LAYER_COUNT = 4`)
- Configuration: Hardcoded `#define` in a platform header (not CMake variable) — document PSRAM rationale in code comment alongside the define

### Claude's Discretion

- Exact error message wording for missing toolchains
- Build script internal structure (functions vs linear flow)
- Whether to add a `--clean` flag to build.sh
- ESP32 platform header file location and naming

### Deferred Ideas (OUT OF SCOPE)

None — discussion stayed within phase scope
</user_constraints>

---

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| BLDINFRA-01 | Developer can run a single setup script on Arch Linux to install Emscripten (emsdk 3.1.73) and ESP-IDF (v5.5) toolchains | emsdk install mechanism via `./emsdk install 3.1.73 && ./emsdk activate 3.1.73`; ESP-IDF via git clone + `install.sh esp32s3`; XDG path pattern documented in Architecture Patterns below |
| BLDINFRA-02 | Developer can build for SDL3, WASM, or ESP32 via `build.sh --target [sdl3\|wasm\|esp32]` helper scripts | All three CMake pathways documented in existing CMakeLists.txt; SDL3 uses `ENJIN2_BUILD_SDL=ON`, WASM uses `emcmake cmake -DENJIN2_BUILD_WASM=ON`, ESP32 uses `idf.py build`; unified dispatch shell pattern documented below |
| BLDINFRA-03 | Build scripts detect `$EMSDK` environment variable and fall back gracefully with actionable error | `build_wasm.sh` already has partial detection (checks directory not `$EMSDK` var); new `build.sh` must check `$EMSDK` env var first, then fall back error; exact pattern documented in Code Examples |
| PLAT-01 | All v1.7 features compile under Emscripten and produce `.js` + `.wasm` output | CMakeLists.txt WASM target already configured with correct flags; `enjin2_wasm` produces `enjin2.js` + `enjin2.wasm`; risk area is unverified v1.7 additions (see Open Questions) |
| PLAT-02 | All v1.7 features compile under ESP-IDF and produce flashable firmware | ESP32 IDF example at `examples/esp32_idf_example/` exists with working component registration; compilation status against v1.7 additions is LOW confidence (unverified) |
| PLAT-03 | ESP32 layer count decision documented in code (`ENJIN_LAYER_COUNT` set appropriately for target hardware) | `ENJIN_LAYER_COUNT` currently lives in `include/enjin2/graphics/layer_compositor.hpp` as `constexpr uint8_t ENJIN_LAYER_COUNT = 5`; must be changed to 4 and split into a platform-specific define with PSRAM rationale comment |
</phase_requirements>

---

## Summary

Phase 53 is a build infrastructure and verification phase with no new engine features. The work is split into three tracks: (1) writing `scripts/setup-dev.sh` to install Emscripten 3.1.73 and ESP-IDF v5.5 to XDG paths, (2) writing `build.sh` as a unified entry point replacing `build_wasm.sh`, and (3) verifying and fixing compilation of the full codebase under all three targets (SDL3, WASM, ESP32) — plus adjusting `ENJIN_LAYER_COUNT` to 4 for ESP32-S3 with PSRAM.

The biggest risk is compilation verification: the ARCHITECTURE.md notes explicitly that "Full Emscripten toolchain build not verified" against v1.7 additions, and ESP32 firmware compilation is similarly unverified. The planner must budget task slots for iterative fix cycles, not just one "compile and check" step. The SDL3 build is the most reliable since it runs on the development machine.

The `ENJIN_LAYER_COUNT` change requires surgery on `layer_compositor.hpp`: the current value is 5 (desktop default), but the ESP32-S3 PSRAM decision locks this to 4. The define should live in a new ESP32 platform header (per Claude's Discretion) or directly in `layer_compositor.hpp` behind an `#ifdef ESP32` guard. The `sdl_main.cpp` currently hardcodes 5 LuaCanvas layer wrappers and passes `4` to `setLayers()` — this is consistent with `ENJIN_LAYER_COUNT = 4` for user-facing layers (layer 4 is the debug layer, excluded from the Lua API).

**Primary recommendation:** Implement in three sequential tasks: (1) `setup-dev.sh`, (2) `build.sh`, (3) compile verification + `ENJIN_LAYER_COUNT` fix. The compilation verification task will likely require sub-iterations; do not assume a single compile attempt succeeds.

---

## Standard Stack

### Core

| Tool | Version | Purpose | Why Standard |
|------|---------|---------|--------------|
| emsdk | 3.1.73 (pinned) | Emscripten toolchain management | Official Emscripten version manager; manages `emcc`, `emcmake`, `emmake` |
| ESP-IDF | v5.5 (pinned) | ESP32 toolchain + build system | Espressif's official SDK; provides `idf.py` build command and FreeRTOS |
| CMake | 3.16+ | Build system for SDL3 and WASM targets | Already used in project; SDL3 uses FetchContent, WASM uses emcmake wrapper |
| Bash | System | Setup and build scripts | `set -euo pipefail` discipline for robust scripts |

### Supporting

| Tool | Version | Purpose | When to Use |
|------|---------|---------|-------------|
| `emcmake` / `emmake` | via emsdk | WASM CMake wrapper | WASM target only; wraps CMake configure and make |
| `idf.py` | via ESP-IDF | ESP32 build entry point | ESP32 target only; wraps xtensa-esp32s3-elf-gcc toolchain |
| SDL3 | release-3.4.2 | Desktop window/rendering for SDL3 target | Fetched via CMake FetchContent on first configure |

### Alternatives Considered

| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| XDG `~/.local/share/enjin2/` install path | `~/emsdk` or `../emsdk` | User decision locked to XDG; avoids polluting `$HOME` root and repo parent |
| Pinned emsdk 3.1.73 | `latest` | Pinning ensures reproducible builds; `latest` can break on API changes |
| Pinned ESP-IDF v5.5 | v6.0-beta | v5.5 is stable; out-of-scope rule in REQUIREMENTS.md explicitly rejects v6.0-beta |
| Single `build.sh` with `--target` | separate per-target scripts | User decision locked to unified script |

---

## Architecture Patterns

### Recommended Project Structure

```
scripts/
├── setup-dev.sh      # NEW: toolchain installer (Emscripten + ESP-IDF)
└── ...               # (existing scripts untouched)

build.sh              # NEW: unified build entry point at project root (replaces build_wasm.sh)
build_wasm.sh         # REMOVE or keep for reference — user decision says "replace entirely"

build/
├── wasm/             # WASM build output (enjin2.js, enjin2.wasm)
├── esp32/            # ESP32 build output (firmware .bin files)
└── sdl3/             # SDL3 build output (enjin2_sdl binary)

include/enjin2/graphics/
└── layer_compositor.hpp   # MODIFIED: ENJIN_LAYER_COUNT changed to 4 with ESP32 PSRAM comment
```

The ESP32 platform header location is **Claude's Discretion**. Two valid options:

**Option A (preferred — minimal new files):** Edit `layer_compositor.hpp` directly with an `#ifdef ESP32` guard:
```cpp
#ifdef ESP32
// ESP32-S3 with 8MB PSRAM: 320x240 4-bit = ~38KB per layer buffer.
// 4 layers = ~152KB, comfortably within 8MB PSRAM. Without PSRAM,
// use 2-3 layers to stay within 512KB SRAM.
constexpr uint8_t ENJIN_LAYER_COUNT = 4;
#else
constexpr uint8_t ENJIN_LAYER_COUNT = 5;
#endif
```

**Option B:** New `include/enjin2/platform/esp32_config.hpp` header included by `layer_compositor.hpp`. Adds a file but makes ESP32-specific config findable.

Option A is simpler and follows the existing pattern (see `lua_platform.hpp` and `canvas_esp32s3.hpp` which both use `#ifdef ESP32` inline guards).

### Pattern 1: setup-dev.sh — Idempotent Toolchain Installer

**What:** Installs Emscripten 3.1.73 and ESP-IDF v5.5 to `~/.local/share/enjin2/emsdk` and `~/.local/share/enjin2/esp-idf`. Skips if already present. Prints shell export commands without writing to `.bashrc`.

**When to use:** New developer onboarding on Arch Linux.

**Key constraints from user decisions:**
- No `pacman -S` system package installs
- Print activation commands, do not auto-append to shell config
- Check for presence before cloning

```bash
#!/usr/bin/env bash
set -euo pipefail

ENJIN2_TOOLS="$HOME/.local/share/enjin2"
EMSDK_DIR="$ENJIN2_TOOLS/emsdk"
ESPIDF_DIR="$ENJIN2_TOOLS/esp-idf"

mkdir -p "$ENJIN2_TOOLS"

# Emscripten emsdk
if [ -d "$EMSDK_DIR" ]; then
    echo "Emscripten already installed at $EMSDK_DIR"
else
    git clone https://github.com/emscripten-core/emsdk.git "$EMSDK_DIR"
fi
cd "$EMSDK_DIR"
./emsdk install 3.1.73
./emsdk activate 3.1.73

# ESP-IDF v5.5
if [ -d "$ESPIDF_DIR" ]; then
    echo "ESP-IDF already installed at $ESPIDF_DIR"
else
    git clone --depth 1 --branch v5.5 \
        https://github.com/espressif/esp-idf.git "$ESPIDF_DIR"
fi
cd "$ESPIDF_DIR"
./install.sh esp32s3

# Print activation instructions (do NOT write to shell config)
echo ""
echo "Add the following to your .bashrc or .zshrc:"
echo ""
echo "  source $EMSDK_DIR/emsdk_env.sh"
echo "  source $ESPIDF_DIR/export.sh"
```

**Idempotency:** `[ -d "$EMSDK_DIR" ]` prevents re-clone. `./emsdk install 3.1.73` is itself idempotent (skips if version already installed). `./install.sh esp32s3` is idempotent.

### Pattern 2: build.sh — Unified Build Entry Point

**What:** Single `build.sh` dispatches to per-target build logic based on `--target` flag. Default target is `sdl3`. Detects missing toolchains early with actionable errors.

```bash
#!/usr/bin/env bash
set -euo pipefail

TARGET="sdl3"  # default

while [[ $# -gt 0 ]]; do
    case "$1" in
        --target) TARGET="$2"; shift 2 ;;
        *) echo "Unknown flag: $1"; exit 1 ;;
    esac
done

case "$TARGET" in
    sdl3)
        build_sdl3
        ;;
    wasm)
        check_emsdk   # fails with actionable error if $EMSDK not set
        build_wasm
        ;;
    esp32)
        check_espidf  # fails with actionable error if $IDF_PATH not set
        build_esp32
        ;;
    *)
        echo "Unknown target: $TARGET. Valid targets: sdl3, wasm, esp32"
        exit 1
        ;;
esac
```

**EMSDK detection logic (BLDINFRA-03):**
```bash
check_emsdk() {
    if [ -z "${EMSDK:-}" ]; then
        echo "Error: EMSDK environment variable not set."
        echo ""
        echo "To fix:"
        echo "  1. Run: scripts/setup-dev.sh"
        echo "  2. Then: source ~/.local/share/enjin2/emsdk/emsdk_env.sh"
        echo "  3. Then retry: build.sh --target wasm"
        exit 1
    fi
}
```

**Build output directories:**
```bash
build_wasm() {
    local OUT="$(pwd)/build/wasm"
    mkdir -p "$OUT"
    cd "$OUT"
    emcmake cmake \
        -DENJIN2_BUILD_WASM=ON \
        -DENJIN2_BUILD_LUA=ON \
        -DENJIN2_BUILD_TESTS=OFF \
        -DENJIN2_BUILD_EXAMPLES=OFF \
        -DCMAKE_BUILD_TYPE=Release \
        ../..
    emmake make -j"$(nproc)" enjin2_wasm
}

build_sdl3() {
    local OUT="$(pwd)/build/sdl3"
    mkdir -p "$OUT"
    cmake -B "$OUT" \
        -DENJIN2_BUILD_SDL=ON \
        -DENJIN2_BUILD_LUA=ON \
        -DENJIN2_BUILD_TESTS=OFF \
        -DENJIN2_BUILD_EXAMPLES=OFF \
        -DCMAKE_BUILD_TYPE=Release \
        .
    cmake --build "$OUT" --target enjin2_sdl -j"$(nproc)"
}

build_esp32() {
    local OUT="$(pwd)/build/esp32"
    mkdir -p "$OUT"
    cd examples/esp32_idf_example
    idf.py -B "$OUT" build
}
```

**DROP copy behavior (from CONTEXT.md "Specific Ideas"):**
The existing `build_wasm.sh` copies output to `/home/unwn/dev/DROP/public`. The user decision says to "preserve this behavior or document how to replicate it manually." The recommended approach: print the copy commands at the end of the WASM build if DROP dir doesn't exist, matching the current `build_wasm.sh` behavior exactly.

### Pattern 3: ENJIN_LAYER_COUNT — Current State and Required Change

**Current state:** `include/enjin2/graphics/layer_compositor.hpp` line 14:
```cpp
constexpr uint8_t ENJIN_LAYER_COUNT = 5;
```

**sdl_main.cpp dependency:** The SDL3 runner (`src/platform/sdl/sdl_main.cpp`) creates 5 `LuaCanvas` wrappers (`g_lua_layer0` through `g_lua_layer4`) but passes only 4 to `setLayers()` — layer 4 is the debug layer. This means `sdl_main.cpp` must remain compatible with `ENJIN_LAYER_COUNT = 5` for the SDL3 target.

**Required change:** The user decision says `ENJIN_LAYER_COUNT = 4` for ESP32. This creates a platform split:
- ESP32-S3 with PSRAM: `ENJIN_LAYER_COUNT = 4` (user-locked decision)
- SDL3/WASM: `ENJIN_LAYER_COUNT = 5` (needed for sdl_main.cpp's debug layer architecture)

The `#ifdef ESP32` guard in `layer_compositor.hpp` handles this correctly:
```cpp
#ifdef ESP32
// ESP32-S3 with 8MB PSRAM: 320x240 x 4-bit = ~38KB per layer buffer.
// 4 layers = ~152KB total framebuffer memory. PSRAM (8MB) provides
// ample headroom for layer buffers without touching SRAM (512KB).
// Without PSRAM, reduce to 2-3 layers to stay within SRAM limits.
constexpr uint8_t ENJIN_LAYER_COUNT = 4;
#else
// Desktop and WASM: 5 layers (4 user-facing + 1 debug layer via engine.debug.*)
constexpr uint8_t ENJIN_LAYER_COUNT = 5;
#endif
```

### Anti-Patterns to Avoid

- **Hardcoding the emsdk path:** The existing `build_wasm.sh` hardcodes `$(pwd)/../emsdk`. The new `build.sh` must use `$EMSDK` env var. See Pattern 2 above.
- **Auto-modifying shell config:** The user decision explicitly locks "no auto-modifying shell config." Print activation commands; do not write to `.bashrc`/`.zshrc`.
- **Installing system packages in setup-dev.sh:** User decision explicitly says no `pacman -S`. Document system prerequisites in comments only.
- **Changing ENJIN_LAYER_COUNT globally:** The SDL3 runner depends on 5 layers. Use `#ifdef ESP32` guard; do not change the default value unconditionally.
- **Putting build.sh inside scripts/:** The build entry point goes at the project root (`build.sh`), not in `scripts/`. `setup-dev.sh` goes in `scripts/`.

---

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Emscripten version management | Custom emsdk clone + PATH wrangling in build.sh | `./emsdk install 3.1.73 && ./emsdk activate 3.1.73` then source `emsdk_env.sh` | emsdk handles the `emcc`/`em++`/`emcmake` PATH setup internally |
| ESP32 build system | Custom xtensa-gcc invocations | `idf.py build` | idf.py handles component dependency resolution, flash partitioning, and linker scripts |
| WASM CMake configure | Direct `cmake` invocation with Emscripten flags | `emcmake cmake` | `emcmake` injects the correct Emscripten toolchain file (`-DCMAKE_TOOLCHAIN_FILE=...`) automatically |
| WASM make invocation | Direct `make` | `emmake make` | `emmake` sets `AR`, `CC`, `CXX` etc. to Emscripten equivalents before invoking make |

**Key insight:** The emsdk toolchain wrappers (`emcmake`, `emmake`) exist precisely to avoid environment variable management bugs. Let them do their job — the build script only needs to verify `$EMSDK` is set, then call these wrappers.

---

## Common Pitfalls

### Pitfall 1: emsdk Version Tag Mismatch

**What goes wrong:** `./emsdk install 3.1.73` fails or installs wrong version.
**Why it happens:** emsdk version tags use `3.1.73` format without a `v` prefix. Passing `v3.1.73` causes "SDK not found" error.
**How to avoid:** Use bare version `3.1.73` (no `v` prefix) in both `emsdk install` and `emsdk activate` calls.
**Warning signs:** `emsdk install` prints "No such SDK" or "not found in registry."

### Pitfall 2: EMSDK Variable Not Set vs Directory Not Existing

**What goes wrong:** Build script checks `[ -d "$EMSDK" ]` but `$EMSDK` is empty string — the directory check silently passes (empty string resolves to current directory on some bash versions) or always fails.
**Why it happens:** Conflating "toolchain installed" with "toolchain activated." Installation puts files on disk; activation sets `$EMSDK`. A developer may have run `setup-dev.sh` but not sourced `emsdk_env.sh` in the current shell.
**How to avoid:** Check `[ -z "${EMSDK:-}" ]` for the env var presence. Separately, the setup script itself doesn't need to check `$EMSDK` — it runs from the install directory directly.
**Warning signs:** `emcc: command not found` after installation.

### Pitfall 3: ESP-IDF Branch Tag vs Release Tag

**What goes wrong:** `git clone --branch v5.5` fails because ESP-IDF uses different tag formats for different releases.
**Why it happens:** ESP-IDF uses `v5.5` or `v5.5.0` depending on the point release. The exact tag must be verified.
**How to avoid:** Check `https://github.com/espressif/esp-idf/releases` for the exact tag. For v5.5: the tag is `v5.5` (not `v5.5.0`). Source: ESP-IDF releases page pattern.
**Warning signs:** `git clone` reports "Remote branch v5.5 not found."

**Confidence on this pitfall:** MEDIUM — verified against ESP-IDF release pattern from prior ARCHITECTURE.md research; the exact tag string for v5.5 was not explicitly checked against the live repo at time of research.

### Pitfall 4: WASM Build Output Directory Conflict

**What goes wrong:** `emcmake cmake` inside `build/wasm/` fails because CMake detects a cached configuration from a non-Emscripten configure.
**Why it happens:** If the developer ran a standard `cmake -B build` previously, the `build/` directory has a non-Emscripten CMakeCache. Using `build/wasm/` as the WASM output (a fresh directory) avoids this — but the script must use a directory that has never been configured with a different toolchain.
**How to avoid:** Always use `build/wasm/` as the WASM build directory, never reusing `build/` or `build_wasm/`. The new output convention (`build/<target>/`) enforces this naturally.
**Warning signs:** CMake error "The C compiler identification ... does not match" or "cached CMAKE_TOOLCHAIN_FILE does not match."

### Pitfall 5: ESP32 Compilation Failures on v1.7 Code

**What goes wrong:** Code that compiles fine on SDL3/WASM fails under xtensa-esp32s3-elf-gcc due to missing includes, unsupported C++17 features, or missing `#ifdef ESP32` guards.
**Why it happens:** ESP-IDF uses a more restricted compiler environment. Features like `<filesystem>`, `<thread>`, or C++ exceptions may not be available or may behave differently.
**How to avoid:** Treat compilation verification as an investigation task with multiple fix cycles. When a file fails, add the missing `#ifdef` guard or stub. The existing codebase already has `#ifdef ESP32` patterns in `lua_platform.hpp` and `canvas_esp32s3.hpp` — follow that pattern.
**Warning signs:** Any error referencing `<filesystem>`, `std::filesystem`, `std::thread`, or C++ exceptions in the compilation output.

### Pitfall 6: ENJIN_LAYER_COUNT Change Breaking sdl_main.cpp

**What goes wrong:** If `ENJIN_LAYER_COUNT` is changed to 4 globally (without the `#ifdef ESP32` guard), `sdl_main.cpp` creates 5 `LuaCanvas` wrappers but `LayerCompositor::layers` only has 4 entries — out-of-bounds array access.
**Why it happens:** `sdl_main.cpp` hardcodes 5 layer wrappers and uses `ENJIN_LAYER_COUNT = 5` implicitly.
**How to avoid:** Use `#ifdef ESP32` to keep `ENJIN_LAYER_COUNT = 5` on non-ESP32 platforms. Verify `sdl_main.cpp` still builds after the change.
**Warning signs:** The SDL3 build compiles but crashes immediately on startup, or static analyzer reports array index out of bounds on `g_compositor.layers`.

---

## Code Examples

Verified patterns from codebase inspection:

### Existing build_wasm.sh flow (to refactor into build.sh)

```bash
# Source: /home/unwn/git/enjin/build_wasm.sh (existing)
# This is the pattern to extract for build.sh --target wasm:

EMSDK_DIR="$(pwd)/../emsdk"   # CHANGE TO: use $EMSDK env var from emsdk_env.sh
export PATH="$EMSDK_DIR:$EMSDK_DIR/upstream/emscripten:$PATH"  # NOT NEEDED when $EMSDK is set

emcmake cmake \
    -DENJIN2_BUILD_WASM=ON \
    -DENJIN2_BUILD_LUA=ON \
    -DENJIN2_BUILD_TESTS=OFF \
    -DENJIN2_BUILD_EXAMPLES=OFF \
    -DCMAKE_BUILD_TYPE=Release \
    ..

emmake make -j$(nproc) enjin2_wasm
# Output: enjin2.js and enjin2.wasm (via OUTPUT_NAME "enjin2" SUFFIX ".js" in CMakeLists.txt)
```

### SDL3 build invocation (from CMakeLists.txt analysis)

```bash
# Source: /home/unwn/git/enjin/CMakeLists.txt — ENJIN2_BUILD_SDL option
cmake -B build/sdl3 \
    -DENJIN2_BUILD_SDL=ON \
    -DENJIN2_BUILD_LUA=ON \
    -DENJIN2_BUILD_TESTS=OFF \
    -DENJIN2_BUILD_EXAMPLES=OFF \
    -DCMAKE_BUILD_TYPE=Release \
    .
cmake --build build/sdl3 --target enjin2_sdl -j$(nproc)
```

### Current ENJIN_LAYER_COUNT definition (to be modified)

```cpp
// Source: /home/unwn/git/enjin/include/enjin2/graphics/layer_compositor.hpp, line 14
// Current (before phase 53):
constexpr uint8_t ENJIN_LAYER_COUNT = 5;

// After phase 53 (add ESP32 guard with PSRAM rationale):
#ifdef ESP32
// ESP32-S3 with 8MB PSRAM: 320x240 x 4-bit = ~38KB per layer buffer.
// 4 layers = ~152KB total framebuffer memory. PSRAM (8MB) provides
// ample headroom for layer buffers without touching SRAM (512KB).
// Without PSRAM, reduce to 2-3 layers to stay within SRAM limits.
constexpr uint8_t ENJIN_LAYER_COUNT = 4;
#else
// Desktop (SDL3) and WASM: 5 layers (4 user-facing + 1 debug layer
// accessible only via engine.debug.* bindings in sdl_main.cpp).
constexpr uint8_t ENJIN_LAYER_COUNT = 5;
#endif
static_assert(ENJIN_LAYER_COUNT >= 1 && ENJIN_LAYER_COUNT <= 8,
              "ENJIN_LAYER_COUNT must be between 1 and 8 (inclusive)");
```

### Idempotent directory check pattern

```bash
# Pattern for setup-dev.sh
EMSDK_DIR="$HOME/.local/share/enjin2/emsdk"
if [ -d "$EMSDK_DIR" ]; then
    echo "Emscripten already installed at $EMSDK_DIR"
else
    git clone https://github.com/emscripten-core/emsdk.git "$EMSDK_DIR"
fi
```

### Actionable error for missing EMSDK (BLDINFRA-03)

```bash
# Pattern for build.sh WASM target
check_emsdk() {
    if [ -z "${EMSDK:-}" ]; then
        echo "Error: Emscripten toolchain not activated."
        echo ""
        echo "To set up:"
        echo "  1. Run: bash scripts/setup-dev.sh"
        echo "  2. Activate: source ~/.local/share/enjin2/emsdk/emsdk_env.sh"
        echo "  3. Retry:    bash build.sh --target wasm"
        exit 1
    fi
}
```

---

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|-----------------|--------------|--------|
| `build_wasm.sh` at project root | `build.sh --target wasm` at project root | Phase 53 | Single entry point; `build_wasm.sh` removed |
| Hardcoded `../emsdk` path | `$EMSDK` env var detection | Phase 53 | Works for any installation path |
| `build_wasm` output dir | `build/wasm/` | Phase 53 | Consistent `build/<target>/` convention |
| `ENJIN_LAYER_COUNT = 5` (unconditional) | `ENJIN_LAYER_COUNT = 4` for ESP32, 5 for others | Phase 53 | Platform-appropriate memory usage |

**Deprecated/outdated:**
- `build_wasm.sh`: Replaced by `build.sh --target wasm`. Remove after migration.

---

## Open Questions

1. **Exact ESP-IDF v5.5 git tag**
   - What we know: ESP-IDF v5.5 is the target version. Prior ARCHITECTURE.md used `v5.4.1` as an example.
   - What's unclear: Whether the git tag is `v5.5` or `v5.5.0` (ESP-IDF uses both formats depending on point release).
   - Recommendation: The implementation task must verify the exact tag against `https://github.com/espressif/esp-idf/releases` before writing the setup-dev.sh git clone command. Use `git ls-remote --tags https://github.com/espressif/esp-idf.git | grep v5.5` to check.

2. **v1.7 WASM compilation status**
   - What we know: ARCHITECTURE.md explicitly flags "Full Emscripten toolchain build not verified against v1.7 additions." STATE.md echoes: "WASM build status LOW confidence."
   - What's unclear: Which specific v1.7 files (coroutines, tweens, camera follow, tilemap) may have Emscripten-incompatible code.
   - Recommendation: Treat PLAT-01 as an investigation task. Run the build, capture all errors, then fix each. Plan for 2-4 fix/compile cycles. Files to watch: `bindings_async.cpp`, `bindings_tween.cpp`, `bindings_camera.cpp`, `src/components/tilemap.cpp`.

3. **v1.7 ESP32 compilation status**
   - What we know: The ESP32 IDF example at `examples/esp32_idf_example/` exists but its currency against v1.7 is unknown.
   - What's unclear: Whether the IDF example CMakeLists.txt registers all v1.7 source files (tilemap, camera, async, tween, etc.).
   - Recommendation: Treat PLAT-02 as an investigation task. The IDF example CMakeLists.txt lists only `main/main.cpp` and `esp32_lua_integration.cpp` — it does not register enjin2 sources directly (uses `add_subdirectory`). The enjin2 root CMakeLists.txt static library targets include all sources. Watch for files that use `std::filesystem` or `VCV_RACK`-specific guards without ESP32 fallbacks.

4. **`build.sh --clean` flag**
   - What we know: This is Claude's Discretion (user deferred the decision).
   - What's unclear: Whether a `--clean` flag adds meaningful value for this phase.
   - Recommendation: Add `--clean` as an optional flag that deletes `build/<target>/` before configuring. Implementation cost is low (one `rm -rf` line); recovery value is high (fixes CMake cache corruption from mismatched builds). Include it.

5. **DROP copy behavior in build.sh**
   - What we know: `build_wasm.sh` copies `enjin2.js` and `enjin2.wasm` to `/home/unwn/dev/DROP/public` if that directory exists. CONTEXT.md says "preserve this behavior or document how to replicate it manually."
   - What's unclear: Whether to embed the DROP copy in `build.sh` or just print the commands.
   - Recommendation: Keep the DROP copy behavior in `build.sh --target wasm` (match existing `build_wasm.sh` behavior exactly). If DROP dir doesn't exist, print the manual copy commands. This is a zero-risk regression.

---

## Sources

### Primary (HIGH confidence)

- `/home/unwn/git/enjin/build_wasm.sh` — existing WASM build flow; source of emcmake/emmake pattern and DROP copy behavior
- `/home/unwn/git/enjin/CMakeLists.txt` — all CMake targets, options, and flags for SDL3, WASM, and ESP32; source of `ENJIN2_BUILD_SDL`, `ENJIN2_BUILD_WASM` option names and `enjin2_wasm` target name
- `/home/unwn/git/enjin/include/enjin2/graphics/layer_compositor.hpp` — current `ENJIN_LAYER_COUNT = 5` location and `static_assert` bounds
- `/home/unwn/git/enjin/src/platform/sdl/sdl_main.cpp` — SDL3 runner's dependency on 5 layer wrappers; context for why `ENJIN_LAYER_COUNT` split requires `#ifdef ESP32`
- `/home/unwn/git/enjin/include/enjin2/scripting/lua_platform.hpp` — reference pattern for `#ifdef ESP32` / `#ifdef VCV_RACK` platform guards in this codebase
- `/home/unwn/git/enjin/examples/esp32_idf_example/CMakeLists.txt` — ESP-IDF component registration pattern; shows `add_subdirectory(../../ enjin2)` approach
- `/home/unwn/git/enjin/.planning/research/ARCHITECTURE.md` — verified Emscripten EM_ASM patterns, ESP-IDF NVS limits, setup-dev.sh structure; cites official Emscripten and ESP-IDF docs
- `.planning/phases/53-environment-and-build-verification/53-CONTEXT.md` — locked user decisions

### Secondary (MEDIUM confidence)

- `/home/unwn/git/enjin/.planning/STATE.md` — "WASM build status LOW confidence" blocker note; ESP32 PSRAM unknown flag
- `/home/unwn/git/enjin/include/enjin2/graphics/canvas_esp32s3.hpp` — confirms `Canvas4_320x240_ESP32S3` alias and 320x240 as the ESP32-S3 target resolution (corroborates layer buffer size calculation)

### Tertiary (LOW confidence — flag for validation)

- ESP-IDF v5.5 exact git tag (`v5.5` vs `v5.5.0`): must be verified against live GitHub releases before implementation
- WASM and ESP32 compilation status for v1.7 additions: unverified; treat as investigation until actual build attempted

---

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH — tools are well-established; versions locked in user decisions
- Architecture patterns: HIGH — directly derived from existing codebase code paths; no speculation
- Pitfalls: HIGH for items 1, 2, 4, 6 (code-verified); MEDIUM for item 3 (ESP-IDF tag format); MEDIUM for item 5 (compilation failures — existence confirmed by STATE.md, specific failures unknown)
- Open questions: Accurately flagged; none are blockers for planning (all have a "how to handle" recommendation)

**Research date:** 2026-03-02
**Valid until:** 2026-04-01 (build infrastructure is stable; toolchain version tags don't change)
