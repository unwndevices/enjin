# External Integrations

**Analysis Date:** 2026-03-01

## APIs & External Services

**Graphics Rendering:**
- SDL3 - Window management, input handling, frame rendering
  - SDK/Client: SDL3::SDL3 (CMake target)
  - Status: Optional, fetched via FetchContent
  - Usage: Desktop display, input polling, frame synchronization
  - Entry point: `src/platform/sdl/sdl_main.cpp`

**Documentation Generation:**
- Doxygen - API documentation extraction
  - Config: `docs/Doxyfile`
  - Integration: CMake build target `docs`
  - Post-processor: `scripts/generate-api-docs.js` (Node.js, xml2js)

## Data Storage

**Databases:**
- Custom persistent store (Lua): `engine.store.*`
  - Client: `src/scripting/bindings_store.cpp`
  - Implementation: Platform-dependent (key-value on ESP32, filesystem on desktop)
  - Functions: `save()`, `load()`, `exists()`, `delete()`, `clear()`

**File Storage:**
- Filesystem (desktop VCV_RACK platform)
  - Scripts: Lua script files loaded via `LuaFileSystem::readScriptFile()`
  - Sprites: .njn binary format from disk
  - No file I/O on ESP32 (security restriction)

**Asset Formats:**
- .njn (enjin native) - 4-bit indexed color sprite sheets
  - Header: 8 bytes (magic "NJ", version, cell dims, grid size)
  - Pixel data: 1 byte per pixel (4-bit palette index)
  - Parser: `include/enjin2/graphics/sprite_asset.hpp`
  - Loader: `src/scripting/bindings_sprite_load.cpp`
- BMP/PNG/TGA/JPEG/HDR - Via stb_image_write.h for canvas export

**Caching:**
- Image cache component: `include/enjin2/components/image_cache.hpp`
  - Pre-rendered graphics caching
  - Fixed-size cache to avoid dynamic allocation

## Authentication & Identity

**Auth Provider:**
- None - This is a game engine, not a services platform
- No user management, OAuth, or remote authentication
- Single-device operation (desktop or ESP32 microcontroller)

## Monitoring & Observability

**Error Tracking:**
- None - No remote error reporting
- Local error handling: `include/enjin2/components/lua_script.hpp` ScriptErrorPolicy
  - Policies: Disable (log once), Log (every frame), Panic (abort/restart)

**Logs:**
- Console output (stdout/stderr)
  - Used by: Lua binding functions via `luaL_error()`, `printf()`
  - Platform-specific: Desktop uses fprintf, ESP32 uses ESP_LOG macros (optional)

**Performance Monitoring:**
- Memory: `engine.lua.memory()` - Returns current Lua heap usage
- GC: `engine.lua.collect()` - Manual garbage collection trigger
- Frame timing: `engine.time.*` subsystem (delta, now, frame count)

## Lua Scripting Bindings

**Scripting System:**
- LuaEngine - Core interpreter instance (`include/enjin2/scripting/lua_engine.hpp`)
  - Location: `src/scripting/lua_engine.cpp`
  - Manages: Lua state creation, memory, script execution, function registration
  - Platform abstraction: `LuaPlatform` class handles library loading per-platform

**Lua Platform Support:**
- Desktop (VCV_RACK): Full Lua with JIT, file I/O, debug libraries
  - File I/O: `LuaFileSystem::readDesktopFile()` → standard filesystem
  - Libraries: All standard Lua libraries loaded
- Embedded (ESP32): Minimal Lua, no file I/O, restricted memory
  - File I/O: `LuaFileSystem::readESP32File()` → LITTLEFS/SPIFFS read-only
  - Libraries: math, table, string only (core libs restricted)
- WebAssembly (Emscripten): LuaJIT without FFI/JIT
  - Compilation flags: `LUAJIT_DISABLE_FFI`, `LUAJIT_DISABLE_JIT`
  - API: Emscripten.js bindings in `src/bindings/emscripten_bindings.cpp`

**Global Lua API (engine.* table):**

