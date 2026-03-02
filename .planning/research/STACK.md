# Stack Research

**Domain:** Cross-platform 2D engine hardening — WASM/Emscripten build verification, ESP32 NVS storage, WASM localStorage bridge, Arch Linux dev setup scripting, Docusaurus tutorial authoring (v1.8)
**Researched:** 2026-03-02
**Confidence:** HIGH for Emscripten/ESP-IDF/Docusaurus APIs (verified via official docs, WebSearch with multiple agreeing sources); MEDIUM for specific version pinning (emsdk 3.1.73 vs 4.x trade-off)

---

## Scope

This document covers **only stack additions and integration decisions for v1.8 Ship Ready**. It does not re-research validated v1.0–v1.7 capabilities (SDL3 runner, LuaJIT scripting, CMake multi-target build, Docusaurus + Doxygen pipeline, etc.).

The v1.8 work is an integration and hardening milestone — new code is thin, mostly glue. The major technical decisions are:

1. Which Emscripten version to pin for the WASM build
2. How to bridge `localStorage` to `LuaStore::saveToFile/loadFromFile` on WASM
3. Which ESP-IDF NVS API calls satisfy `LuaStore` on ESP32
4. What Arch Linux packages the dev setup script needs
5. How to structure tutorial docs in the existing Docusaurus site

---

## What Already Exists (Critical Integration Context)

| Existing Element | Implication for v1.8 |
|-----------------|----------------------|
| `build_wasm.sh` — sources emsdk from `../emsdk`, calls `emcmake cmake` | Setup script must install emsdk alongside the repo, or update script path. The `source emsdk_env.sh` pattern is already used — dev script just automates this |
| `CMakeLists.txt` — `ENJIN2_BUILD_WASM=ON` target with Embind, `EXPORT_ES6=1`, `MODULARIZE=1` | Emscripten 4.x requires C++17 for Embind. CMake already sets `CMAKE_CXX_STANDARD 17` globally — this constraint is already satisfied |
| `src/scripting/bindings_store.cpp` — `#if !defined(ESP32) && !defined(__EMSCRIPTEN__)` guard around `saveToFile`/`loadFromFile` | WASM and ESP32 implementations live inside this guard region. The `#else` stub `return false` is the integration point — replace with real implementations |
| `src/bindings/emscripten_bindings.cpp` — uses `emscripten::val`, `typed_memory_view`, `EM_BINDINGS` | `EM_JS` macro for localStorage is the natural companion — same file, or a new `emscripten_store.cpp` |
| `luajit/src/ljamalg.c` — LuaJIT amalgamated build with `LUAJIT_DISABLE_JIT`, `LUAJIT_DISABLE_FFI` | LuaJIT interpreter-only mode is already the WASM strategy — confirmed correct by research. Do not attempt JIT on WASM |
| `src/scripting/lua_platform.cpp` — ESP32 branch includes `esp_heap_caps.h`, `esp_spiffs.h` | NVS headers (`nvs_flash.h`, `nvs.h`) follow the same pattern — include under `#ifdef ESP32` guard |
| Docusaurus 3.9.2 already installed at `docs/package.json` with `@docusaurus/plugin-content-docs` (id: `api`) | Tutorial docs go into `docs/src/` (the guides plugin, `routeBasePath: '/'`). No new Docusaurus plugins needed for tutorials |

---

## Recommended Stack

### Core Technologies

