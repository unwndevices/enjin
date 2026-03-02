# Architecture Research

**Domain:** 2D game engine — v1.8 platform hardening and developer onboarding
**Researched:** 2026-03-02
**Confidence:** HIGH (codebase read + official Emscripten/ESP-IDF/Docusaurus docs verified)

---

## Standard Architecture

### System Overview

The v1.8 milestone adds platform backend implementations and onboarding tooling on top of the existing architecture. No new engine subsystems are added. The work is concentrated in four areas: LuaStore backends, build infrastructure, and documentation.

```
┌──────────────────────────────────────────────────────────────────────┐
│                         Docusaurus Site                               │
│   docs/src/guides/ (existing)   docs/src/tutorials/ (NEW)            │
│   docs/api/ (existing, Doxygen-generated)                            │
├──────────────────────────────────────────────────────────────────────┤
│                         Build Infrastructure                          │
│   build_wasm.sh (existing, has bugs)     scripts/setup-dev.sh (NEW)  │
│   CMakeLists.txt (WASM flags)            scripts/build-helpers.sh    │
├──────────────────────────────────────────────────────────────────────┤
│                           LuaBindings                                 │
│   m_store : LuaStore          m_storePath[256]                       │
│                                                                       │
│   bindings_store.cpp                                                  │
│   ┌─────────────────────────────────────────────────────────────┐    │
│   │                Platform Guard (current: broken)             │    │
│   │  #if !defined(ESP32) && !defined(__EMSCRIPTEN__)            │    │
│   │    std::fstream JSON I/O  ← SDL3 desktop (WORKING)         │    │
│   │  #else                                                       │    │
│   │    return false;          ← ESP32 stub  (NEEDS NVS)         │    │
│   │    return false;          ← WASM stub   (NEEDS localStorage) │    │
│   │  #endif                                                      │    │
│   └─────────────────────────────────────────────────────────────┘    │
├──────────────────────────────────────────────────────────────────────┤
│         Target Platform Backends (what v1.8 implements)               │
│  ┌──────────────┐  ┌──────────────────────┐  ┌───────────────────┐  │
│  │ SDL3 Desktop │  │  WASM / Emscripten   │  │      ESP32        │  │
│  │ std::fstream │  │  EM_ASM localStorage │  │  nvs_flash NVS   │  │
│  │  (existing)  │  │    (NEW v1.8)        │  │   (NEW v1.8)      │  │
│  └──────────────┘  └──────────────────────┘  └───────────────────┘  │
└──────────────────────────────────────────────────────────────────────┘
```

### Component Responsibilities

| Component / Module | Responsibility | v1.8 Change |
|--------------------|---------------|-------------|
| `LuaStore` | Fixed-capacity 16-key KV store; in-memory only | No change — API is stable |
| `LuaStore::saveToFile()` / `loadFromFile()` | Platform I/O backends called by Lua bindings | Replace the stub `#else return false` with real ESP32 NVS and WASM localStorage implementations |
| `bindings_store.cpp` | Platform dispatch via preprocessor guards | Add `#elif defined(__EMSCRIPTEN__)` and `#elif defined(ESP32)` branches |
| `pre.js` | WASM module preamble; module init hooks | Add `FS.mkdir + FS.mount(IDBFS)` and initial `FS.syncfs(true, cb)` call (or use `addRunDependency` pattern) |
| `CMakeLists.txt` WASM target | Linker flags for enjin2_wasm | Add `-lidbfs.js` linker flag; verify existing flags compile cleanly with current emsdk |
| `scripts/setup-dev.sh` | New: installs emsdk + ESP-IDF on Arch Linux | New file; must be idempotent |
| `scripts/build-helpers.sh` | New: wrappers for common build commands | New file |
| `docs/src/tutorials/` | New Docusaurus tutorial content | New directory + MDX files |
| `docs/sidebars.js` | Sidebar navigation config | Add `tutorials` category under `guidesSidebar` |

---

## Recommended Project Structure

### LuaStore Backend Split

The existing `bindings_store.cpp` uses a single `#if !defined(ESP32) && !defined(__EMSCRIPTEN__)` guard. v1.8 keeps this structure but fills in the stub branches.

