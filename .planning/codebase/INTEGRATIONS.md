# External Integrations

**Analysis Date:** 2026-02-28

## Lua Scripting API

**Overview:**
The engine exposes a love2d.graphics-style Lua API for game scripting. All functions are registered as global Lua functions and tables. The Lua runtime is sandboxed: `dofile`, `loadfile`, `require`, `io`, `debug`, `package`, and `os` modules are disabled.

**Core Global Functions (Drawing API):**

Canvas Management:
- `clear(color)` - Clear canvas to color (0-15 for 4-bit, 0-255 for 8-bit)
- `getWidth()` - Get canvas width in pixels
- `getHeight()` - Get canvas height in pixels

Color & Style:
- `setColor(color)` - Set current drawing color
- `getColor()` - Get current drawing color
- `setLineWidth(width)` - Set line thickness for primitives
- `getLineWidth()` - Get current line width

Primitives:
- `point(x, y)` - Draw single pixel
- `line(x1, y1, x2, y2)` - Draw line
- `rectangle(mode, x, y, width, height)` - Draw or fill rectangle. Mode: "line" or "fill"
- `circle(mode, x, y, radius)` - Draw or fill circle. Mode: "line" or "fill"
- `triangle(mode, x1, y1, x2, y2, x3, y3)` - Draw or fill triangle. Mode: "line" or "fill"

Pixel Access:
- `setPixel(x, y, color)` - Set single pixel
- `getPixel(x, y)` - Read single pixel value

Text Rendering (LAYER-06 support):
- `text(string, x, y [, color] [, size] [, font])` - Draw text at position
- `textWrapped(string, x, y, maxWidth [, color] [, size] [, font])` - Draw text with word wrap
- `setTextSize(size)` - Set text multiplier (1=normal, 2=double, etc.)
- `getTextSize()` - Get current text size
- `setFont(name)` - Switch to named font (e.g., "default", "default8")
- `getFont()` - Get current font name
- `getTextWidth(string [, size] [, font])` - Measure string width in pixels
- `getTextHeight([size] [, font])` - Measure character height in pixels

**Layer System (LAYER-06):**

Layer Constants (1-indexed in Lua):
- `LAYER_BG` = 1 - Background layer
- `LAYER_MID` = 2 - Middle layer
- `LAYER_FG` = 3 - Foreground layer
- `LAYER_UI` = 4 - UI layer

Layer Functions:
- `setLayer(layer)` - Switch active layer for drawing (1-4)
- `getLayer()` - Get current active layer
- `clearLayer(color)` - Clear current layer
- `getLayerCount()` - Get total number of layers
- `setLayerVisible(layer, visible)` - Show/hide layer for rendering
- `isLayerVisible(layer)` - Check if layer is visible

**Sprite System (SPR-06):**

Sprite Pool Management:
- `engine.sprite.load(name)` - Load .njn sprite file from asset directory. Returns handle 0-15 or -1 if pool full.
- `engine.sprite.free(handle)` - Free sprite slot. Reclaims buffer space if asset is at buffer tip.
- `newSprite(data_lightuserdata, cell_w, cell_h, cols, rows)` - Create sprite from raw pixel buffer (lightuserdata). Returns handle or -1.

Sprite Animation & Drawing:
- `drawSprite(handle, x, y [, flipH] [, flipV] [, rotate90])` - Draw current frame. Supports flip and 90° rotation.
- `updateSprite(handle, dt_ms)` - Advance animation by dt_ms milliseconds.
- `setFrame(handle, frame)` - Jump to specific frame index.

Sprite Properties (managed per slot):
- FPS: 8.0 default (frames per second)
- Animation modes: Loop, Once, PingPong
- Transparent pixel index: 15 (compile-time constant, skipped during blit)
- Pool size: 16 slots (fixed, zero-allocation)
- Asset buffer: 64KB for loaded .njn files