| Technology | Version | Purpose | Why Recommended |
|------------|---------|---------|-----------------|
| Emscripten (emsdk) | **3.1.73** (pin, not `latest`) | Cross-compile enjin2 to WASM | 3.1.73 is the last widely-validated 3.1.x release. Emscripten 4.x landed early 2025 and requires a full rebuild. Pinning 3.1.73 avoids the 4.x Embind C++17 enforcement surprise (the project already uses C++17, so 4.x is actually safe — but 3.1.73 is what `build_wasm.sh` was written against, and version stability matters for reproducibility). If choosing 4.x, use 4.0.0+. The project's `CMAKE_CXX_STANDARD 17` satisfies the C++17 Embind requirement in either version. |
| ESP-IDF | **v5.5.x** (latest stable) | Build and flash ESP32 target | v5.5.3 is the current stable as of 2026-03. NVS API is unchanged between v4.x and v5.x at the C call level. v6.0-beta1 exists but is beta — use stable. The AUR `esp-idf` package installs v5.5. |
| ESP-IDF NVS component | Bundled in ESP-IDF v5.5 | Key-value persistent storage for LuaStore on ESP32 | NVS is the canonical Espressif mechanism for small key-value pairs. 15-char key limit, 4000-byte string limit — fits LuaStore's `STORE_MAX_KEY=16` and `STORE_MAX_STRING` constraints exactly. No external library. |
| Emscripten `EM_JS` macro | Part of Emscripten SDK | localStorage bridge from C++ to browser | `localStorage` is synchronous on the JS side — no Asyncify needed. `EM_JS` with `UTF8ToString`/`stringToUTF8` is the correct, zero-overhead pattern for a C++ ↔ localStorage bridge. |
| Docusaurus 3.9.2 (existing) | Already installed | Tutorial authoring | No version change needed. The existing dual-plugin setup (`guides` + `api`) supports tutorial docs in `docs/src/tutorials/`. Use `_category_.json` with `position` and `link.type: "generated-index"` for category pages. |

### Supporting Libraries

| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| `nvs_flash.h` + `nvs.h` | ESP-IDF v5.5 (bundled) | NVS init + handle management | In `bindings_store.cpp` ESP32 branch: `nvs_flash_init()` once at boot, `nvs_open()` / `nvs_set_str()` / `nvs_get_str()` / `nvs_commit()` / `nvs_close()` per store operation |
| `<emscripten.h>` | Emscripten SDK (bundled) | `EM_JS` macro for localStorage calls | In `bindings_store.cpp` WASM branch — already included transitively via the build, but include explicitly for `EM_JS` |
| `@docusaurus/plugin-content-docs` | 3.9.2 (already installed) | Tutorial content instance | Already wired as the `guides` preset — tutorials drop into `docs/src/tutorials/` with appropriate frontmatter |

### Development Tools

| Tool | Purpose | Notes |
|------|---------|-------|
| `emsdk` (git clone) | Emscripten SDK manager | Install at `~/emsdk` or alongside repo. `./emsdk install 3.1.73 && ./emsdk activate 3.1.73`. The `build_wasm.sh` looks for `../emsdk` relative to the project — dev script should honor this path or parameterize it |
| `idf.py` (ESP-IDF) | ESP32 build + flash | Installed via `esp-idf` AUR package (places ESP-IDF at `/opt/esp-idf`) or manual clone + `./install.sh esp32` |
| `yay` / `paru` (AUR helper) | Install AUR packages | Required for `esp-idf` from AUR. The dev script should check for an AUR helper before attempting AUR installs |
| `python3` >= 3.10 | Emscripten + ESP-IDF runtime | Both toolchains require Python 3.10+. Arch ships current Python; verify with `python --version` |
| Doxygen + Node 18+ | Doc generation (existing) | No change for v1.8 — already in the repo. Tutorial docs are plain Markdown, not Doxygen-generated |

---

## Arch Linux Package List (Dev Setup Script)

This is the authoritative list for the `setup-dev-arch.sh` script. Verified against ESP-IDF v5.5 official Linux docs and Arch package search results.

### Pacman (official repos)

```bash
sudo pacman -S --needed \
  # WASM toolchain prerequisites
  python python-pip \
  cmake ninja \
  git \
  # ESP-IDF prerequisites
  gcc make flex bison gperf \
  ccache dfu-util libusb \
  # Existing project deps (verify presence)
  lua doxygen nodejs npm
```

### AUR (via yay/paru)

```bash
yay -S --needed \
  esp-idf \
  ncurses5-compat-libs  # required for xtensa-esp32-elf-gdb on Arch
```

**Notes:**
- `emscripten` is in the official `extra` repo (`sudo pacman -S emscripten`) at version 5.0.2-1 as of 2026-03. However, the project uses a manually-cloned emsdk for version pinning — using the pacman `emscripten` package bypasses version control. Recommend: skip pacman `emscripten`, clone emsdk manually and pin to 3.1.73.
- `esp-idf` AUR package places ESP-IDF at `/opt/esp-idf`. After install, run `/opt/esp-idf/install.sh esp32` and source `/opt/esp-idf/export.sh` in the shell.
- The emsdk clone goes to `~/emsdk` or a project-adjacent path. The `build_wasm.sh` script expects `../emsdk` — the dev setup script should clone there or update the path variable.