```
src/scripting/bindings_store.cpp   (MODIFIED — no new files needed)

Platform dispatch structure (after v1.8):
  #if !defined(ESP32) && !defined(__EMSCRIPTEN__)
    // SDL3 desktop — existing std::fstream JSON I/O (unchanged)
    bool LuaStore::saveToFile(path) { ... fstream ... }
    bool LuaStore::loadFromFile(path) { ... fstream ... }

  #elif defined(__EMSCRIPTEN__)
    // WASM — EM_ASM localStorage bridge (NEW)
    bool LuaStore::saveToFile(path) {
        // Serialize m_entries[] to compact JSON string in C++ stack buffer
        // EM_ASM({ localStorage.setItem(UTF8ToString($0), UTF8ToString($1)); }, key, json);
        return true;
    }
    bool LuaStore::loadFromFile(path) {
        // EM_JS to read localStorage, copy to stack buffer, parse with existing readJson*
    }

  #elif defined(ESP32)
    // ESP32 — NVS (NEW)
    // nvs_open("enjin_store", NVS_READWRITE, &handle)
    // Per key: nvs_set_str / nvs_get_str for string encoding of slot data
    // nvs_commit() on write
    // nvs_close(handle)
  #endif
```

### Script Infrastructure

```
scripts/
├── generate-api-docs.js     (existing)
├── setup-dev.sh             (NEW — Arch Linux dev environment)
├── build-wasm.sh            (rename/replace existing build_wasm.sh at root)
├── build-sdl.sh             (NEW — SDL3 desktop build helper)
└── build-esp32.sh           (NEW — ESP32 IDF build helper; stubs idf.py calls)

(existing build_wasm.sh at project root may be replaced or kept and updated)
```

### Docusaurus Tutorial Structure

```
docs/src/
├── intro.md                 (existing)
├── getting-started.md       (existing — may need update)
├── architecture.md          (existing)
├── components.md            (existing)
├── canvas.md                (existing)
├── sprites.md               (existing)
├── text-rendering.md        (existing)
├── scene-management.md      (existing)
├── scene-transitions.md     (existing)
└── tutorials/               (NEW directory)
    ├── _category_.json      (NEW — label: "Tutorials", position: 2)
    ├── 01-getting-started.md (NEW)
    ├── 02-arkanoid.md        (NEW — tutorial built on arkanoid.lua)
    └── 03-tamagotchi.md      (NEW — tutorial built on tamagotchi.lua)

docs/sidebars.js             (MODIFIED — add tutorials category)
```

The existing `guidesSidebar` in `sidebars.js` uses a manual array. Adding tutorials requires one new `category` entry:

```javascript
// In docs/sidebars.js, inside guidesSidebar array:
{
  type: 'category',
  label: 'Tutorials',
  items: [
    'tutorials/01-getting-started',
    'tutorials/02-arkanoid',
    'tutorials/03-tamagotchi',
  ],
},
```

Alternatively, switch the tutorials directory to `type: 'autogenerated'` with `_category_.json` front matter and `sidebar_position` fields on each file. Either approach works; the manual array is simpler given the existing sidebars.js pattern.

---

## Architectural Patterns

### Pattern 1: WASM localStorage Bridge via EM_ASM

**What:** The `saveToFile` and `loadFromFile` functions serialize the in-memory `LuaStore` to/from a single localStorage key under `#elif defined(__EMSCRIPTEN__)`. The "path" parameter becomes the localStorage key name.

**When to use:** Only inside `LuaStore::saveToFile()` and `loadFromFile()` under the WASM guard.

**Trade-offs:**
- Simple: no IDBFS mount/sync lifecycle needed since localStorage is synchronous.
- localStorage has a 5–10MB quota per origin; the LuaStore is tiny (16 keys, 64-char strings max) so this is irrelevant.
- localStorage is main-thread synchronous, matching the existing C++ call pattern.
- EM_JS requires `-sEXPORTED_RUNTIME_METHODS` to include `UTF8ToString`; the existing CMake flags already include the Emscripten runtime, but this must be verified.
- IDBFS is an alternative but requires async sync with `FS.syncfs()` which conflicts with the synchronous `saveToFile` signature. localStorage is the right choice here.

