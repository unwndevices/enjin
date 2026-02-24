# Pitfalls Research

**Domain:** Adding multi-layer Canvas4 composition, sprite system rework, Lua hot reload, and Docusaurus MDX navigation fix to enjin2
**Researched:** 2026-02-24
**Confidence:** HIGH (based on direct codebase analysis of all relevant source files)

---

## Critical Pitfalls

### Pitfall 1: Linear-Bump Allocator Leaks on Hot Reload — luaAllocator Cannot Free Into Gaps

**What goes wrong:**
`LuaEngine::luaAllocator` is a linear bump allocator over a static `char memoryPool[MEMORY_LIMIT]`. Allocations advance `memoryUsed` forward; the "free" path only decrements `memoryUsed` by `osize` — there is no ability to reclaim holes left by freed blocks in the middle of the pool. After `lua_close()` followed by a new `lua_newstate()` (the hot-reload sequence), `memoryUsed` is reset to 0 in `initialize()`, which is correct. However, there is a window between `shutdown()` and `initialize()` where `memoryUsed` is 0 but `L` has already been closed. If any pointer into `memoryPool` is held elsewhere (e.g., `LuaBindings` caching a `lua_State*` raw pointer, or a C closure holding a stale `LuaCallback` address), dereferencing it after `shutdown()` causes undefined behavior.

A second problem: `registerFunction(const std::string& name, LuaCallback callback)` stores a pointer to `callback` — a local parameter — into the Lua registry via `lua_pushlightuserdata(L, reinterpret_cast<void*>(&callback))`. The local `callback` object is destroyed at function exit. The Lua closure then holds a dangling pointer. This is already a latent bug; hot reload exposes it because the Lua state is recreated and bindings are re-registered, increasing the chance of the dangling-pointer closure being called.

**Why it happens:**
Hot reload feels straightforward: call `shutdown()`, call `initialize()`, re-register bindings, re-load the script. The allocator reset is in `initialize()`, so it appears clean. The dangling-pointer closure bug was not visible in single-load operation because the stale pointer was never called after the LuaEngine object was destroyed (the program exits before that point).

**How to avoid:**
1. Fix the `LuaCallback` registration before implementing hot reload: store the callback in a stable collection (e.g., `std::vector<LuaCallback>` member of `LuaEngine`) and store a pointer into that collection — not a pointer to a local. Alternatively, use `lua_CFunction` exclusively in bindings (already the dominant pattern in `bindings.cpp`).
2. For hot reload: after `shutdown()`, immediately null `g_currentBindings` and any cached `lua_State*` pointers. The SDL main loop must not call `g_lua.callFunction()` between shutdown and the new initialize completing.
3. Zero the memory pool in `shutdown()` (not just in `initialize()`) to surface use-after-free as a crash rather than silent corruption.

**Warning signs:**
- `registerFunction` overload taking `LuaCallback` (std::function) is used in hot-reload path
- Any code that caches `engine->getState()` across a shutdown/initialize cycle
- `g_currentBindings` not set to nullptr before `g_lua.shutdown()`
- Hot reload triggered mid-frame (between `callFunction("update")` and `callFunction("draw")`)

**Phase to address:**
Lua hot reload phase — fix the dangling-callback bug as step one before wiring F5 reload.

---

### Pitfall 2: Four Canvas4<128,128> Buffers Simultaneously Allocated Exceed ESP32 SRAM

**What goes wrong:**
`Canvas4<128, 128>` has `BUFFER_SIZE = (128 * 128) / 2 = 8192` bytes. Four independent Canvas4 buffers for a 4-layer compositor consume `4 * 8192 = 32,768 bytes` of SRAM as static globals. The ESP32-S3 has 512 KB of SRAM total, but a significant portion is consumed by the Arduino/ESP-IDF stack, WiFi/BT buffers, heap overhead, and the existing Lua pool (64 KB configured in `LuaPlatformConfig::MEMORY_LIMIT`). The existing RGB staging buffer in `sdl_main.cpp` is `128 * 128 * 3 = 49,152 bytes`. Adding four canvas layers on top of the Lua pool, RGB staging, and system overhead likely causes heap exhaustion or stack overflow on ESP32.

The compositing step also needs a scratch buffer: blitting four layers into a final output requires either an additional full-canvas scratch or doing the composite in-place, which is not possible without careful ordering. If a scratch buffer is added, the total rises to `5 * 8192 = 40,960 bytes` for canvases alone.

**Why it happens:**
The SDL3 desktop runner has gigabytes of memory — four canvas buffers are imperceptible. Developers test on SDL3, the compositor works perfectly, and the ESP32 impact is not considered until hardware integration.