---

## API Patterns

### WASM localStorage Bridge (`bindings_store.cpp` — WASM branch)

The `localStorage` API is synchronous in the browser — no Asyncify or async bridge needed. Use `EM_JS` to call `localStorage.setItem`/`getItem` directly:

```cpp
// In bindings_store.cpp, inside #ifdef __EMSCRIPTEN__

#include <emscripten.h>

// Write a C string to localStorage under a key
EM_JS(void, js_localStorage_setItem, (const char* key, const char* value), {
    try {
        localStorage.setItem(UTF8ToString(key), UTF8ToString(value));
    } catch(e) {
        // Storage quota exceeded or private browsing restriction
        console.warn('[enjin2] localStorage.setItem failed:', e);
    }
});

// Read a C string from localStorage — writes into caller-provided buffer
EM_JS(int, js_localStorage_getItem, (const char* key, char* out, int maxLen), {
    var val = localStorage.getItem(UTF8ToString(key));
    if (val === null) return 0;
    var encoded = intArrayFromString(val);
    var len = Math.min(encoded.length, maxLen - 1);
    writeArrayToMemory(encoded.slice(0, len), out);
    HEAP8[out + len] = 0;  // null terminator
    return len;
});

// LuaStore::saveToFile — serialize via existing JSON writer, push to localStorage
bool LuaStore::saveToFile(const char* path) const {
    // Reuse existing JSON serialization via in-memory buffer
    // (write to a static char buffer, then push to localStorage)
    static char jsonBuf[4096];  // size to fit max store content
    // ... fill jsonBuf with JSON ...
    js_localStorage_setItem(path, jsonBuf);
    return true;
}

// LuaStore::loadFromFile — pull from localStorage, parse via existing JSON reader
bool LuaStore::loadFromFile(const char* path) {
    static char buf[4096];
    int len = js_localStorage_getItem(path, buf, sizeof(buf));
    if (len == 0) return false;
    // ... parse buf via existing readJsonValue logic ...
    return true;
}
```

**Integration note:** The existing JSON serializer in `bindings_store.cpp` writes to `std::ofstream`. For WASM, the serializer must write to a `char[]` buffer instead, then pass it to `js_localStorage_setItem`. This means extracting the JSON writer into a `writeToBuffer(char* buf, int maxLen)` helper that works on both branches. The reader already parses `const char*` — WASM only needs to fill that buffer from localStorage.

**Key limit:** `localStorage` strings have a per-item size limit of ~5 MB in modern browsers. The LuaStore max content (16 keys × max string values) is well under 4 KB — no risk of exceeding storage limits.

### ESP32 NVS Storage (`bindings_store.cpp` — ESP32 branch)

The NVS API sequence: `nvs_flash_init()` once at boot, then per-operation `nvs_open()` → read/write → `nvs_commit()` → `nvs_close()`.