**Example:**
```cpp
#elif defined(__EMSCRIPTEN__)
#include <emscripten.h>

// Serialize the store to a compact JSON string (reuse the existing
// stack-based writer logic or write to a static char buffer)
static char s_jsonBuf[4096];  // static: zero-alloc, single-threaded WASM is fine

bool LuaStore::saveToFile(const char* key) const {
    // Write JSON into s_jsonBuf using existing write logic
    // (or a new minimal serializer that writes to a char* instead of ofstream)
    writeToBuffer(s_jsonBuf, sizeof(s_jsonBuf));
    EM_ASM({
        var key = UTF8ToString($0);
        var val = UTF8ToString($1);
        try { localStorage.setItem('enjin_' + key, val); } catch(e) {}
    }, key, s_jsonBuf);
    return true;
}

bool LuaStore::loadFromFile(const char* key) {
    int len = EM_ASM_INT({
        var key = UTF8ToString($0);
        var val = localStorage.getItem('enjin_' + key);
        if (!val) return 0;
        var bytes = lengthBytesUTF8(val) + 1;
        if (bytes > $2) return -1;   // buffer too small
        stringToUTF8(val, $1, $2);
        return bytes;
    }, key, s_jsonBuf, sizeof(s_jsonBuf));
    if (len <= 0) return false;
    clear();
    const char* p = s_jsonBuf;
    return parseFromBuffer(p);  // reuse existing readJson* logic
}
```

**WASM serializer refactor required:** The current `saveToFile` writes to `std::ofstream`. A new `writeToBuffer(char* buf, int cap)` helper (or equivalent) is needed that writes the same JSON to a char array. The existing JSON-reading functions (`readJsonString`, `readJsonValue`, etc.) already operate on `const char*` and can be reused as-is for `loadFromFile`.

### Pattern 2: ESP32 NVS Backend

**What:** Replace the ESP32 stub in `bindings_store.cpp` with real NVS calls. NVS stores key-value pairs in flash with wear levelling and atomic writes.

**When to use:** Only inside `LuaStore::saveToFile()` and `loadFromFile()` under `#elif defined(ESP32)`.

**NVS constraint alignment with LuaStore:**
- NVS max key length: 15 characters. LuaStore `STORE_MAX_KEY` must be <= 15, or the save loop truncates keys to 15 chars. Verify the current `STORE_MAX_KEY` constant.
- NVS string values: max 4000 bytes per value. The LuaStore serializes each top-level slot as a JSON fragment. Since `STORE_MAX_STRING = 64` and `STORE_MAX_TABLE_ENTRIES = 8`, the worst-case serialized slot is well under 4000 bytes.
- NVS namespace: use `"enjin"` (max namespace length is 15 chars in ESP-IDF).

**Trade-offs:**
- NVS requires `nvs_flash_init()` called before first use, typically in `app_main()`. The enjin2 ESP32 host must call this before initializing LuaBindings.
- NVS `nvs_commit()` must be called after writes; failing to call it means data is not persisted if power is lost.
- NVS initialization can fail (`ESP_ERR_NVS_NO_FREE_PAGES`); the correct response is `nvs_flash_erase()` followed by `nvs_flash_init()` again. This is boilerplate for the setup script / host code.

**Example:**
```cpp
#elif defined(ESP32)
#include "nvs_flash.h"
#include "nvs.h"

// Each LuaStore is saved as a per-key NVS string entry.
// The "path" parameter is used as the NVS namespace.
// Each LuaStore slot is serialized as a single JSON string value under its key.

bool LuaStore::saveToFile(const char* ns) const {
    nvs_handle_t h;
    if (nvs_open(ns, NVS_READWRITE, &h) != ESP_OK) return false;
    bool ok = true;
    for (int i = 0; i < m_count; ++i) {
        char buf[256];
        // serialize m_entries[i] to buf (use new writeSlotToBuffer helper)
        if (nvs_set_str(h, m_entries[i].key, buf) != ESP_OK) { ok = false; }
    }
    if (ok) nvs_commit(h);
    nvs_close(h);
    return ok;
}

bool LuaStore::loadFromFile(const char* ns) {
    nvs_handle_t h;
    if (nvs_open(ns, NVS_READONLY, &h) != ESP_OK) return false;
    clear();
    // NVS does not enumerate keys; load known keys by iterating all possible
    // entries (limitation: requires a meta-key storing the list of active keys)
    // Alternative: store a compact JSON blob under a single meta key "store_data"
    nvs_close(h);
    return true;
}
```

**NVS enumeration limitation:** NVS does not provide a native "list all keys in namespace" API at the app level (it exists in diagnostics but is not stable). The recommended approach for LuaStore is to serialize the entire store as a single JSON blob stored under one NVS key (e.g., `"store_data"`), identical to the desktop approach but stored in NVS instead of a file. This avoids the enumeration problem entirely.

