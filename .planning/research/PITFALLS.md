# Pitfalls Research

**Domain:** Adding Emscripten/WASM build verification, ESP32 NVS storage, WASM localStorage bridge, Arch Linux dev setup scripts, coroutine-tween integration, camera dead zone, and Docusaurus tutorials to an existing zero-alloc 2D game engine with Lua scripting (enjin2 v1.8 Ship Ready)
**Researched:** 2026-03-02
**Confidence:** HIGH (direct codebase analysis of shipped v1.7 sources, Emscripten official docs, ESP-IDF official docs, community issue trackers, and first-principles reasoning from embedded + Lua VM design)

---

## Critical Pitfalls

### Pitfall 1: emscripten_set_main_loop — Stack Objects Destroyed Before Loop Runs

**What goes wrong:**
Any object created on the stack in `main()` before `emscripten_set_main_loop()` is called will be destroyed when `simulate_infinite_loop = 0` and `main()` returns. The loop callback then holds dangling references to those destroyed objects. With `simulate_infinite_loop = 1`, Emscripten throws a longjmp-style exception to abort `main()` — code after the call never executes, silently skipping any cleanup or initialization intended to run post-call.

**Why it happens:**
Native game engines own the loop. Emscripten inverts control: `emscripten_set_main_loop` registers a callback that the browser calls once per frame via `requestAnimationFrame`. This means `main()` must hand off all state to static or heap-allocated objects before registering the callback.

**How to avoid:**
- All engine state (`LuaScriptSystem`, canvas arrays, bindings) must be static or dynamically allocated — never stack-local in `main()`.
- Use a static global `WasmRunner` struct that holds all state. Register the loop callback as a C function that calls into this struct.
- Call `emscripten_set_main_loop(loopCallback, 0, 1)` with `fps=0` (uses `requestAnimationFrame`) and `simulate_infinite_loop=1` as the final statement in `main()`.
- Never place initialization code after `emscripten_set_main_loop()` — it will not execute.

**Warning signs:**
- Segfault or null-pointer access immediately on first frame render
- Engine state appears reset every frame
- Initialization log messages appear but no rendering occurs

**Phase to address:** Emscripten/WASM build verification phase (first phase touching WASM main loop)

---

### Pitfall 2: ASYNCIFY Code-Size Explosion — Do Not Use It for Coroutine Wait

**What goes wrong:**
ASYNCIFY rewrites the entire WASM binary to make synchronous code suspendable. Without `-O3`, an already-large binary becomes 3-5x larger. Even with optimization, ASYNCIFY adds ~50% binary overhead. More critically: ASYNCIFY instruments every function in the call graph unless `ASYNCIFY_IMPORTS` and `ASYNCIFY_REMOVE` are explicitly configured — meaning it will instrument Lua VM internals, the entire bindings layer, and every engine system.

For enjin2's use case (coroutine `wait` implemented via `lua_yield`/`lua_resume` outside pcall scope), ASYNCIFY is not needed. The coroutine scheduler already yields at the C boundary correctly. Using ASYNCIFY as a shortcut for tween-await integration would cause binary bloat for no benefit.

**Why it happens:**
Developers see `emscripten_sleep()` described as "yield to the browser event loop" and assume it maps cleanly to `engine.async.wait()`. In reality, `emscripten_sleep` requires ASYNCIFY and pauses the entire WASM instance — not just the Lua coroutine. enjin2's coroutine scheduler uses `lua_resume` outside pcall, which is correct and does not need ASYNCIFY.

**How to avoid:**
- Do not add `-sASYNCIFY` to `emcmake cmake` flags.
- The existing `tickCoroutines()` + `lua_resume` pattern works in WASM as-is: the WASM main loop callback (registered via `emscripten_set_main_loop`) calls `tickCoroutines(dt)` once per frame, which advances timers and resumes suspended coroutines via `lua_resume`. This is equivalent to the SDL runner.
- If `engine.async.wait()` is called from inside the WASM main loop callback (not from inside a blocking `main()`), no ASYNCIFY is needed.
- Verify by running the async demo script through the WASM build — coroutines should resume on schedule without ASYNCIFY.