```cpp
// In bindings_store.cpp, inside #ifdef ESP32

#include "nvs_flash.h"
#include "nvs.h"

// NVS namespace for enjin2 store — max 15 chars
static constexpr const char* NVS_NAMESPACE = "enjin2_store";

// LuaStore::saveToFile — store all keys into NVS under the namespace
// The 'path' parameter is repurposed as the NVS partition label (use nullptr for default "nvs")
bool LuaStore::saveToFile(const char*) const {
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h);
    if (err != ESP_OK) return false;

    bool ok = true;
    for (int i = 0; i < m_count; ++i) {
        const StoreSlot& slot = m_entries[i];
        // Serialize each slot as a compact string value
        // Key must be <= 15 chars (NVS limit); STORE_MAX_KEY=16 — truncate to 15
        char nvsKey[16];
        strncpy(nvsKey, slot.key, 15);
        nvsKey[15] = '\0';

        // Store type tag + serialized value in one NVS string
        char valueBuf[STORE_MAX_STRING + 8];  // type prefix + value
        // ... encode slot into valueBuf ...
        if (nvs_set_str(h, nvsKey, valueBuf) != ESP_OK) { ok = false; break; }
    }

    if (ok) nvs_commit(h);
    nvs_close(h);
    return ok;
}

// LuaStore::loadFromFile — iterate known keys OR use NVS iterator
bool LuaStore::loadFromFile(const char*) {
    nvs_flash_init();  // idempotent after first call (returns ESP_ERR_NVS_... which is safe to ignore here)
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &h);
    if (err != ESP_OK) return false;

    // Use NVS iterator to enumerate all stored keys in the namespace
    nvs_iterator_t it = nullptr;
    err = nvs_entry_find("nvs", NVS_NAMESPACE, NVS_TYPE_STR, &it);
    clear();
    while (err == ESP_OK && it != nullptr) {
        nvs_entry_info_t info;
        nvs_entry_info(it, &info);
        // Read value for this key
        size_t needed = 0;
        nvs_get_str(h, info.key, nullptr, &needed);
        if (needed > 0 && needed < STORE_MAX_STRING + 8) {
            char valueBuf[STORE_MAX_STRING + 8];
            nvs_get_str(h, info.key, valueBuf, &needed);
            // ... decode valueBuf back into StoreSlot ...
        }
        err = nvs_entry_next(&it);
    }
    if (it) nvs_release_iterator(it);
    nvs_close(h);
    return true;
}
```

**NVS key constraint:** NVS keys are max 15 chars. `STORE_MAX_KEY` in `LuaStore` is 16 (15 chars + null). A 15-char NVS key exactly matches — the last character of a 15-char `STORE_MAX_KEY` value must be truncated. Solution: keep `STORE_MAX_KEY` at 16 in the store struct (it controls the in-memory char array), but truncate to 15 when writing to NVS. Document this.

**Type encoding:** NVS stores strings only. Encode the slot type as a prefix byte in the value string: `"N1.5"` for Number 1.5, `"Shello"` for String "hello", `"B1"` for Bool true, `"T{...}"` for Table. This avoids per-type NVS key proliferation.

**`nvs_flash_init()` placement:** For ESP32, this should be called once in `app_main` before the engine runs. In the enjin2 ESP32 host integration, add it to the platform init sequence. The `loadFromFile` stub can call it defensively (it is idempotent after success).

**NVS iterator availability:** `nvs_entry_find` / `nvs_entry_next` / `nvs_release_iterator` are available since ESP-IDF 4.0. Confirmed present in v5.5.

### Docusaurus Tutorial Structure

The existing Docusaurus site has a guides plugin rooted at `docs/src/` with `routeBasePath: '/'`. Tutorial docs go in a `tutorials/` subdirectory:

```
docs/src/
  tutorials/
    _category_.json          ← category metadata
    getting-started.md       ← intro page
    arkanoid-demo.md         ← tutorial using arkanoid script
    tamagotchi-demo.md       ← tutorial using tamagotchi script
    platform-targets.md      ← how to build for SDL/WASM/ESP32
```

`_category_.json` pattern:
```json
{
  "label": "Tutorials",
  "position": 2,
  "link": {
    "type": "generated-index",
    "description": "Step-by-step guides for building games with enjin2."
  }
}
```

Document frontmatter:
```yaml
---
id: getting-started
title: Getting Started
sidebar_label: Getting Started
sidebar_position: 1
description: Set up enjin2 and run your first script in 5 minutes.
---
```

No new Docusaurus plugins are needed. The project already has `prism.additionalLanguages: ['cpp', 'cmake', 'bash']` — code blocks in tutorials can use all three languages.

---

## What NOT to Add