**Preferred NVS approach:**
```
LuaStore::saveToFile(ns):
    serialize entire store to JSON in s_jsonBuf (reuse existing logic)
    nvs_set_str(h, "store_data", s_jsonBuf)
    nvs_commit(h)

LuaStore::loadFromFile(ns):
    nvs_get_str(h, "store_data", s_jsonBuf, &len)
    parse s_jsonBuf using existing readJson* functions
```

This means both WASM and ESP32 backends share the same serializer path, just with different storage backends. The serializer refactor (splitting `writeToBuffer` out of `saveToFile`) benefits both platforms.

### Pattern 3: Shared JSON Serializer (Refactor Required)

**What:** Both WASM and ESP32 backends need to serialize `LuaStore` to a char buffer. The current `saveToFile` writes directly to `std::ofstream`. A `writeToBuffer(char* out, size_t cap) const` function must be extracted.

**How to structure:**
```cpp
// New private helper on LuaStore (or file-static in bindings_store.cpp)
static int writeStoreToBuffer(const LuaStore& store, char* out, size_t cap);

// saveToFile on desktop:
bool LuaStore::saveToFile(const char* path) const {
    std::ofstream f(path);
    if (!f.is_open()) return false;
    char buf[4096];
    writeStoreToBuffer(*this, buf, sizeof(buf));
    f << buf;
    return f.good();
}

// saveToFile on WASM and ESP32: use writeStoreToBuffer then their respective APIs
```

This refactor is internal to `bindings_store.cpp`; no header changes required.

### Pattern 4: Build Verification Script

**What:** `scripts/setup-dev.sh` installs emsdk and ESP-IDF on Arch Linux. The script is idempotent: it checks for existing installations before cloning.

**When to use:** New developer onboarding; CI environment setup.

**Structure:**
```bash
#!/usr/bin/env bash
set -euo pipefail

EMSDK_DIR="$HOME/emsdk"
ESPIDF_DIR="$HOME/esp/esp-idf"

# Install system deps
sudo pacman -S --needed --noconfirm cmake ninja python git curl

# Emscripten
if [ ! -d "$EMSDK_DIR" ]; then
    git clone https://github.com/emscripten-core/emsdk.git "$EMSDK_DIR"
fi
cd "$EMSDK_DIR"
./emsdk install latest
./emsdk activate latest
echo "source $EMSDK_DIR/emsdk_env.sh" >> ~/.zshrc

# ESP-IDF (v5.x recommended; use AUR or manual install)
if [ ! -d "$ESPIDF_DIR" ]; then
    mkdir -p "$(dirname "$ESPIDF_DIR")"
    git clone --depth 1 --branch v5.4.1 \
        https://github.com/espressif/esp-idf.git "$ESPIDF_DIR"
fi
cd "$ESPIDF_DIR"
./install.sh esp32s3   # or esp32 depending on target chip
echo "source $ESPIDF_DIR/export.sh" >> ~/.zshrc
```

**Arch-specific ncurses note:** The precompiled `xtensa-esp32-elf-gdb` in ESP-IDF links against ncurses 5, but Arch Linux ships ncurses 6. The setup script must install `ncurses5-compat-libs` from AUR before running `install.sh`.

### Pattern 5: Docusaurus Tutorial Integration

**What:** Tutorials live in `docs/src/tutorials/` (a new subdirectory of the existing `src/` content path). The existing Docusaurus config uses `path: 'src'` with `routeBasePath: '/'`, so files under `src/tutorials/` automatically get routes at `/tutorials/*`.

**When to use:** When adding the arkanoid and tamagotchi walkthroughs.

**Critical constraint:** The existing `docusaurus.config.js` excludes `api/**` from the main docs plugin (`exclude: ['api/**']`). Tutorials are under `src/tutorials/`, not `api/`, so no exclusion rule change is needed.

**Sidebar wiring:** The existing `sidebars.js` manually lists items under `guidesSidebar`. Add a `tutorials` category block manually (mirrors the existing `Core Concepts` and `Graphics` category pattern). Do not use `type: 'autogenerated'` for the whole sidebar — it would interfere with the existing `api` plugin structure.

**Tutorial content pattern:** Each tutorial file should:
1. Reference the demo Lua script in `scripts/` (e.g., `arkanoid.lua`)
2. Walk through one concept per section (not a full line-by-line dump)
3. Include code blocks using the `lua` language identifier (already configured in Docusaurus `prism` config with `additionalLanguages: ['cpp', 'cmake', 'bash']` — add `'lua'` here)