**Warning signs:**
- Build flags include `-sASYNCIFY` or `emscripten_sleep()` calls in the WASM host
- WASM binary exceeds 2MB for what should be a ~500KB build
- Build times increase 3-4x over the SDL build

**Phase to address:** WASM build verification phase

---

### Pitfall 3: ESP32 NVS Key Length Silently Truncates at 15 Characters

**What goes wrong:**
The NVS key name limit is exactly 15 ASCII characters. NVS does not return an error for keys longer than 15 characters — it silently truncates them. This means two keys like `"player_health_current"` and `"player_health_maximum"` both truncate to `"player_health_m"` and collide, causing one to silently overwrite the other.

LuaStore uses keys up to `STORE_MAX_KEY` characters (check current constant). If this constant is larger than 15, any LuaStore key over 15 characters will collide in NVS storage while appearing distinct in the in-memory store.

**Why it happens:**
The NVS spec requires 15-character maximum. LuaStore's in-memory key storage has its own constant (likely larger). When the NVS backend serializes keys, it truncates without warning.

**How to avoid:**
- Validate key length in the NVS backend adapter: `if (strlen(key) > 15) { ESP_LOGW(TAG, "NVS key truncated: %s", key); }` or return false immediately.
- Keep a NVS-specific namespace constant: `static constexpr int NVS_MAX_KEY = 15;`
- Document this limit in the Lua API: `engine.store.save("k", v)` — keys must be 15 characters or fewer on ESP32.
- Consider key hashing for long keys if needed, but simpler: enforce the limit and fail loudly during development.

**Warning signs:**
- Two distinct keys return the same value when loaded on ESP32 but different values on SDL3
- Saving one key silently overwrites another on embedded target
- `nvs_get_str` or `nvs_set_str` returns `ESP_ERR_NVS_NOT_FOUND` for a key that was just written

**Phase to address:** ESP32 NVS LuaStore backend phase

---

### Pitfall 4: NVS Namespace RAM Overhead — 22KB per 1MB Partition

**What goes wrong:**
NVS consumes approximately 22 KB of RAM per 1 MB of NVS flash partition and 5.5 KB of RAM per 1,000 keys. On an ESP32 with 320 KB internal RAM and a 5-layer canvas stack already consuming substantial static memory, opening a large NVS namespace can cause `ESP_ERR_NO_MEM` at runtime even if flash space is plentiful.

**Why it happens:**
NVS caches page metadata in heap. Opening a namespace involves loading page headers into RAM. The more pages in the NVS partition, the higher the RAM cost.

**How to avoid:**
- Use the minimum viable NVS partition size: 12 KB (3 pages) is the official minimum; 24–32 KB is sufficient for game save data with small values.
- Open the NVS namespace once at startup, keep the handle open for the session, and close it at shutdown. Do not open/close repeatedly per operation.
- Use `ESP_ERROR_CHECK(nvs_flash_init())` and check for `ESP_ERR_NVS_NO_FREE_PAGES` or `ESP_ERR_NVS_NEW_VERSION_FOUND` — both require `nvs_flash_erase()` followed by `nvs_flash_init()` again.
- Keep LuaStore key count small (≤16 keys matching `STORE_MAX_KEYS`) to avoid RAM growth.

**Warning signs:**
- ESP32 boot fails with `heap_caps_malloc` errors after `nvs_flash_init()`
- `esp_get_free_heap_size()` drops dramatically after store initialization
- `nvs_open()` returns `ESP_ERR_NVS_NOT_ENOUGH_SPACE`

**Phase to address:** ESP32 NVS LuaStore backend phase

---

### Pitfall 5: WASM localStorage Bridge — Synchronous Writes Block Render Frame