| Avoid | Why | Use Instead |
|-------|-----|-------------|
| `pacman -S emscripten` (Arch official package) | Gives system Emscripten 5.0.2 — not version-pinned and differs from what `build_wasm.sh` expects | Clone emsdk manually, pin to 3.1.73. Dev script should `git clone https://github.com/emscripten-core/emsdk && ./emsdk install 3.1.73 && ./emsdk activate 3.1.73` |
| Asyncify for localStorage | localStorage is synchronous — Asyncify adds significant binary size bloat (~1.5–2x) with zero benefit here | `EM_JS` with synchronous `localStorage.setItem`/`getItem` — clean, zero overhead |
| IndexedDB for WASM storage | IndexedDB is asynchronous — would require Asyncify or JSPI, both of which complicate the build considerably | `localStorage` is synchronous, sufficient for LuaStore's small key-value payload (< 4 KB) |
| JSPI (JS Promise Integration) | Still phase 4 in W3C spec, only in Chrome 137+ and Firefox 139+ — not universally supported | Stick with synchronous localStorage via `EM_JS` |
| FAT filesystem for ESP32 NVS | FAT/LittleFS require flash partition configuration, significantly more setup | NVS is purpose-built for small key-value pairs, requires only `nvs_flash_init()` and partition table entry (default ESP-IDF project includes NVS partition by default) |
| `esp-idf` v6.0-beta1 | Beta — API may change before stable release | v5.5.x is the current stable with 30-month support window |
| Wasmoon / Fengari (Lua WASM runtimes) | Pre-compiled Lua environments, not the LuaJIT interpreter used by enjin2 | The existing `luajit/src/ljamalg.c` with `LUAJIT_DISABLE_JIT` + `LUAJIT_DISABLE_FFI` is the correct approach |
| LuaJIT JIT compilation on WASM | LuaJIT's JIT uses architecture-specific assembly — cannot compile to WASM | Already handled: `LUAJIT_DISABLE_JIT` is in the CMakeLists.txt WASM branch |
| Docusaurus plugins for tutorials | The existing `@docusaurus/plugin-content-docs` (guides instance) handles tutorials natively via `_category_.json` and `sidebar_position` frontmatter | No new plugins — add tutorial `.md` files to `docs/src/tutorials/` |
| Separate NVS key per StoreSlot field | Would use `STORE_MAX_KEYS * N` NVS keys per store, hitting namespace limits | Serialize entire slot as a single prefixed string value — one NVS key per LuaStore key |

---

## Alternatives Considered

| Recommended | Alternative | When to Use Alternative |
|-------------|-------------|-------------------------|
| Emscripten 3.1.73 (pinned) | Emscripten 4.x (latest) | If starting a new WASM project from scratch in 2026, prefer 4.x. For v1.8, pinning 3.1.73 first, then optionally bumping to 4.x in a separate phase de-risks the build verification work |
| `EM_JS` for localStorage | `emscripten::val` (val.h) | `val.h` is more idiomatic for complex object interaction; `EM_JS` is simpler and more explicit for two-function read/write bridge |
| NVS string serialization (type-prefixed) | One NVS key per slot field (e.g. `key_type`, `key_val`) | Multiple keys per slot adds complexity and hits NVS namespace key count limits faster. Single prefixed string is simpler |
| `esp-idf` AUR package | Manual ESP-IDF clone + install | Manual clone gives more control over version and path. AUR package is convenient for a dev setup script targeting common Arch users |
| `_category_.json` tutorial category | Manually maintaining `sidebars.js` entries | `_category_.json` is the modern Docusaurus 3 approach — auto-generated sidebars are simpler to maintain and scale |

---

## Version Compatibility

| Component | Version | Compatibility Notes |
|-----------|---------|---------------------|
| Emscripten 3.1.73 | emsdk 3.1.73 | Works with `CMAKE_CXX_STANDARD 17`. Embind in 3.1.x does not require C++17 (that became mandatory in 4.0.20). The project already uses C++17 so either version works — pinning 3.1.73 for stability |
| Emscripten 4.0.x | emsdk 4.x | Requires C++17 for Embind (project already satisfies this). If upgrading to 4.x, add `--std=c++17` explicitly to emcc CFLAGS or rely on `CMAKE_CXX_STANDARD 17` being propagated |
| ESP-IDF v5.5.x | NVS API | `nvs_entry_find` / `nvs_entry_next` / `nvs_release_iterator` available since v4.0. All API calls used are stable in v5.5. |
| NVS key max length | ESP-IDF all versions | 15 characters maximum. `LuaStore::STORE_MAX_KEY = 16` (15 chars + null) — truncate to 15 when writing to NVS. |
| `localStorage` key/value size | Browser standard | ~5 MB per item limit; LuaStore JSON payload < 4 KB. No risk of exceeding limit. |
| Docusaurus 3.9.2 | Already installed | `_category_.json` with `position` and `link.type: "generated-index"` supported since Docusaurus 3.0. `sidebar_position` frontmatter unchanged. |
| LuaJIT amalgam (`ljamalg.c`) | Emscripten 3.1.73 | `LUAJIT_DISABLE_JIT` + `LUAJIT_DISABLE_FFI` already set in CMakeLists.txt WASM branch. Amalgam build compiles as a single `.c` file passed to `emcc`. No compatibility issues. |
| Python 3.10+ | Emscripten 3.x / 4.x minimum | Emscripten changed minimum Python from 3.8 to 3.10. Arch Linux ships current Python (3.13.x as of 2026) — no issue. |
| Node.js 18.3+ | Emscripten 4.x minimum | Emscripten 4.x requires Node 18.3+. Arch ships current Node — no issue. The project already requires Node ≥ 18 for Docusaurus. |