**How to avoid:**
- Declare the 4-layer static buffer array in a platform-aware section (`DRAM_ATTR` on ESP32, or in external PSRAM via `EXT_RAM_BSS_ATTR` if the ESP32-S3 board has PSRAM).
- Do not allocate all 4 layers unconditionally. Allow the layer count to be a compile-time template parameter or a CMake option: desktop can default to 4, ESP32 can use 2 or fewer.
- Compute the SRAM budget before writing the layer code: `4 * 8192 (layers) + 64*1024 (Lua) + existing system overhead` and verify it fits.
- The RGB staging buffer in `sdl_main.cpp` (49 KB) is SDL3-only and not present on ESP32, which helps.

**Warning signs:**
- Layer buffers declared as `static Canvas4<W, H> g_layers[4]` unconditionally in a shared header
- No `EXT_RAM_BSS_ATTR` or PSRAM annotation on layer arrays for ESP32
- No compile-time layer count option — hardcoded 4 everywhere
- ESP32 firmware flashes but crashes at boot (stack overflow symptom) or throws `abort() was called` (heap exhaustion)

**Phase to address:**
Multi-layer design phase — compute memory budget and define the compile-time layer count constant before writing any compositing code.

---

### Pitfall 3: Layer System Cannot Use Polymorphism to Hold Canvas4<W,H> — Template Mismatch

**What goes wrong:**
`Canvas4<W, H>` is a template class. A layer compositor needs to hold multiple canvas instances and iterate over them. The natural C++ approach is a base class pointer array: `ICanvas<Pixel4>* layers[4]`. This works because `Canvas4<W, H>` inherits `ICanvas<Pixel4>`. However, the compositor's compositing function needs access to `getBuffer()` for bulk operations (not just `getPixel()`), and `getBuffer()` is not part of `ICanvas<Pixel4>` — it is defined only on the concrete `Canvas4<W, H>` template.

If the compositor is written to use only the `ICanvas<Pixel4>` interface, compositing devolves to per-pixel `getPixel()`/`setPixel()` calls — `128 * 128 * 4 = 65,536` calls per frame just for compositing. At 30 Hz that is nearly 2 million virtual dispatch calls per second, which is measurable overhead on both ESP32 and WASM.

If the compositor downcast to `Canvas4<128, 128>*` to access `getBuffer()`, it is hardcoded to a single canvas size, defeating the template design.

**Why it happens:**
The `ICanvas<Pixel4>` interface was designed for drawing operations, not bulk buffer access. Adding buffer access to the interface would require a non-template return type (e.g., `const uint8_t*` plus a size accessor), which is a design change to an already-stable interface.

**How to avoid:**
Add `virtual const uint8_t* getRawBuffer() const = 0` and `virtual size_t getRawBufferSize() const = 0` to `ICanvas<TPixel>` (or to a separate `IBufferCanvas` intermediate interface). Both `Canvas4` and `Canvas8` implement these. The compositor uses `getRawBuffer()` for fast `memcpy`-based blending. This avoids per-pixel virtual dispatch while keeping the interface polymorphic. Alternatively, make the compositor itself a template on `<uint16_t W, uint16_t H>` — all canvases are the same size, so template instantiation is exact and no downcast is needed.

**Warning signs:**
- Compositor iterates `getPixel()`/`setPixel()` across all layers (per-pixel dispatch on WASM/ESP32)
- Compositor stores `ICanvas<Pixel4>*` but calls `static_cast<Canvas4<128,128>*>` to get buffer
- No bulk buffer access in `ICanvas<TPixel>` or `Canvas4`
- Compositor templated on a specific size `<128, 128>` hard-coded in the compositor header

**Phase to address:**
Multi-layer design phase — decide the interface extension before writing the compositor.

---

### Pitfall 4: Sprite Rework Breaks All Existing Code Using Old Sprite API

**What goes wrong:**
The existing `Sprite` class (in `include/enjin2/graphics/sprite.hpp`) uses `ICanvas<uint8_t>` (8-bit canvas) as its draw target, not `ICanvas<Pixel4>` (4-bit canvas). The current `Canvas4` is a 4-bit canvas. The existing `Sprite::draw()` method calls `canvas.setPixel(x, y, pixel)` where `pixel` is a `uint8_t` value from the sprite data. On `ICanvas<Pixel4>`, `setPixel` requires a `Pixel4`, not a `uint8_t`. This is a type mismatch that will cause compile errors the moment any reworked sprite tries to draw to the main Canvas4 framebuffer.

Separately, the existing `Sprite` has both private and public versions of all its member fields (`width`/`_width`, `height`/`_height`, etc.) — clear evidence of an incomplete migration from a legacy API. Any code that accessed `sprite._width` directly will silently stop working if the rework removes the legacy `_` prefixed members.

**Why it happens:**
The sprite was carried over from enjin1 compatibility headers and never fully adapted to the 4-bit pipeline. The `uint8_t` canvas interface works for 8-bit greyscale (256 levels) but not for the 4-bit indexed pipeline (16 palette entries). The sprite's `matte` field defaults to 16, which is outside the 0–15 valid range for Pixel4 — the transparency convention is already inconsistent.