**What goes wrong:**
`localStorage.setItem()` in JavaScript is synchronous but relatively slow (10–100ms on cold writes for large strings). If the WASM-to-JS localStorage bridge calls `EM_ASM` or `emscripten_run_script` on every `engine.store.save()` call (auto-persist behavior), each save blocks the render frame. For LuaStore with auto-persist enabled (save-on-every-write), a script that calls `engine.store.save()` inside `update()` will drop frames every update cycle.

**Why it happens:**
The SDL3 LuaStore backend auto-persists to file on save (matching the current `engine.store.save` → `saveToFile()` pattern in `bindings_store.cpp`). Porting this directly to WASM means calling `localStorage.setItem()` synchronously on every save call. Unlike file I/O on desktop (which is fast for small files), browser storage operations have non-trivial overhead and jitter.

**How to avoid:**
- In the WASM backend, do NOT auto-persist on every `engine.store.save()`. Mark the store dirty and only flush on `engine.store.flush()` or on page unload (`window.beforeunload`).
- Implement `EM_JS` or `EM_ASM` flush function called only from `lua_engine_store_flush()` — not from `lua_engine_store_save()`.
- Serialize the entire LuaStore to a single JSON string and call `localStorage.setItem("enjin2_store", json)` once per flush, not per key.
- Register a `window.addEventListener("beforeunload", ...)` in the WASM JavaScript glue to call `module.flush()` when the user navigates away.

**Warning signs:**
- Frame time spikes from 16ms to 30–100ms whenever `engine.store.save()` is called in `update()`
- `localStorage.setItem` appears in browser DevTools performance flame chart inside the render frame
- Store data is inconsistent between page loads (partial writes due to mid-frame interruption)

**Phase to address:** WASM localStorage bridge phase

---

### Pitfall 6: localStorage Quota Exceeded — 5MB Browser Hard Limit

**What goes wrong:**
Browsers enforce a 5 MB per-origin localStorage quota. If a WASM game serializes the entire LuaStore as JSON (including embedded Lua table values, long strings, or repeated saves) the stored JSON can grow beyond the quota. `localStorage.setItem()` throws a `QuotaExceededError` DOMException in JavaScript, which, if not caught, bubbles up as an Emscripten exception and either crashes the WASM module or silently drops the save.

**Why it happens:**
LuaStore allows strings up to `STORE_MAX_STRING` bytes and tables up to `STORE_MAX_TABLE_ENTRIES` entries. A fully saturated store with maximum-length strings could easily exceed 5 KB, and while that is within quota, nested tables with long string values, or any future expansion of `STORE_MAX_KEYS`, pushes toward the limit. Additionally, if localStorage is shared with other data (e.g., Docusaurus PWA caching), the effective budget is smaller.

**How to avoid:**
- Wrap all `localStorage.setItem()` calls in a try/catch in the JavaScript glue:
  ```javascript
  try { localStorage.setItem(key, value); }
  catch(e) { console.error("enjin2: localStorage quota exceeded", e); }
  ```
- Return a boolean from the JS flush function back to WASM via `EM_JS` so `engine.store.flush()` can return `false` on quota failure.
- Keep LuaStore serialized JSON small: the 16-key / 64-char string limits make this naturally bounded (~2–3 KB max), well within quota.
- Do not store binary data or base64-encoded images in LuaStore.

**Warning signs:**
- `engine.store.flush()` returns false in WASM but true on SDL3
- Browser console shows `QuotaExceededError`
- Save data silently disappears between page refreshes

**Phase to address:** WASM localStorage bridge phase

---

### Pitfall 7: Tween Await Integration — Resuming a Coroutine from a Tween Callback

**What goes wrong:**
`engine.tween.to()` accepts an optional `done_cb` callback. If a coroutine calls `engine.tween.to()` and then attempts to `engine.async.wait()` on the tween completing (rather than a timeout), there is no native mechanism to resume the coroutine from inside the tween `done_cb`. The done callback fires via `lua_pcall` from `tickTweens()`. If the callback tries to `coroutine.resume()` a suspended coroutine, this is a re-entrant coroutine resume inside `tickTweens()` — undefined behavior that can corrupt the coroutine pool.