**Lua syntax highlighting:** The existing `prism` config in `docusaurus.config.js` does not include `lua`. This must be added:
```javascript
prism: {
  additionalLanguages: ['cpp', 'cmake', 'bash', 'lua'],  // add 'lua'
},
```

---

## Data Flow

### LuaStore Save/Load Data Flow (v1.8 — All Platforms)

```
Lua: engine.store.save("score", 42)
    └── LuaStore::setNumber("score", 42.0)          [in-memory; unchanged]
    └── if m_storePath != "": LuaStore::saveToFile(m_storePath)

saveToFile(path) dispatch:

  SDL3 desktop:
    writeStoreToBuffer() → std::ofstream(path)     [existing; refactored to use shared serializer]

  WASM:
    writeStoreToBuffer(s_jsonBuf) → EM_ASM localStorage.setItem('enjin_' + path, buf)

  ESP32:
    writeStoreToBuffer(s_jsonBuf) → nvs_set_str(h, "store_data", buf) + nvs_commit()

Lua: engine.store.path("my_save")
    └── m_storePath = "my_save"
    └── LuaStore::loadFromFile("my_save")

loadFromFile(path) dispatch:

  SDL3 desktop:
    std::ifstream(path) → readJson*()              [existing; unchanged]

  WASM:
    EM_ASM_INT to copy localStorage.getItem() into s_jsonBuf → readJson*()

  ESP32:
    nvs_get_str(h, "store_data", s_jsonBuf) → readJson*()
```

### WASM Build Verification Flow

```
Developer runs: scripts/build-wasm.sh
    └── source emsdk_env.sh
    └── mkdir -p build_wasm && cd build_wasm
    └── emcmake cmake -DENJIN2_BUILD_WASM=ON -DENJIN2_BUILD_LUA=ON ..
    └── emmake make enjin2_wasm
    └── output: build_wasm/enjin2.js + build_wasm/enjin2.wasm

Verification steps (manual or CI):
    1. emcc --version (confirm emsdk active)
    2. cmake configure succeeds (no missing includes)
    3. emmake make produces .js + .wasm without errors
    4. Load enjin2.js in a browser; call testFunction() → 42
    5. Run a Lua script via LuaEngine; confirm engine.store.save() writes to localStorage
```

### Dev Environment Setup Flow

```
New developer:
    git clone repo && cd enjin
    ./scripts/setup-dev.sh
        ├── installs system packages (pacman)
        ├── installs ncurses5-compat-libs (AUR — needed for ESP32 gdb on Arch)
        ├── clones emsdk → installs + activates latest
        ├── clones esp-idf v5.x → runs install.sh
        └── appends source lines to ~/.zshrc

Developer then:
    source ~/.zshrc   (or open new terminal)
    cmake -B build -DENJIN2_BUILD_SDL=ON && cmake --build build
    ./scripts/build-wasm.sh
```

---

## Integration Points (New vs. Modified)

### Explicit New/Modified Table

| Item | New or Modified | Integration Point | File(s) |
|------|----------------|-------------------|---------|
| WASM localStorage backend | **New** | `#elif defined(__EMSCRIPTEN__)` branch in `bindings_store.cpp` | `src/scripting/bindings_store.cpp` |
| ESP32 NVS backend | **New** | `#elif defined(ESP32)` branch in `bindings_store.cpp` | `src/scripting/bindings_store.cpp` |
| JSON buffer serializer | **New (refactor)** | `writeStoreToBuffer()` extracted from `saveToFile()`; used by WASM and ESP32 backends | `src/scripting/bindings_store.cpp` |
| WASM build fix verification | **Modified** | Confirm all v1.7 features build under Emscripten; fix any missing guards | `CMakeLists.txt`, possibly `src/scripting/` |
| `-lidbfs.js` linker flag | **Modified** (optional) | Add to `target_link_options(enjin2_wasm ...)` IF IDBFS is used (not needed if localStorage approach is used) | `CMakeLists.txt` |
| ESP32 build fix verification | **Modified** | Confirm 5-layer stack, coroutines, store compile under IDF | ESP32 host CMake; `bindings_store.cpp` |
| `pre.js` module init | **Modified** | May need `onRuntimeInitialized` updates if IDBFS is adopted instead of localStorage | `src/bindings/pre.js` |
| `setup-dev.sh` | **New** | Arch Linux dev environment; installs emsdk + ESP-IDF | `scripts/setup-dev.sh` |
| `build-wasm.sh` | **Modified** | Move from project root to `scripts/`; fix emsdk path assumption | `scripts/build-wasm.sh` |
| `build-sdl.sh` helper | **New** | Wraps cmake configure + build for SDL3 target | `scripts/build-sdl.sh` |
| `docs/src/tutorials/` | **New** | Tutorial content for arkanoid and tamagotchi demos | `docs/src/tutorials/*.md` |
| `docs/sidebars.js` | **Modified** | Add `tutorials` category to `guidesSidebar` | `docs/sidebars.js` |
| `docs/docusaurus.config.js` prism | **Modified** | Add `'lua'` to `additionalLanguages` | `docs/docusaurus.config.js` |
| Tech debt: m_followTargetProxy | **Modified** | Clear in `registerAll()` / `setActiveScene()` | `src/scripting/bindings_engine.cpp` or `camera.cpp` |
| Tech debt: tween await | **Modified** | `engine.async.wait_frames(n)` variant | `src/scripting/bindings_async.cpp` |
| Tech debt: camera dead zone | **Modified** | `C_Camera` dead zone threshold before follow activates | `src/components/camera.cpp`, `bindings_engine.cpp` |