**How to avoid:**
- Define the new `Sprite` API to target `ICanvas<Pixel4>` explicitly, using `Pixel4` pixel values (0–15) and `PALETTE_TRANSPARENT` (15) as the transparency sentinel — consistent with the established convention.
- Remove the legacy `_`-prefixed public members in the rework. Do a codebase grep for `_width`, `_height`, `_frame`, `_position`, `_matte`, `_mode` before removing them to catch all callers.
- The existing `Sprite::draw(ICanvas<uint8_t>& canvas)` signature is a compile-time breakage, not a runtime one — it will fail to compile against Canvas4. Treat this as a hard API break and update all callers at the same time as the rework.
- `matte` default of `16` must change to `15` (PALETTE_TRANSPARENT).

**Warning signs:**
- Sprite draw method still accepts `ICanvas<uint8_t>&` after rework
- `matte` default value is 16 (out-of-range for Pixel4)
- Legacy `_`-prefixed public fields still present after rework
- Any code calling `sprite.draw(canvas)` where `canvas` is a `Canvas8` (8-bit) instance — confirms old API dependency

**Phase to address:**
Sprite rework phase — this is the primary concern of that phase. Treat it as a clean break.

---

### Pitfall 5: LuaCanvas Type-Erasure Breaks With Per-Layer Canvas Routing

**What goes wrong:**
`LuaCanvas` is a type-erased wrapper around either `ICanvas<Pixel4>*` or `ICanvas<uint8_t>*`, stored as `void* canvasPtr` with a `bool is4Bit` discriminator. Currently there is one `LuaCanvas` wrapping one `Canvas4<128, 128>` in the SDL3 runner and one per WASM canvas instance.

With multi-layer composition, Lua scripts need to target specific layers: `setLayer(1)`, draw to it, then `setLayer(0)`. This means `LuaBindings::currentCanvas` must be swappable to point to different `LuaCanvas` instances during a single frame. There is nothing inherently wrong with this — but the existing `LuaCanvas` constructor template `LuaCanvas(Canvas4<W, H>* canvas)` is defined inline in the header; each distinct `<W, H>` instantiation generates a different vtable-free wrapper. If all 4 layers are the same size (they will be — `Canvas4<128, 128>`), there is only one template instantiation, so it works.

However, `LuaCanvas` stores `void*` and casts based on `is4Bit`. If a future layer has a different size (hypothetical), the void-pointer cast is silently wrong — `static_cast<ICanvas<Pixel4>*>(canvasPtr)` with an `ICanvas<Pixel4>` pointing to a `Canvas4<64, 64>` is fine polymorphically, but the void pointer cast loses the concrete type and the virtual dispatch still works. This is safe as long as no raw buffer access is performed through the void pointer.

The real risk is: if the layer API adds a Lua function `getLayerBuffer(n)` for direct buffer access, it must NOT go through the void-pointer path — it must downcast cleanly.

**Why it happens:**
The type-erasure via `void*` was a practical shortcut to support both 4-bit and 8-bit canvases without templates in the Lua binding layer. It is fragile by design and the fragility grows as new capabilities (layer routing, direct buffer access) are added.

**How to avoid:**
- Keep all layer canvases as `Canvas4<128, 128>` (same size). One `LuaCanvas` wrapper per layer, all `is4Bit = true`.
- For Lua layer switching, maintain a `LuaCanvas* layerCanvases[4]` array in `LuaBindings` and a `currentLayerIndex` integer. `setLayer(n)` sets `currentCanvas = &layerCanvases[n]`.
- Do not expose raw buffer pointers through `LuaCanvas`. Layer blending is done in C++, not Lua.

**Warning signs:**
- `LuaCanvas` void pointer cast used to access per-layer buffer data
- `setLayer()` implemented by replacing `LuaCanvas*` with a freshly allocated (dynamic allocation forbidden) wrapper
- Any `new LuaCanvas(...)` in the hot path

**Phase to address:**
Multi-layer Lua API phase — design the setLayer/getLayer API before implementing the compositor.

---

### Pitfall 6: Hot Reload Triggers Mid-Frame — draw() and update() Called on Invalid Lua State

**What goes wrong:**
The SDL3 game loop in `sdl_main.cpp` calls `g_lua.callFunction("update", dt)` then `g_lua.callFunction("draw")` every frame. Hot reload (F5) is detected as a key event in the event pump, which runs at the start of the frame before update/draw. If F5 triggers `shutdown()` and `initialize()` inline during the event pump, the update and draw calls later in the same frame execute against the freshly initialized (but not yet script-loaded) Lua state — `callFunction("update")` returns "Function not found" because the script has not been loaded yet.