`engine.scene.*` - Scene management
- `engine.scene.switch(sceneName)` - Transition to named scene
- `engine.scene.find(name)` - Get scene by name
- `engine.scene.spawn(name, data)` - Create new scene instance
- `engine.scene.destroy(name)` - Destroy named scene

`engine.input.*` - Input polling
- `engine.input.held(buttonIdx)` - Check if button held this frame
- `engine.input.just_pressed(buttonIdx)` - Edge: button pressed this frame
- `engine.input.just_released(buttonIdx)` - Edge: button released this frame
- `engine.input.axis(axisNum)` - Get analog axis value

`engine.time.*` - Frame timing
- `engine.time.delta()` - Elapsed time since last frame (seconds)
- `engine.time.now()` - Current absolute time (seconds)
- `engine.time.frame()` - Current frame number (counter)

`engine.collision.*` - Geometric tests
- `engine.collision.aabb(r1, r2)` - AABB overlap test
- `engine.collision.circleCircle(c1, r1, c2, r2)` - Circle collision
- `engine.collision.pointInRect(p, r)` - Point-in-rectangle
- `engine.collision.pointInCircle(p, c, r)` - Point-in-circle
- `engine.collision.lineLine(...)` - Line-line intersection
- `engine.collision.lineCircle(...)` - Line-circle collision
- `engine.collision.aabbOverlap(...)` - AABB overlap with depth
- `engine.collision.circleResponse(...)` - Collision response vectors
- `engine.collision.reflect(v, n)` - Vector reflection

`engine.lua.*` - Memory & GC control
- `engine.lua.collect()` - Trigger garbage collection
- `engine.lua.memory()` - Returns bytes used by Lua heap

`engine.random.*` - Seeded PRNG
- `engine.random.seed(seed)` - Set RNG seed for determinism
- `engine.random.integer(min, max)` - Pseudorandom int
- `engine.random.float(min, max)` - Pseudorandom float

`engine.physics.*` - Physics helpers (Phase 45)
- `engine.physics.setGravity(gx, gy)` - Set global gravity
- `engine.physics.getGravity()` → gx, gy
- `engine.physics.applyGravity(vx, vy, dt)` - Apply gravity to velocity
- `engine.physics.checkCollision(...)` - Collision checks
- `engine.physics.resolveCollision(...)` - Response vectors

`engine.event.*` - Event bus (Phase 42)
- `engine.event.on(eventName, callback)` - Listen to event
- `engine.event.off(eventName, callback)` - Remove listener
- `engine.event.emit(eventName, ...)` - Emit event with args

`engine.camera.*` - 2D camera system (Phase 44)
- `engine.camera.setPosition(x, y)` - Camera position
- `engine.camera.getPosition()` → x, y
- `engine.camera.lookAt(targetX, targetY)` - Look at target
- `engine.camera.shake(intensity, duration)` - Screen shake effect
- `engine.camera.setBounds(x, y, w, h)` - Clamp camera bounds
- `engine.camera.clearBounds()` - Remove bounds

`engine.store.*` - Persistent key-value store
- `engine.store.save(key, value)` - Save JSON/number/string
- `engine.store.load(key)` - Retrieve value
- `engine.store.exists(key)` - Check key existence
- `engine.store.delete(key)` - Remove key
- `engine.store.clear()` - Clear all entries

**Graphics Bindings:**

Drawing API (love2d.graphics-style)
- `setColor(colorIdx)` - Set draw color (palette index)
- `getColor()` → colorIdx
- `clear(colorIdx)` - Clear canvas to color
- `point(x, y)` - Draw single pixel
- `line(x1, y1, x2, y2)` - Draw line
- `rect(x, y, w, h)` - Draw rectangle outline
- `fillRect(x, y, w, h)` - Filled rectangle
- `circle(x, y, r)` - Circle outline
- `fillCircle(x, y, r)` - Filled circle
- `polygon(points)` - Polygon outline
- `fillPolygon(points)` - Filled polygon
- `triangle(...)` - Triangle drawing

Sprite API
- `loadSprite(filename)` → spriteSheet - Load .njn file
- `drawSprite(sheet, cellIdx, x, y, opts)` - Draw sprite cell
  - Options: flipH, flipV, rotate90, scale