### Internal Boundaries

| Boundary | Communication | Notes |
|----------|---------------|-------|
| `LuaStore` ↔ WASM storage | `EM_ASM` macro with `UTF8ToString` / `stringToUTF8` | Static 4KB char buffer is sufficient; stack allocation safe in single-threaded WASM |
| `LuaStore` ↔ ESP32 NVS | `nvs_open` / `nvs_set_str` / `nvs_commit` / `nvs_close` | ESP32 host must call `nvs_flash_init()` before LuaBindings init; NVS namespace limit is 15 chars |
| `LuaStore::m_storePath` ↔ WASM key | `m_storePath` used as localStorage key suffix | Prefix `'enjin_'` prevents collision with other localStorage users on the same origin |
| `LuaStore::m_storePath` ↔ NVS namespace | `m_storePath` used as NVS namespace string | Max 15 chars; script must set a short path: `engine.store.path("save")` not `"my_long_save_path"` |
| `emscripten_bindings.cpp` ↔ `enjin2_lua` | Emscripten embind exposure of `LuaScriptSystem` | Already present; WASM build issues likely in v1.7 features not yet verified under Emscripten |
| `CMakeLists.txt` ↔ Emscripten | `-lidbfs.js` linker flag (not needed with localStorage approach) | If localStorage: no IDBFS link needed; simpler |
| `scripts/setup-dev.sh` ↔ emsdk | `emsdk install latest` + `emsdk activate latest` | Path must be sourced in shell; script appends to `~/.zshrc` |
| Docusaurus `src/` ↔ tutorials | `docs/src/tutorials/*.md` within `path: 'src'` plugin config | No config change to `docusaurus.config.js` plugin section; only `sidebars.js` needs the new category |

---

## Build Order

Dependencies for v1.8 phases. Phases with no inter-dependency can run in parallel.

```
Phase A: WASM build verification + fixes
  Goal: emcmake cmake + emmake make produces enjin2.js/.wasm cleanly.
  Work: Run build_wasm.sh; diagnose compile errors; fix preprocessor guards
        in any v1.7 files that assume VCV_RACK or SDL-only context.
  Dependencies: none (start here — must pass before WASM-specific features).

Phase B: ESP32 build verification + fixes
  Goal: idf.py build produces enjin2 firmware cleanly with 5-layer stack.
  Work: Diagnose IDF-specific compile errors; verify coroutine library opens;
        confirm PSRAM availability for 5 layers (may need ENJIN_LAYER_COUNT=3 fallback).
  Dependencies: none (parallel to Phase A; independent platform).

Phase C: JSON buffer serializer refactor (in bindings_store.cpp)
  Goal: Extract writeStoreToBuffer() and reuse existing readJson* functions.
  Work: No API change; pure internal refactor of bindings_store.cpp.
  Dependencies: Phase A + Phase B complete (know what compile errors exist
                before touching bindings_store.cpp).

Phase D: WASM localStorage backend
  Goal: engine.store.save/load works in browser via localStorage.
  Work: Add #elif __EMSCRIPTEN__ branch using EM_ASM; use writeStoreToBuffer.
  Dependencies: Phase C (serializer refactor), Phase A (WASM build clean).

Phase E: ESP32 NVS backend
  Goal: engine.store.save/load works on ESP32 via NVS.
  Work: Add #elif ESP32 branch using nvs_flash; use writeStoreToBuffer.
        Document that host must call nvs_flash_init() before engine init.
  Dependencies: Phase C (serializer refactor), Phase B (ESP32 build clean).

Phase F: Tech debt cleanup
  Goal: Fix m_followTargetProxy clear, tween await, camera dead zone.
  Work: Targeted modifications; each is a small, isolated change.
  Dependencies: Phase A + Phase B (build clean first).

Phase G: Dev environment setup script
  Goal: scripts/setup-dev.sh installs emsdk + ESP-IDF on Arch Linux.
  Work: Write idempotent bash script; document ncurses5-compat-libs AUR step.
  Dependencies: none (documentation/scripting; does not touch engine).

Phase H: Build helper scripts
  Goal: scripts/build-wasm.sh, build-sdl.sh, build-esp32.sh.
  Work: Refactor existing build_wasm.sh; add SDL and ESP32 wrappers.
  Dependencies: Phase A + Phase B (know what the correct build commands are).

Phase I: Docusaurus tutorials
  Goal: tutorials/ directory with getting-started, arkanoid, tamagotchi guides.
  Work: Write MDX; add tutorials category to sidebars.js; add 'lua' to prism config.
  Dependencies: none (pure documentation; independent of engine work).
```