**Sprite Asset Format (.njn):**
- Binary format with header followed by pixel data
- Structure: `NjnHeader` (uint8 cellW, cellH, cols, rows) + uint8_t pixel array
- Pixel encoding: 1 byte per pixel, lower 4 bits = palette index 0-15, index 15 = transparent
- File path: `assetPath_ + "/" + name + ".njn"` (resolved in `bindings_sprite_load.cpp` line 33-38)
- Load via: `engine.sprite.load("spritename")` → loads `/assets/spritename.njn`

**Input Polling (INP-05):**

Button/Key Input:
- `engine.input.held(button)` - Check if button is held this frame
- `engine.input.just_pressed(button)` - Check if button pressed this frame
- `engine.input.just_released(button)` - Check if button released this frame
- `engine.input.axis(axis)` - Read analog axis value (0-7). Returns float -1.0 to 1.0.

Also available as global functions (legacy):
- `isButtonHeld(button)` - Same as engine.input.held()
- `isButtonJustPressed(button)` - Same as engine.input.just_pressed()
- `isButtonJustReleased(button)` - Same as engine.input.just_released()
- `getAxis(axis)` - Same as engine.input.axis()

Button/Axis mapping is platform-specific (SDL, ESP32, or VCV Rack context).

**Time Functions (ENG-04):**
- `engine.time.delta()` - Get delta time in seconds (float)
- `engine.time.now()` - Get total elapsed time in seconds (float)
- `engine.time.frame()` - Get frame counter (uint32)
- `time()` - Alias for engine.time.now()

**Scene & Entity Management (ENG-01, ENG-02, ENG-03):**

Scene Switching:
- `engine.scene.switch(sceneName)` - Switch to named scene (via SceneStateMachine)
- `engine.scene.find(objectName)` - Find object by name in active scene. Returns ObjectProxy userdata.

Entity Spawning/Destruction:
- `engine.scene.spawn(objectName)` - Create new object in active scene
- `engine.scene.destroy(objectName)` - Destroy object by name

Object Proxy (returned from `engine.scene.find()`):
- Properties (read/write): `x`, `y`, `visible`, `layer` (1-indexed), `active`
- Properties (read-only): `name`
- Methods: `addTag(tag)`, `hasTag(tag)`, `clearTags()`

**Collision Functions (ENG-05):**
- `engine.collision.aabb(x1, y1, w1, h1, x2, y2, w2, h2)` - AABB-AABB collision test
- `engine.collision.circleCircle(x1, y1, r1, x2, y2, r2)` - Circle-circle collision
- `engine.collision.pointInRect(px, py, x, y, w, h)` - Point in rectangle
- `engine.collision.pointInCircle(px, py, cx, cy, r)` - Point in circle
- `engine.collision.lineLine(x1, y1, x2, y2, x3, y3, x4, y4)` - Line-line intersection
- `engine.collision.lineCircle(x1, y1, x2, y2, cx, cy, r)` - Line-circle collision
- `engine.collision.aabbOverlap(x1, y1, w1, h1, x2, y2, w2, h2)` - AABB overlap with delta info
- `engine.collision.circleResponse(x1, y1, r1, x2, y2, r2)` - Circle collision response
- `engine.collision.reflect(vx, vy, nx, ny)` - Reflect velocity vector across normal

**Random Number Generation (RNG):**
- `engine.random.seed(value)` - Seed xorshift32 PRNG
- `engine.random.integer(min, max)` - Generate random integer in range
- `engine.random.float()` - Generate random float [0.0, 1.0)

**Persistent Key-Value Store (STORE):**
- `engine.store.save(key, value)` - Save value (number, string, bool, or table)
- `engine.store.load(key)` - Load value by key. Returns value or nil.
- `engine.store.exists(key)` - Check if key exists
- `engine.store.delete(key)` - Delete key
- `engine.store.clear()` - Clear all data