---

## Sources

- [Emscripten emsdk GitHub](https://github.com/emscripten-core/emsdk) — install procedure, `./emsdk install 3.1.73` pattern
- [Emscripten Downloads — official docs](https://emscripten.org/docs/getting_started/downloads.html) — Python 3.10+ minimum, Node 18.3+ minimum
- [Emscripten ChangeLog](https://github.com/emscripten-core/emscripten/blob/main/ChangeLog.md) — Embind C++17 requirement in 4.0.20, confirmed via OpenCV issue #28178
- [Emscripten EM_JS + localStorage patterns — web.dev](https://web.dev/articles/emscripten-embedding-js-snippets) — `EM_JS` with `UTF8ToString`/`stringToUTF8` pattern, MEDIUM confidence (official Emscripten/Google source)
- [Emscripten Filesystem API docs](https://emscripten.org/docs/api_reference/Filesystem-API.html) — localStorage vs IndexedDB trade-offs, synchronous vs async
- [Synchronous LocalStorage filesystem gist](https://gist.github.com/makryl/96d87b23d7a7c3cc5bc1eee1021bb6ff) — community WASM localStorage bridge pattern, LOW confidence (single source, unverified)
- [ESP-IDF NVS Flash API Reference — stable v5.5.3](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/storage/nvs_flash.html) — `nvs_open`, `nvs_set_str`, `nvs_get_str`, `nvs_commit`, `nvs_entry_find`, key 15-char limit, HIGH confidence
- [ESP-IDF Releases — GitHub](https://github.com/espressif/esp-idf/releases) — v5.5.x is current stable, v6.0-beta1 exists
- [ESP-IDF Toolchain Setup Linux — stable](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/get-started/linux-macos-setup.html) — official Arch Linux pacman dependency list: `gcc git make flex bison gperf python cmake ninja ccache dfu-util libusb python-pip`
- [Arch Linux emscripten package](https://archlinux.org/packages/extra/x86_64/emscripten/) — version 5.0.2-1 in `extra` repo; confirms pacman path exists but is not version-pinned
- [AUR esp-idf package](https://aur.archlinux.org/packages/esp-idf) — installs ESP-IDF 5.5 to `/opt/esp-idf`
- [AUR gcc-xtensa-esp32-elf-bin](https://aur.archlinux.org/packages/gcc-xtensa-esp32-elf-bin) — standalone Xtensa GCC (not needed with modern ESP-IDF which auto-downloads toolchain)
- [ArchWiki ESP32](https://wiki.archlinux.org/title/ESP32) — ncurses5-compat-libs requirement for gdb on Arch
- [ESP32 PSRAM static allocation — ESP-IDF docs](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-guides/external-ram.html) — `EXT_RAM_BSS_ATTR` for 5-layer canvas on PSRAM-equipped ESP32
- [Docusaurus sidebar autogenerated docs](https://docusaurus.io/docs/sidebar/autogenerated) — `_category_.json`, `sidebar_position` frontmatter, `link.type: "generated-index"`
- Codebase direct analysis: `CMakeLists.txt`, `build_wasm.sh`, `src/scripting/bindings_store.cpp`, `src/bindings/emscripten_bindings.cpp`, `src/scripting/lua_platform.cpp`, `docs/package.json`, `docs/docusaurus.config.js`

---

*Stack research for: enjin2 v1.8 Ship Ready — cross-platform hardening*
*Researched: 2026-03-02*