**Suggested phase grouping:**

| Group | Phases | Rationale |
|-------|--------|-----------|
| 1 — Build verification | A + B | Must come first; find all compile errors before touching platform code |
| 2 — Serializer foundation | C | Prerequisite for D and E; tiny isolated refactor |
| 3 — Platform backends | D + E | WASM and ESP32 are independent; can be done in parallel |
| 4 — Tech debt | F | Isolated changes; can begin after builds are green |
| 5 — Infrastructure + docs | G + H + I | No engine deps; can overlap with phases 2–4 |

---

## Anti-Patterns

### Anti-Pattern 1: Using IDBFS Instead of localStorage for LuaStore

**What people do:** Mount an IDBFS virtual filesystem and call `saveToFile` with a real file path, relying on `FS.syncfs()` to persist to IndexedDB.

**Why it is wrong:** IDBFS `FS.syncfs()` is asynchronous. The `LuaStore::saveToFile()` signature is synchronous (returns bool). Bridging the async callback into a synchronous C++ return requires `addRunDependency` or blocking hacks that stall the main thread. localStorage is fully synchronous and sufficient for LuaStore's tiny data volume.

**Do this instead:** Use `EM_ASM` with `localStorage.setItem` / `localStorage.getItem` directly. Only consider IDBFS if LuaStore data exceeds localStorage quotas (it will not given the 16-key/64-char limits).

### Anti-Pattern 2: Separate NVS Key per LuaStore Slot

**What people do:** Call `nvs_set_str(h, slot.key, slot.value)` for each slot individually, relying on NVS to enumerate keys later.

**Why it is wrong:** NVS does not provide a stable key enumeration API for app use. `nvs_entry_find` exists but is designed for diagnostics. Iterating all possible keys on load requires knowing what keys were saved.

**Do this instead:** Serialize the entire LuaStore to a single JSON blob, write it to one NVS key (`"store_data"`). This is the same pattern as the desktop and WASM backends, just with a different storage sink. Enumeration is handled by the existing JSON parser.

### Anti-Pattern 3: Unconditional `nvs_flash_init()` Inside LuaStore

**What people do:** Call `nvs_flash_init()` inside `LuaStore::loadFromFile()` to make the backend self-contained.

**Why it is wrong:** `nvs_flash_init()` is a one-time system call that must happen before any NVS operation. Calling it multiple times (once per LuaStore load) causes errors. It belongs in the ESP32 host's `app_main()` before engine initialization.

**Do this instead:** Document in the ESP32 integration guide that `nvs_flash_init()` must be called before constructing LuaBindings. The NVS backend in `bindings_store.cpp` calls only `nvs_open` / `nvs_set_str` / `nvs_commit` / `nvs_close`.

### Anti-Pattern 4: Tutorial Files Outside `docs/src/`

**What people do:** Place tutorial MDX files in a top-level `tutorials/` directory or in `docs/docs/`.

**Why it is wrong:** The Docusaurus config uses `path: 'src'` for the main guides plugin. Files outside `src/` are not discovered by this plugin. The existing content all lives under `docs/src/`.

**Do this instead:** `docs/src/tutorials/` — a subdirectory of the existing content path. No plugin config changes needed.

### Anti-Pattern 5: WASM Build Script Hardcoding the emsdk Path

