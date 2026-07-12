---
id: LuaBindings
title: LuaBindings
sidebar_label: LuaBindings
---

# LuaBindings

Lua bindings for Enjin graphics and UI. 


Provides love2d.graphics-style API for familiar Lua scripting. All functions are registered as global Lua functions. 

---

**Namespace:** `enjin2`

**Header:** `include/enjin2/scripting/bindings.hpp`

## Public Methods

### ` LuaBindings(LuaEngine *luaEngine)`

Constructor. 

luaEngineLua engine to bind to 

---

### `void registerAll()`

Register all bindings with Lua engine. 

---

### `void setCanvas(LuaCanvas *canvas)`

Set current canvas for drawing operations. 

canvasCanvas to draw on 

---

### `LuaCanvas * getCanvas() const`

Get current canvas. 

Current canvas or nullptr 

---

### `void setInput(InputState *input)`

Set input state for this frame. 

inputCurrent InputState pointer (updated by host after input_platform_poll) 

---

### `InputState * getInput() const`

Get current frame's input state. 

Pointer to InputState set by host this frame, or nullptr if not set 

---

### `void setLayers(LuaCanvas **canvases, uint8_t count, bool *visibleArr)`

Wire layer canvas pointers from host (called once at init). 

canvasesArray of LuaCanvas pointers, one per layer countNumber of layers visibleArrPointer to LayerCompositor::visible[] array 

---

### `void resetSpritePool()`

Reset all sprite pool slots to inactive state and clear asset buffer. 

Called automatically from registerAll() to ensure a clean sprite pool on every Lua state reload. Also resets drawing state (currentColor, lineWidth, text state). 

---

### `void setAssetPath(const std::string &path)`

Set the base directory for loading .njn sprite assets. 

pathDirectory path (e.g. "/data" or "assets") 

---

### `void registerFont(const char *name, const GFXfont *font)`

Register a named font for use from Lua via setFont(name). 

nameName to use in Lua (e.g. "myfont"); "default" = built-in 5x7, "default8" is pre-registered. fontPointer to GFXfont (non-owning); may be nullptr for "default" to mean built-in 5x7. 

---

### `void setSceneStateMachine(SceneStateMachine *ssm)`

Inject SceneStateMachine pointer for engine.scene.switch(). 

ssmNon-owning pointer; may be nullptr in SDL standalone mode 

---

### `void setActiveScene(Scene *scene)`

Inject active Scene pointer for engine.scene.find(). 

sceneNon-owning pointer to the currently active scene; may be nullptr 

---

### `void setActiveCamera(C_Camera *cam)`

Set active camera pointer for engine.camera.* bindings. 

camNon-owning pointer; may be nullptr 

---

### `C_Camera * getActiveCamera() const`

Get active camera pointer. 

Current active camera or nullptr 

---

### `void tickCameraFollow(float dt)`

Tick camera follow — called automatically each frame from the update path. 

Reads target position from m_followTargetProxy and calls cam-&gt;lookAt(). If the proxy is invalid or destroyed, silently clears the follow target. No-op if no follow target is set or no active camera is available. dtDelta time in seconds (unused; forwarded for future use) 

---

### `void tickCoroutines(float dt)`

Tick all active coroutines — resume ready ones via lua_resume. Called once per frame from SDL runner OUTSIDE any pcall scope (avoids yield-across-pcall). 

dtDelta time in seconds for wait timer decrement 

---

### `void clearCoroutines()`

Cancel all active coroutines and unref their threads. Called on scene transition (setActiveScene) and hot-reload (registerAll). 

---

### `void tickTweens(float dt)`

Tick all active tweens — advance elapsed time and interpolate properties. Called once per frame from SDL runner after tickCoroutines(dt). 

dtDelta time in seconds 

---

### `void clearTweens()`

Cancel all active tweens and unref their targets/callbacks. Called on scene transition (setActiveScene) and hot-reload (registerAll). 

---

### `void setDebugCanvas(LuaCanvas *canvas)`

Inject debug canvas pointer (called from host alongside setLayers). 

canvasNon-owning pointer to the debug layer LuaCanvas

---

### `LuaCanvas * getDebugCanvas() const`

Get debug canvas pointer. 

Current debug canvas or nullptr 

---

### `void setTimeState(float dt, float totalTime, uint32_t frameCount)`