Worse: if shutdown/initialize spans the boundary between event pump and the render calls, the sequence becomes: event pump (processes F5, calls shutdown, calls initialize, calls loadScript) → update (succeeds) → draw (succeeds) — which is fine. But if any of these fail and the error path calls `g_canvas.clear(Pixel4(14))` directly, the canvas is cleared without a draw call, producing a single-frame flash of the error color.

**Why it happens:**
SDL3 key events are natural triggers for reload actions. Developers put the reload logic inside the event handling switch, which runs in the middle of frame setup. The frame is not yet "started" from the game loop's perspective, but the canvas and Lua state have already been touched.

**How to avoid:**
Use a `bool g_reload_requested` flag. Set it true on F5 key event. At the top of the next frame (before input advance, before update/draw), check the flag and perform the full reload sequence. This ensures reload happens at a clean frame boundary: event pump → [reload if flagged] → input advance → update → draw → render. Never call `shutdown()` inside the event pump.

**Warning signs:**
- `g_lua.shutdown()` called directly inside `SDL_EVENT_KEY_DOWN` handler
- No `reload_requested` flag pattern
- Error path directly calling `g_canvas.clear()` from within the reload sequence (bypasses Lua draw)
- Script file path hardcoded — reload always reloads the same file with no mechanism to discover it

**Phase to address:**
Lua hot reload phase — the frame-boundary flag pattern is the first design decision.

---

### Pitfall 7: Docusaurus `type: 'docSidebar'` with `docsPluginId: 'api'` Requires Exact sidebarId Match

**What goes wrong:**
The Docusaurus config declares an API Reference navbar item with `docsPluginId: 'api'` and `sidebarId: 'apiSidebar'`. The `api-sidebar.js` exports `{ apiSidebar: [{ type: 'autogenerated', dirName: '.' }] }`. If any generated markdown file inside `docs/api/` contains unescaped angle brackets (`<` or `>`) in body text — not inside code blocks — the MDX compiler treats them as JSX tags and fails to parse the file. A single broken file causes the entire `autogenerated` sidebar to fail, disabling the "API Reference" nav link for all pages.

The v1.3 Sprite and Canvas docs already contain C++ template type references in plain text: `ICanvas<TPixel>`, `Canvas4<WIDTH, HEIGHT>`, `ICanvas<uint8_t>`. The Sprite rework and layer addition will introduce more: `LayerCompositor<N>`, `SpriteSheet<W, H, F>`. Each of these will appear in generated API docs as unescaped angle brackets in prose sections.

**Why it happens:**
The Doxygen-to-Markdown generator (`generate-api-docs.js`) processes XML and writes prose text containing C++ template syntax. C++ templates use angle brackets which are valid in HTML (escaped as `&lt;`/`&gt;`) and valid in Markdown code spans, but NOT valid as bare `<` in MDX outside a code block. MDX parses files as JSX, so `ICanvas<TPixel>` looks like an unclosed JSX tag `<TPixel`.

**How to avoid:**
- In `generate-api-docs.js`, escape all template angle brackets in prose sections: replace `<` with `\<` (MDX escape) or wrap the entire type reference in backticks (code span). Specifically target: parameter descriptions, `@tparam` content, brief descriptions containing type names.
- Do not place template type references outside of code blocks or inline code spans in handwritten docs.
- After adding new API files (post-sprite rework, post-layer addition), run `npm run build` in the docs directory to catch MDX parse errors before they reach CI. Add this as a verification step in the phase plan.
- The `markdown: { format: 'detect' }` config in `docusaurus.config.js` means files are auto-detected as MDX or CommonMark based on content. Files with JSX-like syntax are processed as MDX. Changing this to `format: 'mdx'` explicitly or `format: 'md'` (CommonMark) for the API plugin would eliminate the issue — but changing to CommonMark disables all MDX features in those files, which is acceptable for auto-generated API docs.

**Warning signs:**
- API Reference navbar link goes to 404 or blank page after `npm run build`
- Docusaurus build log contains `MDXError` or `SyntaxError: Unexpected token` for any file in `docs/api/`
- `generate-api-docs.js` outputs `ICanvas<` or `Canvas4<` without backtick wrapping
- New template class added to codebase, API regenerated, build not verified before commit

**Phase to address:**
Docusaurus navigation fix phase — this is the root cause of the carried-forward navigation breakage.

---

### Pitfall 8: Compositing Transparent Index 15 Across Layers — Wrong Merge Semantics

**What goes wrong:**
The transparency convention is: index 15 is transparent, indices 0–14 are opaque colors. When compositing 4 layers from bottom to top, the correct rule is: for each pixel position, take the topmost layer that has a non-transparent (non-15) pixel. If all layers have index 15 at position (x,y), the final pixel is transparent (renders as black in SDL3, passes through in WASM).

A common implementation mistake is to composite by checking `pixel != 0` (transparent check against black, not against index 15). This causes black pixels drawn intentionally by scripts to be treated as transparent and "punched through" to lower layers, making it impossible to draw solid black in any layer.