**What people do:** Write `build_wasm.sh` (as the current version does) with a hardcoded relative `../emsdk` path, assuming the developer has emsdk adjacent to the project directory.

**Why it is wrong:** The `setup-dev.sh` script will install emsdk to `$HOME/emsdk`, not `../emsdk`. The build script fails for anyone who used `setup-dev.sh`.

**Do this instead:** In `scripts/build-wasm.sh`, detect emsdk via `$EMSDK` environment variable (set by `emsdk_env.sh`), then fall back to `../emsdk`, then fail with an actionable error. This handles both the setup-script path and the legacy adjacent-directory path.

---

## Confidence Assessment

| Area | Confidence | Basis |
|------|------------|-------|
| WASM localStorage EM_ASM bridge | HIGH | Verified against Emscripten official docs; pattern is explicit in docs with UTF8ToString |
| WASM CMake flag interactions | MEDIUM | The `-lidbfs.js` issue is documented in emscripten/issues #15491; localStorage approach avoids it entirely |
| ESP32 NVS API | HIGH | Verified against official ESP-IDF docs (v5.x stable); key/namespace limits confirmed |
| ESP32 NVS enumeration limitation | HIGH | Official docs explicitly state `nvs_entry_find` is for diagnostics; confirmed by community usage patterns |
| Docusaurus tutorial sidebar | HIGH | Verified against official Docusaurus autogenerated sidebar docs; manual array approach matches existing sidebars.js pattern |
| Arch Linux ncurses5 issue | MEDIUM | Documented in AUR comments and ESP-IDF community; applies to precompiled gdb binary |
| WASM build current status | LOW | Not verified by actually running the build; PROJECT.md notes "Full Emscripten toolchain build not verified" |
| ESP32 PSRAM 5-layer stack | LOW | PROJECT.md flags this as known debt; actual PSRAM availability depends on hardware variant |

---

## Sources

- `/home/unwn/dev/enjin/src/scripting/bindings_store.cpp` — current platform guard structure, existing JSON I/O, LuaStore::saveToFile/loadFromFile (HIGH confidence)
- `/home/unwn/dev/enjin/CMakeLists.txt` — enjin2_wasm target, Emscripten flags, ENJIN2_BUILD_SDL option (HIGH confidence)
- `/home/unwn/dev/enjin/src/bindings/pre.js` — WASM module preamble, onRuntimeInitialized hook (HIGH confidence)
- `/home/unwn/dev/enjin/src/bindings/emscripten_bindings.cpp` — Emscripten bind surface (HIGH confidence)
- `/home/unwn/dev/enjin/src/scripting/lua_platform.cpp` — ESP32 platform guard patterns, NVS includes present (HIGH confidence)
- `/home/unwn/dev/enjin/build_wasm.sh` — existing WASM build script (shows hardcoded path assumption) (HIGH confidence)
- `/home/unwn/dev/enjin/docs/docusaurus.config.js` — Docusaurus plugin config, path settings, prism config (HIGH confidence)
- `/home/unwn/dev/enjin/docs/sidebars.js` — existing guidesSidebar structure (HIGH confidence)
- `/home/unwn/dev/enjin/.planning/PROJECT.md` — v1.8 target features, known tech debt, constraints (HIGH confidence)
- [Emscripten File System API docs](https://emscripten.org/docs/api_reference/Filesystem-API.html) — IDBFS, FS.syncfs, async/sync characteristics (HIGH confidence)
- [Emscripten Interacting with code docs](https://emscripten.org/docs/porting/connecting_cpp_and_javascript/Interacting-with-code.html) — EM_ASM, EM_JS, UTF8ToString patterns (HIGH confidence)
- [ESP-IDF NVS docs (stable v5.5.3)](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/storage/nvs_flash.html) — key/namespace limits, API surface, commit requirement (HIGH confidence)
- [Docusaurus Autogenerated Sidebar docs](https://docusaurus.io/docs/next/sidebar/autogenerated) — category metadata, sidebar_position front matter (HIGH confidence)
- [AUR esp-idf package](https://aur.archlinux.org/packages/esp-idf) — Arch Linux install path, ncurses dependency (MEDIUM confidence)
- [Emscripten GitHub issue #15491](https://github.com/emscripten-core/emscripten/issues/15491) — IDBFS CMake `-lidbfs.js` compiler flag warning (MEDIUM confidence)

---

*Architecture research for: enjin2 v1.8 Ship Ready (platform hardening + developer onboarding)*
*Researched: 2026-03-02*