Update time state for engine.time.* bindings (call before each frame's update). 

dtCurrent frame delta in seconds totalTimeAccumulated total time in seconds frameCountCurrent frame counter 

---

### `void setStorePath(const char *path)`

Set storage file path for auto-persist (call before scripts run). If empty, store operations are in-memory only. 

pathFile path for JSON save file 

---

### `LuaStore & getStore()`

Get the persistent store (for testing). 

Reference to the LuaStore

---

### `LuaEventBus & getEventBus()`

Get the event bus (for testing). 

Reference to the LuaEventBus

---

### `const SpriteSheet * getSpriteSheet(int handle) const`

Get a pointer to the SpriteSheet at a given sprite pool slot (Phase 43: C_Tilemap binding). 

Provides read-only access to sprite pool entries from static Lua binding functions. handleSprite pool index (0..LUA_SPRITE_POOL_SIZE-1). Pointer to the SpriteSheet if the slot is active, nullptr otherwise. 

---

### `static LuaBindings * getBindings(lua_State *L)`

Retrieve the LuaBindings instance from the Lua registry. 

Stored via "enjin_bindings" key during registerAll(). Used by static Lua C-function callbacks that are not member functions of LuaBindings. LActive Lua state. Pointer to the LuaBindings for this Lua state, or nullptr on error. 

---

## Private Methods

### `static int lua_getWidth(lua_State *L)`

---

### `static int lua_getHeight(lua_State *L)`

---

### `static int lua_clear(lua_State *L)`

---

### `static int lua_setColor(lua_State *L)`

---

### `static int lua_getColor(lua_State *L)`

---

### `static int lua_setLineWidth(lua_State *L)`

---

### `static int lua_getLineWidth(lua_State *L)`

---

### `static int lua_point(lua_State *L)`

---

### `static int lua_line(lua_State *L)`

---

### `static int lua_rectangle(lua_State *L)`

---

### `static int lua_circle(lua_State *L)`

---

### `static int lua_triangle(lua_State *L)`

---

### `static int lua_setPixel(lua_State *L)`

---

### `static int lua_getPixel(lua_State *L)`

---

### `static int lua_createEntity(lua_State *L)`

---

### `static int lua_destroyEntity(lua_State *L)`

---

### `static int lua_addComponent(lua_State *L)`

---

### `static int lua_removeComponent(lua_State *L)`

---

### `static int lua_getComponent(lua_State *L)`

---

### `static int lua_print(lua_State *L)`

---

### `static int lua_fastFillRect(lua_State *L)`

---

### `static int lua_fastDrawLine(lua_State *L)`

---

### `static int lua_setPaletteColor(lua_State *L)`

---

### `static int lua_getPaletteColor(lua_State *L)`

---

### `static int lua_loadPalette(lua_State *L)`

---

### `static int lua_getPaletteSize(lua_State *L)`

---

### `static int lua_isButtonHeld(lua_State *L)`

---

### `static int lua_isButtonJustPressed(lua_State *L)`

---

### `static int lua_isButtonJustReleased(lua_State *L)`

---

### `static int lua_getAxis(lua_State *L)`

---

### `static int lua_newSprite(lua_State *L)`

---

### `static int lua_loadSprite(lua_State *L)`

---

### `static int lua_freeSprite(lua_State *L)`

---

### `static int lua_drawSprite(lua_State *L)`

---

### `static int lua_updateSprite(lua_State *L)`

---

### `static int lua_setFrame(lua_State *L)`

---

### `static int lua_setLayer(lua_State *L)`

---

### `static int lua_getLayer(lua_State *L)`

---

### `static int lua_clearLayer(lua_State *L)`

---

### `static int lua_getLayerCount(lua_State *L)`

---

### `static int lua_setLayerVisible(lua_State *L)`

---

### `static int lua_isLayerVisible(lua_State *L)`

---

### `static int lua_text(lua_State *L)`

---

### `static int lua_textWrapped(lua_State *L)`

---

### `static int lua_textCentered(lua_State *L)`

---

### `static int lua_textAligned(lua_State *L)`

---

### `static int lua_setTextSize(lua_State *L)`

---

### `static int lua_getTextSize(lua_State *L)`

---

### `static int lua_setFont(lua_State *L)`

---

### `static int lua_getFont(lua_State *L)`

---

### `static int lua_getTextWidth(lua_State *L)`

---

### `static int lua_getTextHeight(lua_State *L)`

---

### `static int lua_engine_debug_rect(lua_State *L)`

---

### `static int lua_engine_debug_circle(lua_State *L)`

---

### `static int lua_engine_debug_line(lua_State *L)`

---

### `static int lua_engine_debug_cross(lua_State *L)`

---

### `static int lua_engine_debug_text(lua_State *L)`

---

### `static int lua_engine_debug_setEnabled(lua_State *L)`

---

### `static int lua_engine_debug_getEnabled(lua_State *L)`

---

### `static int lua_engine_physics_setGravity(lua_State *L)`

---

### `static int lua_engine_physics_getGravity(lua_State *L)`

---

### `static int lua_engine_physics_applyGravity(lua_State *L)`

---

### `static int lua_engine_physics_bounce(lua_State *L)`

---

### `static int lua_engine_physics_applyDrag(lua_State *L)`

---

### `static int lua_engine_physics_springForce(lua_State *L)`

---

### `static int lua_engine_physics_attract(lua_State *L)`

---

### `static int lua_engine_physics_orbitVelocity(lua_State *L)`

---

### `static int lua_engine_physics_applyVelocity(lua_State *L)`

---

### `static int lua_engine_physics_raycast(lua_State *L)`

---

### `static int lua_engine_scene_switch(lua_State *L)`

---

### `static int lua_engine_scene_find(lua_State *L)`

---

### `static int lua_engine_scene_spawn(lua_State *L)`

---

### `static int lua_engine_scene_destroy(lua_State *L)`

---

### `static int lua_engine_scene_persist(lua_State *L)`

---

### `static int lua_engine_scene_unpersist(lua_State *L)`

---

### `static int lua_engine_input_held(lua_State *L)`

---

### `static int lua_engine_input_just_pressed(lua_State *L)`

---

### `static int lua_engine_input_just_released(lua_State *L)`

---

### `static int lua_engine_input_axis(lua_State *L)`

---

### `static int lua_engine_time_delta(lua_State *L)`

---

### `static int lua_engine_time_now(lua_State *L)`

---

### `static int lua_engine_time_frame(lua_State *L)`

---

### `static int lua_engine_log(lua_State *L)`

---

### `static int lua_engine_lua_collect(lua_State *L)`

---

### `static int lua_engine_lua_memory(lua_State *L)`

---

### `static int lua_engine_collision_aabb(lua_State *L)`

---

### `static int lua_engine_collision_circleCircle(lua_State *L)`

---

### `static int lua_engine_collision_pointInRect(lua_State *L)`

---

### `static int lua_engine_collision_pointInCircle(lua_State *L)`

---

### `static int lua_engine_collision_lineLine(lua_State *L)`

---

### `static int lua_engine_collision_lineCircle(lua_State *L)`

---

### `static int lua_engine_collision_aabbOverlap(lua_State *L)`

---

### `static int lua_engine_collision_circleResponse(lua_State *L)`

---

### `static int lua_engine_collision_reflect(lua_State *L)`

---

### `static int lua_engine_config_resolution(lua_State *L)`

---

### `static int lua_engine_state_switch(lua_State *L)`

---

### `static int lua_engine_state_current(lua_State *L)`

---

### `static int lua_engine_state_on_enter(lua_State *L)`

---

### `static int lua_engine_state_on_exit(lua_State *L)`

---

### `static int lua_engine_random_seed(lua_State *L)`

---

### `static int lua_engine_random_integer(lua_State *L)`

---

### `static int lua_engine_random_float(lua_State *L)`

---

### `static int lua_engine_camera_follow(lua_State *L)`

---

### `static int lua_engine_camera_stopFollow(lua_State *L)`

---

### `static int lua_engine_camera_setDeadZone(lua_State *L)`

---

### `static int lua_engine_async_start(lua_State *L)`

---

### `static int lua_engine_async_wait(lua_State *L)`

---

### `static int lua_engine_async_cancel(lua_State *L)`

---

### `static int lua_engine_async_cancelAll(lua_State *L)`

---

### `static int lua_engine_async_wait_frames(lua_State *L)`

---

### `static int lua_engine_tween_to(lua_State *L)`

---

### `static int lua_engine_tween_cancel(lua_State *L)`

---

### `static int lua_engine_tween_cancelAll(lua_State *L)`

---

### `static int lua_engine_tween_await(lua_State *L)`

---

### `static int lua_engine_ui_progressBar(lua_State *L)`

---

### `static int lua_engine_ui_statBar(lua_State *L)`

---

### `static int lua_engine_ui_panel(lua_State *L)`

---

### `static int lua_engine_ui_label(lua_State *L)`

---

### `static int lua_engine_store_save(lua_State *L)`

---

### `static int lua_engine_store_load(lua_State *L)`

---

### `static int lua_engine_store_exists(lua_State *L)`

---

### `static int lua_engine_store_delete(lua_State *L)`

---

### `static int lua_engine_store_clear(lua_State *L)`

---

### `static int lua_engine_store_flush(lua_State *L)`

---

### `static int lua_engine_store_path(lua_State *L)`

---

### `static int lua_Vec2_new(lua_State *L)`

---

### `static int lua_Point_new(lua_State *L)`

---

### `static int lua_Rect_new(lua_State *L)`

---

### `static int lua_Vec2_add(lua_State *L)`

---

### `static int lua_Vec2_sub(lua_State *L)`

---

### `static int lua_Vec2_mul(lua_State *L)`

---

### `static int lua_Vec2_div(lua_State *L)`

---

### `static int lua_Vec2_unm(lua_State *L)`

---

### `static int lua_Vec2_eq(lua_State *L)`

---

### `static int lua_Vec2_tostring(lua_State *L)`

---

### `static int lua_Vec2_index(lua_State *L)`

---

### `static int lua_Vec2_newindex(lua_State *L)`

---

### `static int lua_Vec2_length(lua_State *L)`

---

### `static int lua_Vec2_lengthSquared(lua_State *L)`

---

### `static int lua_Vec2_normalized(lua_State *L)`

---

### `static int lua_Vec2_dot(lua_State *L)`

---

### `static int lua_Vec2_cross(lua_State *L)`

---

### `static int lua_Vec2_distance(lua_State *L)`

---

### `static int lua_Vec2_angle(lua_State *L)`

---

### `static int lua_Vec2_rotate(lua_State *L)`

---

### `static int lua_Point_add(lua_State *L)`

---

### `static int lua_Point_sub(lua_State *L)`

---

### `static int lua_Point_eq(lua_State *L)`

---

### `static int lua_Point_tostring(lua_State *L)`

---

### `static int lua_Point_index(lua_State *L)`

---

### `static int lua_Point_newindex(lua_State *L)`

---

### `static int lua_Rect_eq(lua_State *L)`

---

### `static int lua_Rect_tostring(lua_State *L)`

---

### `static int lua_Rect_index(lua_State *L)`

---

### `static int lua_Rect_newindex(lua_State *L)`

---

### `static int lua_Rect_contains(lua_State *L)`

---

### `static int lua_Rect_intersects(lua_State *L)`

---

### `static int lua_math_clamp(lua_State *L)`

---

### `static int lua_math_lerp(lua_State *L)`

---

### `static int lua_math_remap(lua_State *L)`

---

### `static int lua_math_sign(lua_State *L)`

---

### `static int lua_math_smoothstep(lua_State *L)`

---

### `static int lua_math_distance(lua_State *L)`

---

### `void registerEngineTable()`

Register engine.* global table (called from registerAll()) Builds engine.scene, engine.input, engine.time, engine.lua sub-tables and engine.log. All C++ pointers (SSM, activeScene, timeState) must be stored in the Lua registry during this call so closures can retrieve them at call time. 

---

### `void registerDebugSubtable(lua_State *L)`

engine.debug.* sub-table (called from registerEngineTable) 

---

### `void registerAsyncSubtable(lua_State *L)`

engine.async.* sub-table (called from registerEngineTable) 

---

### `void registerTweenSubtable(lua_State *L)`

engine.tween.* sub-table (called from registerEngineTable) 

---

### `void registerUISubtable(lua_State *L)`

engine.ui.* sub-table (called from registerEngineTable) 

---

### `void registerProxyMetatable()`

---

### `void registerObjectProxyMetatable()`

Register the ObjectProxy metatable (called from registerAll()). Provides __index (name/hasTag/position/enable read) and __newindex (position write + enable/disable control via C_LuaScript::setEnabled()). 

---

### `void registerComponentProxyMetatable()`

Register ComponentProxy metatables for self:get() return values (Phase 39). Registers C_Position_Proxy metatable with __index providing getX() and getY() methods. Phase 40/41 will add C_Timer_Proxy and C_StateMachine_Proxy in the same call. 

---

### `void registerMathBindings()`

Register Vec2/Point/Rect metatables and math utility globals (called from registerAll()). 

---