A second mistake: clearing a layer with `layer.clear(Pixel4(15))` (correct — marks all pixels transparent) vs. `layer.clear(Pixel4(0))` (incorrect — marks all pixels as color 0/black, which then blocks lower layers). The `Canvas4::clear()` default is `Pixel4(0)`, not `Pixel4(15)`. Layer initialization must use `Pixel4(15)` explicitly.

**Why it happens:**
`Pixel4(0)` looks like "zero/empty/transparent" intuitively. The `Canvas4::clear()` default reinforces this by clearing to `Pixel4(0)`. Developers who don't internalize the transparency convention assume 0 is transparent, which contradicts `PALETTE_TRANSPARENT = 15`.

**How to avoid:**
- The layer compositor's `clearLayer(n)` function must default to `Pixel4(PALETTE_TRANSPARENT)`, not `Pixel4(0)`.
- The compositing loop: `if (pixel.value != PALETTE_TRANSPARENT) { result = pixel; break; }` — explicitly use the named constant, never magic 0.
- In Lua, the `setLayer(n)` API should auto-clear the layer to transparent on activation (or document that users must call `clear(15)` themselves at the top of each draw call).
- Add a named Lua constant `TRANSPARENT = 15` so script authors do not use magic numbers.

**Warning signs:**
- Compositor checks `pixel.value != 0` for transparency
- Layer clear called with default `Pixel4(0)` instead of `Pixel4(15)`
- No `TRANSPARENT` constant in Lua bindings
- Black pixels "bleed through" from upper to lower layers in test scripts

**Phase to address:**
Multi-layer compositor implementation — transparency semantics defined in the compositor spec before writing the blend loop.

---

### Pitfall 9: SDL3 Texture Update Pitch Mismatch With Multi-Layer Composite

**What goes wrong:**
`sdl_main.cpp` calls `SDL_UpdateTexture(texture, nullptr, g_rgb_staging, CANVAS_W * 3)` where the pitch is `CANVAS_W * 3` (3 bytes per RGB24 pixel, no padding). This is correct for a single canvas. After adding multi-layer compositing, the composite step writes the final merged pixel data into `g_rgb_staging` (or a new composite buffer), then calls `SDL_UpdateTexture`. If the composite step produces a buffer with different padding or alignment (e.g., 4-byte aligned rows for SIMD), the pitch argument must be updated to match.

A subtler issue: the existing `expand_canvas_to_rgb()` function reads from a single `g_canvas`. After compositing, there is no longer a single authoritative canvas — the compositor produces the final pixel data. If the composite is not integrated into `expand_canvas_to_rgb()`, the function still reads only `g_canvas` (layer 0), ignoring layers 1–3. This is a silent correctness error: the SDL3 window renders only layer 0.

**Why it happens:**
`expand_canvas_to_rgb()` and the SDL update call are in `sdl_main.cpp` and feel like "render plumbing" that is separate from the graphics feature. Developers add the compositor to the graphics module but forget to update the SDL runner's render path to use the composite output.

**How to avoid:**
- Replace `expand_canvas_to_rgb()` with a `composite_and_expand_to_rgb()` function that iterates layers top-to-bottom and writes the winning pixel (non-transparent, topmost layer) into `g_rgb_staging`.
- This function is called from the SDL3 runner's render step instead of the old single-canvas expansion.
- On WASM: `getCanvasData128()` in `emscripten_bindings.cpp` similarly exposes only a single canvas buffer. After layer composition, the WASM binding must expose the composited output, not the raw layer-0 buffer.
- Pitch: keep RGB24 (3 bytes per pixel, no padding) — do not change the texture format. This avoids re-examining the pitch arithmetic.

**Warning signs:**
- `expand_canvas_to_rgb()` not modified when layer compositor added
- SDL3 window shows only layer 0 content regardless of what other layers contain
- WASM `getCanvasData128()` still reads `g_canvas` (single buffer) after layer system added
- Any SIMD or alignment optimization in the composite step that changes row stride

**Phase to address:**
Multi-layer compositor integration phase — update SDL3 renderer and WASM bindings in the same phase as the compositor.

---

### Pitfall 10: Emscripten Cannot Compile std::function Closures in Some Configurations

**What goes wrong:**
`LuaEngine::registerFunction(const std::string& name, LuaCallback callback)` stores a `std::function<int(lua_State*)>` as an upvalue via `lua_pushlightuserdata`. Emscripten (WASM target) has restrictions on C++ exception handling, `longjmp`, and some stdlib features when compiled without `-fexceptions`. Lua's error handling uses `longjmp` internally, and wrapping a Lua call inside a `std::function` creates a C++ object on the call stack that may have non-trivial destructor calls in the unwind path.