Store Characteristics:
- Per-script instance (persists across script reloads if configured)
- Fixed capacity: 16 keys max
- String values: 128 bytes max
- Table support: Each table holds up to 16 entries
- Persistence: JSON file on desktop (VCV Rack), NVS deferred on ESP32
- Access: `setStorePath(path)` sets JSON file location; empty path = in-memory only

**Garbage Collection (GC-01, GC-02):**
- `engine.lua.collect()` - Force garbage collection
- `engine.lua.memory()` - Get current Lua memory usage in bytes

**Math Types:**
- `Vec2(x, y)` - 2D vector constructor
- `Point(x, y)` - Point constructor (alias for Vec2)
- `Rect(x, y, w, h)` - Rectangle constructor

**Logging:**
- `engine.log(...)` - Print to console/log
- `print(...)` - Alias for engine.log()

## Component System

**C_LuaScript Component:**
- **Purpose:** Attach Lua scripts to game objects. Each script is a separate Lua coroutine/execution context.
- **Location:** `include/enjin2/components/lua_script.hpp`, `src/components/lua_script.cpp`
- **Script callbacks:** `init()`, `update(dt)`, `lateUpdate()`, `onRender()` (called from host)
- **Bindings:** `LuaBindings::loadScriptFile(path)` loads and executes .lua file, creates ScriptProxy

**Other Components:**
- `C_Position` - Object position (x, y). Accessible via proxy: `obj.x`, `obj.y`
- `C_Drawable` - Render interface with `draw()` method
- `C_Sprite` - Built-in sprite animation (alternative to Lua sprite pool)
- `C_Canvas` - Draws to canvas
- `C_ImageCache` - Caches image assets

## Data Storage

**Databases:**
- **None** - Engine is headless/embedded. VCV Rack integration provides context; no built-in DB.

**File Storage:**
- Local filesystem only. Paths:
  - Sprites: `assetPath_ + "/" + name + ".njn"` (binary sprite format)
  - Scripts: `LuaFileSystem::readScriptFile()` uses VCV Rack or ESP32 native calls (see `lua_platform.cpp`)
  - Persistent store: JSON file (location set via `setStorePath()`)

**Caching:**
- **Sprite Asset Buffer:** 64KB fixed arena in `LuaBindings::assetBuffer_[]`. FIFO allocation with best-effort deallocation (only reclaims if asset is at buffer tip).
- **Font Registry:** Fixed 8-font registry in `LuaBindings::fontRegistry[]`. Pre-loaded fonts can be registered via `registerFont(name, gfxfont_ptr)`.
- **ImageCache Component:** Caches images in memory. Location: `include/enjin2/components/image_cache.hpp`

**Caching (None detected):**
- No Redis, Memcached, or explicit HTTP caching layer.

## Authentication & Identity

**Auth Provider:**
- **None** - Engine is not networked. Single-user/local game execution.

**Credentials/Secrets:**
- Not applicable (no network APIs requiring authentication).

## Monitoring & Observability

**Error Tracking:**
- `luaL_error()` and standard Lua error handling. No external error tracking service.
- C++ exception handling via catch blocks in bindings (e.g., `loadScriptFile()` traps Lua errors)

**Logs:**
- Standard output via `printf()` and `print()` in Lua
- No log aggregation; logs printed to console/stdio
- Platform-specific: VCV Rack may redirect to plugin UI; ESP32 via serial; web via browser console

**Profiling:**
- Lua memory profiling: `engine.lua.memory()` returns heap usage
- No frame-time profiling or performance metrics exposed

## CI/CD & Deployment

**Hosting:**
- **Desktop:** SDL3-based standalone executable or VCV Rack plugin module
- **Embedded:** ESP32 firmware via IDF build system
- **Web:** WebAssembly module (Emscripten) for browser execution