**Why it happens:**
`tickTweens()` iterates `m_tweenPool` linearly and calls `done_cb` via `lua_pcall`. `tickCoroutines()` runs before `tickTweens()` in the SDL runner (confirmed: `tickCoroutines(dt)` precedes `tickTweens(dt)` in `sdl_main.cpp`). A done callback that calls `coroutine.resume()` on a slot in `m_coroutinePool` tries to resume a coroutine from inside the tween tick loop — outside the scheduler's designed resume path.

**How to avoid:**
- The tween-await pattern (`engine.tween.await(...)`) must not use re-entrant coroutine resume. Instead, implement it as a yield + polling approach:
  - `engine.tween.await(target, props, duration, easing)` stores the tween ID and yields the current coroutine.
  - On each `tickCoroutines()` frame, before checking wait time, check if the awaited tween slot is still active. If not, clear the wait and resume.
- Alternatively: the `done_cb` sets a flag (e.g., into the coroutine's `waitRemaining` field set to 0) so the next `tickCoroutines()` naturally resumes it — without any re-entrant `coroutine.resume()` call.
- Never call `coroutine.resume()` from inside a tween `done_cb` that fires from `tickTweens()`.

**Warning signs:**
- Coroutine resumes twice in one frame (visible as double-stepping animation)
- `lua_resume` returns `LUA_ERRRUN` with "cannot resume running coroutine"
- Tween completes but coroutine never wakes up (if done_cb discards resume attempt silently)

**Phase to address:** Tween await QoL phase

---

### Pitfall 8: Camera Dead Zone Applied Before Lerp — Jitter at Zone Boundary

**What goes wrong:**
A naively implemented camera dead zone checks whether the target is outside the dead zone rectangle and, if so, snaps the camera target to the zone edge and applies lerp from the current camera position. If the dead zone check is applied before the lerp step, the effective lerp target jumps discontinuously every frame the target moves in/out of the zone boundary. This produces a visible stutter — the camera oscillates between "dead zone engaged" (no movement) and "dead zone disengaged" (sudden lerp pull) at the boundary.

**Why it happens:**
Developers treat the dead zone as a simple "if outside rect, follow; else stop" gate. But lerp follow requires a smooth target position — the dead zone should offset the lerp target (how much to pursue), not toggle the lerp on and off. When toggling, the camera position jumps one lerp-step's worth of distance every time the target crosses the boundary.

**How to avoid:**
- Compute the dead zone delta first: `clamp(target.x - camera.x, -deadZoneHalfW, deadZoneHalfW)` gives the displacement within the zone. Subtract this from the target to get the "pull target."
- Only begin lerp movement when the target is outside the dead zone. When inside, the "pull target" equals the current camera position — lerp has nothing to do.
- The C_Camera `update()` with dead zone should look like:
  ```
  float dx = target.x - m_x;
  float dy = target.y - m_y;
  float pullX = dx - clamp(dx, -m_deadZoneW/2, m_deadZoneW/2);
  float pullY = dy - clamp(dy, -m_deadZoneH/2, m_deadZoneH/2);
  m_x += pullX * m_lerpSpeed * dt * 10.0f;
  m_y += pullY * m_lerpSpeed * dt * 10.0f;
  ```
- Camera updates must run after object updates each frame (confirmed in SDL runner order).

**Warning signs:**
- Camera jitters when target hovers at the dead zone boundary
- Lerp speed feels inconsistent — fast when far, stuttery when near boundary
- Dead zone appears asymmetric (easier to escape from one side than the other)

**Phase to address:** Camera dead zone QoL phase

---

### Pitfall 9: Arch Linux emsdk/ESP-IDF Setup Script Breaks on Python Version Drift

**What goes wrong:**
ESP-IDF's `install.sh` and emsdk's `emsdk install` script both assume a stable Python version. Arch Linux's rolling release model means `python` can advance to a minor or major version (e.g., 3.12 → 3.13 → 3.14) at any `pacman -Syu`. ESP-IDF's install script has a known history of breaking on Arch (GitHub issue #7809). Symptoms include: `pip` refusing to install into the system Python (PEP 668 "externally managed" error since Python 3.11), virtual environment creation failures, and `idf.py` import errors for specific Python packages compiled against older ABI.

emsdk similarly creates a virtual environment and installs Python packages; if the system Python upgrades between installs, the venv's package hashes become invalid and `emsdk activate` fails silently.

**Why it happens:**
Arch tracks Python releases aggressively and does not maintain compatibility shims. Both ESP-IDF and emsdk depend on specific Python packages (`pyserial`, `kconfiglib`, `cryptography`, etc.) that may not be binary-compatible with the new Python ABI until upstream releases new wheels.

**How to avoid:**
- Pin the emsdk version: use `emsdk install 3.1.XX` (a specific tagged version) rather than `latest`. Record the version in the setup script.
- Use `python -m venv` explicitly for ESP-IDF: `python3 -m venv $IDF_PATH/.venv && source $IDF_PATH/.venv/bin/activate && pip install -r $IDF_PATH/requirements.txt`
- Add a Python version check at the top of the setup script: `python3 --version | grep -E "3\.(11|12|13)"` — warn if outside tested range.
- Do not use `sudo pip install` — always use a venv or `--user` installs.
- For emsdk: prefer the AUR `emsdk` package or install into a project-local directory and source `emsdk_env.sh` rather than modifying system paths.
- Add `set -e` to the setup script and explicit error messages for each step.

**Warning signs:**
- `pip install` fails with "externally-managed-environment" error
- `idf.py` fails to import `kconfiglib` or `pyparsing`
- `emcc --version` hangs or prints "emsdk: command not found" after successful install
- Setup script completes with exit code 0 but `emcc` and `idf.py` are not on PATH

**Phase to address:** Dev environment setup script phase

---

### Pitfall 10: ESP32 5-Layer Stack Exceeds Internal RAM Without PSRAM

**What goes wrong:**
enjin2 v1.7 ships with `ENJIN_LAYER_COUNT=5` (4 game layers + 1 debug layer). Each Canvas4 of 320×240 pixels at 4-bit (nibble) packing requires 320×240/2 = 38,400 bytes = ~37.5 KB per layer. Five layers = ~187.5 KB of canvas data. ESP32 has 320 KB internal RAM, with significant portions consumed by Lua VM state, Lua scripts, coroutine stacks, and system firmware overhead. A 5-layer stack may leave insufficient heap for Lua.

**Why it happens:**
enjin2 uses static allocation for canvases, so the canvas array is allocated at program start before Lua is initialized. If static arrays are placed in BSS (internal RAM by default on ESP32), the 187.5 KB canvas block may coexist with Lua's ~50–80 KB minimum heap and other static allocations, leaving less than 32 KB free heap — too little for reliable Lua operation.

**How to avoid:**
- Compile enjin2 with `ENJIN_LAYER_COUNT=2` or `ENJIN_LAYER_COUNT=3` for ESP32 targets by default (game layers only, no debug layer on embedded).
- If 5 layers are needed, place canvas arrays in PSRAM using `DRAM_ATTR` + `heap_caps_malloc` at init, or declare canvas buffers with `__attribute__((section(".spiram")))` if the linker script supports it.
- Add a compile-time static assert for ESP32 builds: `static_assert(ENJIN_LAYER_COUNT <= 3 || CONFIG_ESP32_SPIRAM_SUPPORT, "5-layer stack requires PSRAM on ESP32")`.
- The `lua_platform.cpp` already handles ESP32 memory via `heap_caps_malloc` — extend this to canvas allocation or document the ESP32 layer count constraint explicitly.

**Warning signs:**
- ESP32 fails to boot with heap panic or stack overflow
- Lua VM initialization returns null (`lua_newstate` fails)
- `esp_get_free_heap_size()` is below 30 KB after canvas initialization
- Build passes but device hangs on `lua_newstate()` call

**Phase to address:** ESP32 build verification phase

---

## Technical Debt Patterns

Shortcuts that seem reasonable but create long-term problems.

| Shortcut | Immediate Benefit | Long-term Cost | When Acceptable |
|----------|-------------------|----------------|-----------------|
| Auto-persist on every `store.save()` in WASM | Matches SDL3 behavior exactly | Drops frames; may hit localStorage quota on every update() call | Never for WASM — flush-only is correct |
| Using `emscripten_run_script` for JS bridge | Simple to write | No type safety, no error propagation, slow; deprecation risk | Never — use `EM_JS` or `EM_ASM_INT` instead |
| `ASYNCIFY` for coroutine wait in WASM | No scheduler changes needed | 50%+ binary bloat, slower WASM, instruments entire Lua VM | Never — the existing lua_resume pattern handles this correctly |
| Opening NVS namespace on every store operation | Simpler implementation | RAM fragmentation, handle leak if not closed, slower per-op | Never — open once at init, close at shutdown |
| Setup script installs to system Python (`sudo pip`) | Fewer venv steps | Breaks on every Arch Python update; violates PEP 668 | Never on Arch Linux rolling |
| Using `emscripten_set_main_loop` with `fps=60` | Feels predictable | Bypasses vsync/rAF; causes tearing and energy waste | Never — use fps=0 (rAF) |
| Hardcoding emsdk `latest` in setup script | Always installs newest | Breaks reproducibility; `latest` may be untested with Lua | Only acceptable for development; pin version for CI |
| Dead zone implemented as on/off lerp toggle | Easy to understand | Jitter at zone boundary; non-deterministic camera position | Never for smooth follow; use offset-clamp pattern instead |

---

## Integration Gotchas

Common mistakes when connecting these new features to the existing system.

| Integration | Common Mistake | Correct Approach |
|-------------|----------------|------------------|
| WASM main loop | Calling `emscripten_set_main_loop` before engine is initialized | Initialize all state as static globals, then register the loop callback as the last statement in `main()` |
| WASM LuaStore backend | Using the existing `saveToFile` stub (returns false) and treating it as correct behavior | Replace the stub with `EM_JS` bridge to `localStorage.setItem()` in a WASM-specific compilation unit |
| ESP32 NVS backend | Using `nvs_set_str` for LuaStore's serialized JSON blob | LuaStore's JSON may exceed 4000-byte NVS string limit; use `nvs_set_blob` for the store payload |
| Tween await | Yielding inside a `done_cb` via `coroutine.yield()` | Use a polling approach: yield the coroutine from `engine.async.wait`, check tween completion on each scheduler tick |
| Camera dead zone + screen shake | Applying dead zone check after screen shake offset | Dead zone operates on target-to-camera delta in world space; screen shake is a render-time offset applied after camera update |
| Arch setup script + emsdk | Sourcing `emsdk_env.sh` without checking if emsdk is initialized | Check `command -v emcc` before sourcing; print actionable error if not found |
| ESP32 build + bindings split | Including `bindings_internal.hpp` from a new NVS-specific file | `bindings_internal.hpp` uses TU-local constexpr — safe, but verify it compiles cleanly in the ESP32 toolchain (Xtensa-specific warnings) |
| Docusaurus C++ code blocks | Using `<` and `>` in angle-bracket template syntax in MDX | Escape as `&lt;` and `&gt;` in prose; in fenced code blocks (triple backtick), angle brackets are safe |

---

## Performance Traps

Patterns that work at small scale but fail under real conditions.

| Trap | Symptoms | Prevention | When It Breaks |
|------|----------|------------|----------------|
| `localStorage.setItem` on every `engine.store.save()` | Frame time spikes 10–100ms each save call | Flush-only pattern: dirty flag + explicit `flush()` | First time `save()` is called from `update()` |
| NVS `nvs_open`/`nvs_close` per operation | Each open costs ~1ms RAM load + flash read; noticeable lag | Open namespace handle once at boot; keep open | Immediately visible on ESP32 at any save frequency |
| `emscripten_set_main_loop` with `fps=60` instead of `0` | Frame tearing, energy waste, vsync mismatch | Always use `fps=0` for rAF-synchronized rendering | Immediately |
| 5-layer canvas stack in internal RAM on ESP32 | Boot failure or Lua heap exhaustion | Reduce layer count for ESP32; use PSRAM for canvas buffers | At boot, before any game code runs |
| Coroutine wait implemented with ASYNCIFY `emscripten_sleep` | WASM binary 50%+ larger; all VM calls are slower | Use `tickCoroutines(dt)` polling loop, not ASYNCIFY | Build time; noticeable in browser load performance |
| Dead zone toggling lerp on/off frame-by-frame | Camera jitter at zone boundary; visible oscillation | Use offset-clamp formula; lerp the pull delta, not a binary switch | When target velocity ≈ lerp speed at boundary |

---

## "Looks Done But Isn't" Checklist

Things that appear complete but are missing critical pieces.

- [ ] **WASM LuaStore:** `saveToFile` stub returns `false` — looks like "build passes" but store is silently dropped on every save. Verify by calling `engine.store.save("x", 1)` → reload page → `engine.store.load("x")` returns value.
- [ ] **ESP32 NVS keys:** Keys under 15 characters pass. Add explicit test with a 16-character key to confirm truncation warning fires, not silent collision.
- [ ] **WASM main loop:** Build succeeds, `.wasm` file generated, but running in browser shows blank canvas because state objects were stack-allocated in `main()`. Verify with a script that draws a colored rectangle and reloads — must persist across frame callbacks.
- [ ] **Camera dead zone:** Feature appears to work but has jitter at boundary. Test by moving target at constant low velocity across the dead zone edge — camera should track smoothly, not oscillate.
- [ ] **Tween await:** `engine.tween.await()` returns at end of tween but coroutine is resumed twice in one frame when done_cb and scheduler both trigger on same tick. Verify frame count matches expected tween duration exactly.
- [ ] **Arch setup script:** Script exits 0 on first run. Verify idempotency: run twice — second run should skip already-installed steps or reinstall cleanly without conflict.
- [ ] **ESP32 5-layer build:** Compiles without error. Verify `esp_get_free_heap_size()` after canvas init and Lua init leaves ≥ 40 KB free. Log this at boot.
- [ ] **localStorage quota:** Single-key store flushes correctly. Test with maximum-capacity store (16 keys, max-length strings) — verify flush returns true and data survives reload.
- [ ] **Docusaurus tutorials:** Renders in dev mode. Verify production build (`npm run build`) completes without MDX parse errors — angle brackets in prose, template syntax in code, and JSX-incompatible HTML are common build-time failures that don't appear in dev mode.

---

## Recovery Strategies

When pitfalls occur despite prevention, how to recover.

| Pitfall | Recovery Cost | Recovery Steps |
|---------|---------------|----------------|
| WASM state in stack-local `main()` | MEDIUM | Refactor engine state to static struct; no API changes needed; test in browser immediately |
| ASYNCIFY bloat already added | LOW | Remove `-sASYNCIFY` flag; rebuild; verify coroutines still work via `tickCoroutines` path |
| NVS key collision from truncation | HIGH | Existing saved data may be corrupted in flash; run `nvs_flash_erase()` + re-init; rename keys to ≤15 chars throughout LuaStore backend |
| ESP32 heap exhaustion from 5 layers | MEDIUM | Reduce `ENJIN_LAYER_COUNT` to 3 in ESP32 CMake preset; rebuild; verify Lua boots |
| localStorage quota error in production | LOW | Catch the DOMException in JS glue; return false to WASM; game handles gracefully if `flush()` return value is checked |
| Tween-coroutine double-resume | MEDIUM | Remove re-entrant `coroutine.resume()` from `done_cb`; implement polling check in `tickCoroutines`; clear tween ID from slot on completion |
| Camera dead zone jitter | LOW | Replace toggle pattern with offset-clamp formula; no API changes; unit-testable in isolation |
| Arch setup script Python breakage | MEDIUM | Add `python3 -m venv` isolation; test in a fresh Arch Docker container; document tested Python version range |

---

## Pitfall-to-Phase Mapping

How roadmap phases should address these pitfalls.

| Pitfall | Prevention Phase | Verification |
|---------|------------------|--------------|
| `emscripten_set_main_loop` stack object destruction | WASM build verification | Script draws rect, page reload preserves behavior |
| ASYNCIFY bloat | WASM build verification | Build flags reviewed; binary size baseline documented |
| NVS key 15-char limit | ESP32 NVS storage phase | Test with 16-char key; confirm warning/rejection |
| NVS namespace RAM overhead | ESP32 NVS storage phase | Log heap after `nvs_flash_init()`; must leave ≥40 KB |
| localStorage frame-blocking | WASM localStorage bridge phase | Profile with browser DevTools; no spikes in `update()` |
| localStorage quota exceeded | WASM localStorage bridge phase | Test with max-capacity store; flush returns bool correctly |
| Tween-coroutine re-entrant resume | Tween await QoL phase | Coroutine wakes exactly once per tween; frame timing matches duration |
| Camera dead zone jitter | Camera dead zone QoL phase | Target at constant velocity across boundary; smooth tracking |
| Arch Python version breakage | Dev setup script phase | Run script on fresh Arch install; run twice for idempotency |
| ESP32 5-layer heap exhaustion | ESP32 build verification phase | `esp_get_free_heap_size()` logged at boot; ≥40 KB after init |

---

## Sources

- [Emscripten emscripten_set_main_loop documentation](https://emscripten.org/docs/api_reference/emscripten.h.html)
- [Emscripten Runtime Environment — main loop design](https://emscripten.org/docs/porting/emscripten-runtime-environment.html)
- [Emscripten Asyncify documentation](https://emscripten.org/docs/porting/asyncify.html)
- [Emscripten File System API — MEMFS and IDBFS](https://emscripten.org/docs/api_reference/Filesystem-API.html)
- [ESP-IDF NVS Flash API Reference (v5.5.3)](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/storage/nvs_flash.html)
- [ESP-IDF External RAM (PSRAM) Guide](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-guides/external-ram.html)
- [ESP-IDF install.sh breakage on Arch Linux — GitHub Issue #7809](https://github.com/espressif/esp-idf/issues/7809)
- [ArchWiki ESP32 page](https://wiki.archlinux.org/title/ESP32)
- [Emscripten Asyncify + coroutines broken issue #8979](https://github.com/emscripten-core/emscripten/issues/8979)
- [Emscripten set_main_loop multiple calls issue #2325](https://github.com/emscripten-core/emscripten/issues/2325)
- [LocalStorage vs IndexedDB vs OPFS vs WASM-SQLite — RxDB 2025](https://rxdb.info/articles/localstorage-indexeddb-cookies-opfs-sqlite-wasm.html)
- [Lua coroutine yield-across-C-boundary — lua-l archive](https://lua-l.lua.narkive.com/uMsdDdA3/attempt-to-yield-across-metamethod-c-call-boundary)
- [Improved Lerp Smoothing — Game Developer Magazine](https://www.gamedeveloper.com/programming/improved-lerp-smoothing-)
- [Docusaurus code blocks documentation](https://docusaurus.io/docs/markdown-features/code-blocks)
- enjin2 v1.7 codebase: `src/scripting/bindings_store.cpp`, `bindings_async.cpp`, `bindings_tween.cpp`, `components/camera.cpp`, `platform/sdl/sdl_main.cpp`, `src/bindings/emscripten_bindings.cpp`
- enjin2 PROJECT.md — Known tech debt and constraints

---
*Pitfalls research for: enjin2 v1.8 Ship Ready — Emscripten/WASM, ESP32 NVS, WASM localStorage, Arch dev scripts, coroutine-tween, camera dead zone, Docusaurus tutorials*
*Researched: 2026-03-02*