More concretely: if the Emscripten build uses `-fno-exceptions` (common for size optimization), and Lua errors propagate via `longjmp` through a stack frame that contains a `std::function` (which has a destructor), the destructor is not called — this is technically undefined behavior in C++, though Emscripten/LLVM may handle it without visible failure. However, it means the `std::function` memory tracking (reference counting inside `shared_ptr`-based std::function implementations) is skipped, potentially leaking the closure allocation.

**Why it happens:**
The `LuaCallback` typedef exists for flexibility. The SDL3 runner works fine (native C++ with exceptions). The WASM build has different compiler flags. The divergence is not visible until the WASM build is actually exercised with Lua callbacks registered via the `std::function` overload.

**How to avoid:**
- Avoid the `LuaCallback` (std::function) overload of `registerFunction` in any code that runs on WASM. The `lua_CFunction` (plain C function pointer) overload has no RAII concerns and is safe across all platforms.
- All bindings in `bindings.cpp` already use `lua_CFunction` via `engine->registerFunction("name", lua_staticFunc)`. Keep this pattern; do not introduce `LuaCallback`-registered functions in the layer or sprite Lua APIs.
- Mark the `LuaCallback` overload of `registerFunction` as `SDL3-only` in a comment to prevent future use on embedded/WASM targets.

**Warning signs:**
- New Lua bindings (layer API, sprite API) registered via `registerFunction(name, [capture lambda])` syntax
- WASM build using `-fno-exceptions` (check `CMakeLists.txt` `enjin2_wasm` target flags)
- Any lambda with captures registered as a Lua callback

**Phase to address:**
Multi-layer Lua API phase and sprite Lua API phase — establish convention to use lua_CFunction only.

---

## Technical Debt Patterns

Shortcuts that seem reasonable but create long-term problems.

| Shortcut | Immediate Benefit | Long-term Cost | When Acceptable |
|----------|-------------------|----------------|-----------------|
| Hardcode 4 layers as `Canvas4 g_layers[4]` unconditionally | Simple, works on SDL3 | Exceeds ESP32 SRAM; compile-time constant cannot be tuned per-platform | Never for unconditional declaration; use `#ifdef` or CMake option |
| Per-pixel `getPixel()`/`setPixel()` compositing | Correct, simple code | 65K virtual calls per composite on WASM/ESP32 at 128x128 | Never for the main compositor; only acceptable for debugging |
| Clear layers with `Pixel4(0)` instead of `Pixel4(PALETTE_TRANSPARENT)` | Matches existing clear() default | Black pixels punch through layers; transparency semantics broken | Never |
| Keep old `Sprite` API alongside new API with deprecation shims | No breaking change | Two parallel APIs diverge; shims rot; cognitive overhead doubles | Only if existing Sprite users cannot be migrated in the same milestone |
| Trigger hot reload inside SDL event handler (not at frame boundary) | Faster perceived response | Partial-frame Lua state: update called on fresh unloaded Lua | Never |
| Use `LuaCallback` (std::function) for new Lua binding registrations | Closures with captures | Dangling-pointer bug on reload; WASM/ESP32 exception interaction | Never for production bindings; only for one-off desktop-only tools |
| Skip `npm run build` after regenerating API docs | Faster iteration | MDX parse errors silently break entire API sidebar on GitHub Pages | Never before milestone completion |

---

## Integration Gotchas

Common mistakes when connecting the new features to the existing system.

| Integration | Common Mistake | Correct Approach |
|-------------|----------------|------------------|
| Layer compositor + SDL3 renderer | Forget to update `expand_canvas_to_rgb()` to read composite output | Replace expansion function with composite-then-expand; update SDL3 render step |
| Layer compositor + WASM | `getCanvasData128()` still reads `g_canvas` directly | Expose composited buffer via new WASM binding or update existing function |
| Lua hot reload + LuaBindings | `g_currentBindings` not cleared before shutdown; stale pointer | Set `g_currentBindings = nullptr` in shutdown sequence; re-assign in initialize |
| Lua hot reload + allocator | Reload sequence called mid-frame (after event pump, before update) | Use `reload_requested` flag; execute reload at clean frame boundary |
| Sprite rework + Canvas4 | Old `Sprite::draw(ICanvas<uint8_t>&)` fails to compile against Canvas4 target | New Sprite targets `ICanvas<Pixel4>` exclusively; update all callers |
| Sprite rework + transparency | Sprite `matte` default of 16 is outside Pixel4 valid range | New API uses `PALETTE_TRANSPARENT = 15` as default; remove legacy `_`-prefixed fields |
| Layer Lua API + LuaCanvas | Adding setLayer() requires routing `currentCanvas` to different layer wrappers | Pre-allocate one `LuaCanvas` per layer as static; switch pointer by index |
| Docusaurus API docs + template types | Regenerated docs for new template classes contain bare `<` in prose | Escape angle brackets in `generate-api-docs.js` for all template parameter text |
| Docusaurus MDX + new API modules | New `layers` or `sprites` module added to API; sidebar auto-generates but build not verified | Run `npm run build` after every `generate-api-docs.js` run; fix MDX issues before commit |