- `getSpriteWidth(sheet)` / `getSpriteHeight(sheet)`
- `getGridCols(sheet)` / `getGridRows(sheet)` - Sheet dimensions

Text API
- `setFont(fontName)` - Select font (defaultfont or builtin)
- `text(string, x, y)` - Draw text
- `textWrapped(string, x, y, maxWidth)` - Word-wrapped text
- `textWidth(string)` → pixels - Measure text width
- `textHeight()` → pixels - Line height

Layer API
- `setLayer(layerNum)` - Set active render layer (0-15)
- `getLayer()` → layerNum
- `clearLayer(layerNum)` - Clear specific layer
- `composeLayers()` - Composite all layers to output

Effects API
- `posterize(levels)` - Reduce colors
- `desaturate()` - Grayscale effect
- `invert()` - Color inversion

**Input Binding:**

Button indices (SDL3 mapping in desktop runner)
- 0: UP (arrows or W)
- 1: DOWN (arrows or S)
- 2: LEFT (arrows or A)
- 3: RIGHT (arrows or D)
- 4: A (Z key)
- 5: B (X key)
- 6: START (Enter key)

Implementation: `src/platform/sdl/sdl_main.cpp` `input_platform_poll()`

**Math Bindings:**

Userdata types
- `Vec2(x, y)` - 2D vector with arithmetic operators
- `Point(x, y)` - Integer point
- `Rect(x, y, w, h)` - Axis-aligned rectangle

Functions
- `clamp(value, min, max)` - Clamp to range
- `lerp(a, b, t)` - Linear interpolation
- `remap(value, inMin, inMax, outMin, outMax)` - Map range
- `distance(x1, y1, x2, y2)` - Euclidean distance
- `sign(value)` - Return -1, 0, or 1
- `smoothstep(edge0, edge1, x)` - Smooth interpolation

**Component Bindings:**

ScriptProxy (self in update/draw)
- `self:get(componentType)` → Component proxy - Get component on owner
- `self:addTag(tagName)` - Add tag to owner
- `self:hasTag(tagName)` → bool
- `self:clearTags()` - Remove all tags

ObjectProxy (via engine.scene.find)
- `obj:getComponent(typeName)` → Component proxy
- `obj:getComponents(typeName)` → list of proxies
- `obj:enable()` / `obj:disable()`
- `obj:destroy()`

ComponentProxy (generic component access)
- `comp.field = value` - Field assignment via __newindex
- `value = comp.field` - Field access via __index
- Component types: C_Position, C_Sprite, C_Timer, C_StateMachine, C_Tilemap, C_Camera, etc.

## WebAssembly Bindings

**Emscripten Integration:**
- Binding file: `src/bindings/emscripten_bindings.cpp`
- Module name: `Enjin2Module` (ES6 module)
- Exposed classes:
  - `Pixel4` - 4-bit pixel value
  - `LuaEngine` - Script interpreter
  - `LuaResult` - Execution result
  - `LuaCanvas` - Rendering context
  - Canvas types: `Canvas4_128x128`, `Canvas8_128x64`, etc.

**Memory Configuration (WASM):**
- Maximum memory: 64MB
- Stack size: 1MB
- Memory growth: Enabled (`-sALLOW_MEMORY_GROWTH=1`)
- Assertions: Enabled in debug builds

## Tilemap System (Phase 43)

**Tilemap Component:** `src/components/tilemap.cpp`
- Grid-based tile rendering
- Collision detection with tiles
- Lua binding: `engine.tilemap.*` functions
- Supports large maps with efficient memory layout

## 2D Camera System (Phase 44)

**Camera Component:** `src/components/camera.cpp`
- Viewport management
- Screen-space transformations
- Lua bindings: `engine.camera.*` functions
- World-space to screen-space conversion

## Optimized 2D Physics (Phase 45)

**Physics Helpers:** `include/enjin2/core/physics.hpp`
- Gravity simulation
- Collision response
- Vector reflection
- Implemented in C++, exposed via `engine.physics.*` Lua bindings
- Supports Vec2 userdata and raw numbers

---

*Integration audit: 2026-03-01*