**CI Pipeline:**
- **Testing:** CMake/CTest (runs via `cmake --build . --target test`)
- **Linting:** clang-tidy (opt-in via `cmake -DCLANG_TIDY=ON`)
- **Documentation:** Doxygen + Docusaurus (opt-in via `cmake --build . --target docs`)
- **No GitHub Actions/CI services detected** - Manual local builds

## Environment Configuration

**Lua Sandbox Configuration:**
- Disabled modules: `dofile`, `loadfile`, `require`, `io`, `debug`, `package`, `os`
- Heap size: 256KB (configured in `lua_engine.cpp`)
- No stdin/stdout redirection; prints to parent process stdout

**Sprite Asset Path:**
- Set via `LuaBindings::setAssetPath(path)`. Default: empty (current directory)
- Example: `setAssetPath("/assets")` resolves `engine.sprite.load("player")` to `/assets/player.njn`

**Store Persistence:**
- Set via `LuaBindings::setStorePath(path)`. Default: empty (in-memory only)
- If path provided, JSON auto-loads on `setStorePath()` and can be manually saved

**Font Registration:**
- Register fonts via `LuaBindings::registerFont(name, gfxfont_ptr)`
- Built-in fonts: "default" (5x7), "default8" (8x8)
- Max 8 fonts registered

**Required Environment Variables:**
- **ESP32 builds:** `IDF_PATH` (ESP-IDF root), `LUA_INCLUDE_DIRS`, `LUA_LIBRARIES`
- **WebAssembly:** EMSCRIPTEN environment (emsdk)
- **Desktop:** Standard C++ toolchain

**Secrets Location:**
- Not applicable. No credentials or API keys in use.

## Webhooks & Callbacks

**Incoming:**
- Not applicable (engine is not a server).

**Outgoing:**
- C_LuaScript lifecycle callbacks: `init()`, `update(dt)`, `lateUpdate()`, `onRender()`
- Entity lifecycle: `Object::onDestroy()` hook for cleanup
- Scene lifecycle: `Scene::onStart()`, `Scene::onStop()` (application-specific)

## Missing Integrations & Known Gaps

**Audio System:**
- **Status:** Not implemented
- **Scope:** No audio playback, synthesis, or MIDI support
- **Components:** No C_AudioSource or equivalent
- **Lua Bindings:** No audio.* API
- **Impact:** Games cannot produce sound/music

**Tilemap System:**
- **Status:** Not implemented (top priority per user)
- **Scope:** No tilemap rendering, collision, or editing tools
- **Components:** No C_Tilemap or equivalent
- **Lua Bindings:** No tilemap.* API
- **Impact:** Grid-based games (roguelikes, puzzles, platformers) must implement custom solutions
- **Workaround:** Manually draw tile grid via sprite sheets and position sprites, or use layers + sprite pool

**Physics Engine:**
- **Status:** Only collision detection (no dynamics)
- **Scope:** Collision shapes and tests available, but no velocity integration, gravity, or impulse response
- **Components:** No C_RigidBody or equivalent
- **Impact:** Games requiring complex physics must implement custom integration

**Networking:**
- **Status:** Not implemented
- **Scope:** No multiplayer, HTTP client, or WebSocket support
- **Impact:** Single-player/local games only

**Input Device Support:**
- **Status:** Partial. SDL gamepad, keyboard; no mouse, touch, or advanced input
- **Components:** InputState polls SDL events in `sdl_main.cpp`
- **Impact:** Touch-based games require custom handling

**Post-Processing Effects:**
- **Status:** Partial. PostFX system exists (`src/effects/postfx.cpp`), but limited built-in effects
- **Lua Bindings:** No postfx.* API exposed to scripts
- **Impact:** Advanced visual effects (bloom, blur, distortion) require C++ components

**Pathfinding:**
- **Status:** Not implemented
- **Scope:** No A*, Dijkstra, or steering behaviors
- **Impact:** AI games must implement custom pathfinding

---

*Integration audit: 2026-02-28*