---

## Performance Traps

Patterns that work on SDL3 desktop but fail on ESP32 or WASM.

| Trap | Symptoms | Prevention | When It Breaks |
|------|----------|------------|----------------|
| Per-pixel virtual dispatch in compositor | Visible frame drops; cannot sustain 30 Hz at 128×128 with 4 layers | Use `getRawBuffer()` for bulk memcpy-based blending; no per-pixel virtual calls | ESP32 (~240 MHz, single-core Lua loop); WASM if called 60+ fps |
| Composite scratch buffer as a 5th static canvas | 5 × 8192 = 40960 bytes for canvases; exceeds ESP32 budget when added to Lua pool | Composite in-place top-to-bottom into layer 0 output buffer; no scratch needed | ESP32 with Lua pool (64KB) + 5 canvas buffers + system overhead |
| `std::string` usage in hot reload path | Fine on SDL3; `std::string` is heap-allocated; forbidden if allocator is bump pool | Use `const char*` with fixed-size buffers for script paths; avoid `std::string` in reload function | ESP32 if `std::string` triggers malloc outside the Lua pool |
| Full Lua state recreation on every hot reload | Acceptable 10-50 ms startup delay in SDL3 | This is correct behavior for hot reload; do not optimize away the shutdown/reinitialize cycle | Never — full recreation is mandatory for clean state |
| Emscripten `emscripten_force_exit()` in error path | Terminates browser tab; no graceful recovery | Use error return + Lua error display; never call exit() from WASM | Always on WASM |

---

## "Looks Done But Isn't" Checklist

Things that appear complete but are missing critical pieces.

- [ ] **Multi-layer compositor:** All 4 layer buffers clear to `Pixel4(15)` (transparent) at start of each frame — verify black pixels are opaque, not transparent
- [ ] **Multi-layer compositor:** SDL3 renderer reads composited output, not `g_canvas` directly — verify by drawing exclusively to layer 2 and confirming it appears
- [ ] **Multi-layer compositor:** WASM `getCanvasData128()` (or equivalent) returns composited data — verify in browser
- [ ] **Multi-layer compositor:** Memory budget verified on ESP32 — `4 * 8192 (layers) + 64KB (Lua) + system` fits in target SRAM
- [ ] **Sprite rework:** No `ICanvas<uint8_t>&` signatures remain in new Sprite API — verify compiles against Canvas4 target
- [ ] **Sprite rework:** Default `matte` is 15 (`PALETTE_TRANSPARENT`), not 16 — verify transparent pixels not drawn
- [ ] **Sprite rework:** Legacy `_width`, `_height`, `_frame`, `_position`, `_matte`, `_mode` public fields removed — grep confirms no callers remain
- [ ] **Lua hot reload:** F5 sets flag; reload executes at frame start, not inside event handler — verify no "Function not found" errors on first frame after reload
- [ ] **Lua hot reload:** After reload, `g_currentBindings` is valid and `g_lua.callFunction("update")` succeeds — verify with console output
- [ ] **Lua hot reload:** Memory pool reset confirmed — `memoryUsed == 0` after shutdown, non-zero after initialize — verify with debug log
- [ ] **Docusaurus fix:** `npm run build` succeeds with zero MDX errors after all new API files generated — verify in CI or manually
- [ ] **Docusaurus fix:** API Reference navbar link resolves to first API page in browser — verify on deployed GitHub Pages
- [ ] **LuaCallback overload:** No new Lua bindings use the `std::function` overload of `registerFunction` — grep `registerFunction.*\[` confirms none

---

## Recovery Strategies

When pitfalls occur despite prevention, how to recover.

| Pitfall | Recovery Cost | Recovery Steps |
|---------|---------------|----------------|
| Dangling LuaCallback pointer on hot reload | MEDIUM | Fix `registerFunction(LuaCallback)` to store callback in stable collection; re-register all bindings; verify reload path |
| ESP32 boot crash after layer addition (SRAM exceeded) | MEDIUM | Move layer buffers to PSRAM with `EXT_RAM_BSS_ATTR`; or reduce layer count to 2 via compile-time option; measure heap usage |
| Compositor uses per-pixel dispatch, too slow on ESP32 | MEDIUM | Refactor `ICanvas` to add `getRawBuffer()`; rewrite compositor blend loop to operate on raw packed bytes |
| Sprite compile failures (uint8_t vs Pixel4 mismatch) | LOW | Change `draw(ICanvas<uint8_t>&)` to `draw(ICanvas<Pixel4>&)`; update `pixel != matte` to `pixel.value != matte`; change matte default to 15 |
| Docusaurus API sidebar broken after new template classes | LOW | Run `generate-api-docs.js` with angle-bracket escaping; `npm run build`; fix MDX errors; commit fixed generated files |
| Hot reload triggers mid-frame (update called on empty Lua) | LOW | Add `reload_requested` flag; move reload logic out of event handler; test F5 during active Lua update call |
| Compositor transparency bug (black punches through) | LOW | Change transparency check from `pixel.value != 0` to `pixel.value != PALETTE_TRANSPARENT`; fix layer clear to use `Pixel4(15)` |

---

## Pitfall-to-Phase Mapping

| Pitfall | Prevention Phase | Verification |
|---------|------------------|--------------|
| LuaAllocator dangling-pointer on hot reload | Lua hot reload — Step 1: fix LuaCallback registration | Reload twice in succession; confirm no crash or stale-pointer errors |
| ESP32 SRAM budget exceeded by 4 layers | Multi-layer design — compute budget before declaring buffers | Verify `4*8192 + 64*1024 + system_overhead < 512KB`; test on device |
| Template mismatch in layer compositor interface | Multi-layer design — add buffer access to ICanvas interface | Compositor compiles without static_cast to concrete size |
| Sprite API breaking change (uint8_t → Pixel4) | Sprite rework — define new API targets; grep and update all callers | Clean build; no `ICanvas<uint8_t>` in new sprite code |
| LuaCanvas routing for layer switching | Multi-layer Lua API — pre-allocate static LuaCanvas array | `setLayer(2); rectangle(...)` draws only to layer 2 in test script |
| Hot reload mid-frame ordering | Lua hot reload — use reload_requested flag pattern | F5 during running script produces zero "Function not found" errors |
| Docusaurus MDX angle-bracket failures | Docusaurus fix — update generate-api-docs.js escaping | `npm run build` passes with zero errors; API Reference link works |
| Compositor transparency semantics (black vs index 15) | Multi-layer compositor implementation — use PALETTE_TRANSPARENT constant | Black fills on lower layer visible when upper layer draws transparent |
| SDL3 renderer still reads single canvas after layer system | Multi-layer compositor integration — update expand_canvas_to_rgb() | Layer 3-only draw test visible in SDL3 window |
| WASM binding still reads single canvas after layer system | Multi-layer compositor + WASM binding update | Layer 3-only draw test visible in browser WASM canvas |
| std::function Lua callbacks on WASM/ESP32 | Sprite Lua API + layer Lua API design | All new bindings use `lua_CFunction`; grep confirms no `LuaCallback` lambda registrations |

---

## Sources

- Codebase analysis: `src/scripting/lua_engine.cpp` — `luaAllocator`, `memoryPool`, `registerFunction(LuaCallback)` dangling-pointer bug (2026-02-24)
- Codebase analysis: `src/scripting/bindings.cpp` — `g_currentBindings` global, `registerAll()`, input binding pattern (2026-02-24)
- Codebase analysis: `src/platform/sdl/sdl_main.cpp` — `expand_canvas_to_rgb()`, single-canvas assumption in render path (2026-02-24)
- Codebase analysis: `include/enjin2/graphics/canvas.hpp` — `Canvas4<W,H>` BUFFER_SIZE, ICanvas interface, missing `getRawBuffer()` (2026-02-24)
- Codebase analysis: `include/enjin2/graphics/sprite.hpp` — `ICanvas<uint8_t>` draw target, matte=16 out-of-range default, legacy `_` fields (2026-02-24)
- Codebase analysis: `include/enjin2/scripting/lua_platform.hpp` — `MEMORY_LIMIT = 64*1024` on ESP32 (2026-02-24)
- Codebase analysis: `include/enjin2/graphics/palette.hpp` — `PALETTE_TRANSPARENT = 15` convention (2026-02-24)
- Codebase analysis: `docs/docusaurus.config.js` — `markdown: { format: 'detect' }`, dual-plugin setup, navbar `sidebarId: 'apiSidebar'` (2026-02-24)
- Codebase analysis: `docs/api-sidebar.js` — `type: 'autogenerated'` sidebar pattern (2026-02-24)
- Codebase analysis: `src/bindings/emscripten_bindings.cpp` — `getCanvasData128()` reads single canvas, WASM layer exposure gap (2026-02-24)
- ESP32-S3 memory map: ESP-IDF docs — 512KB SRAM total; WiFi/BT buffers consume ~100KB; heap overhead significant
- Docusaurus MDX parse error behavior: bare `<` outside code blocks treated as JSX open tag; confirmed by MDX v3 parser behavior
- Lua `longjmp` and C++ destructors: ISO C++ standard; LLVM/Emscripten `-fno-exceptions` UB with stack-allocated RAII objects across longjmp

---
*Pitfalls research for: multi-layer Canvas4 composition, sprite rework, Lua hot reload, Docusaurus MDX fix — enjin2 v1.4*
*Researched: 2026-02-24*
