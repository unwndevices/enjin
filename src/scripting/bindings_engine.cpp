#include "../../include/enjin2/scripting/bindings.hpp"
#include "../../include/enjin2/scripting/lua_api.hpp"
#include "../../include/enjin2/scripting/lua_event_bus.hpp"
#include "../../include/enjin2/core/scene.hpp"
#include "../../include/enjin2/core/scene_state_machine.hpp"
#include "../../include/enjin2/core/object.hpp"
#include "../../include/enjin2/components/position.hpp"
#include "../../include/enjin2/components/camera.hpp"
#include "bindings_internal.hpp"

namespace enjin2 {

// Forward declarations for engine.event.* static binding functions (defined later in this file)
static int lua_engine_event_on(lua_State* L);
static int lua_engine_event_off(lua_State* L);
static int lua_engine_event_emit(lua_State* L);

// Forward declarations for engine.camera.* static binding functions (Phase 44)
static int lua_engine_camera_setPosition(lua_State* L);
static int lua_engine_camera_getPosition(lua_State* L);
static int lua_engine_camera_lookAt(lua_State* L);
static int lua_engine_camera_shake(lua_State* L);
static int lua_engine_camera_setBounds(lua_State* L);
static int lua_engine_camera_clearBounds(lua_State* L);
// Note: lua_engine_camera_follow and lua_engine_camera_stopFollow are LuaBindings
// member functions (defined later) — no file-scope forward declarations needed.

//==============================================================================
// engine.* Global Table (ENG-01..ENG-06)
//
// Every sub-table registers from a descriptor array (ADR-0013). engine.scene,
// engine.camera, engine.debug and engine.physics.raycast are switchable
// (LuaFeatures, off by default): each is checked once, here or in its own
// register function, and its descriptors stay written either way.
//==============================================================================

void LuaBindings::registerEngineTable() {
    lua_State* L = engine->getState();

    lua_newtable(L);                               // [engine_table]

    // --- engine.scene (ENG-01/02, Phase 51 persistence, ADR-0003 §4) — switchable ---
    static constexpr LuaApiEntry kScene[] = {
        luaFunction("switch", lua_engine_scene_switch, "(id:int) -> nil",
                    "Ask the scene state machine to switch to scene id.",
                    "id: the scene's id in the state machine")
            .note("A no-op when the host installed no scene state machine."),
        luaFunction("find", lua_engine_scene_find, "(name:string) -> ObjectProxy?",
                    "The named object in the active scene, else a persisted one; nil when none.",
                    "name: the object's name"),
        luaFunction("spawn", lua_engine_scene_spawn, "(name:string?) -> ObjectProxy?",
                    "Add a new object, with a C_Position, to the active scene; nil without one.",
                    "name: optional name, for engine.scene.find"),
        luaFunction("destroy", lua_engine_scene_destroy, "(obj:ObjectProxy) -> nil",
                    "Remove an object from the active scene; a destroyed or foreign value is ignored.",
                    "obj: the object"),
        luaFunction("persist", lua_engine_scene_persist, "(obj:ObjectProxy) -> boolean?",
                    "Keep an object alive across scene switches: true, or nil when the pool is full.",
                    "obj: the object; any other value gives nil")
            .note("nil without a scene state machine."),
        luaFunction("unpersist", lua_engine_scene_unpersist, "(obj:ObjectProxy) -> boolean?",
                    "Let a persisted object go at the next scene switch: true, or nil.",
                    "obj: the object; any other value gives nil")
            .note("nil without a scene state machine."),
        luaFunction("colliders", lua_engine_scene_colliders, "() -> ColliderSet?",
                    "The active scene's static collider set; nil without a scene."),
    };
    static constexpr LuaApiModule kSceneModule = luaApiModule(
        LuaApiScope::Table, "engine.scene",
        "The active scene: find, spawn and destroy objects, switch scenes.", kScene);
    if (m_features.scene) luaApiSetSubtable(L, -1, kSceneModule);

    // --- engine.time (ENG-04) ---
    static constexpr LuaApiEntry kTime[] = {
        luaFunction("delta", lua_engine_time_delta, "() -> number",
                    "Seconds since the previous frame, as the host reported it."),
        luaFunction("now", lua_engine_time_now, "() -> number",
                    "Total seconds of frame time the host has reported."),
        luaFunction("frame", lua_engine_time_frame, "() -> int", "The host's frame counter."),
    };
    static constexpr LuaApiModule kTimeModule = luaApiModule(
        LuaApiScope::Table, "engine.time", "Frame time, as the host reports it each frame.", kTime);
    luaApiSetSubtable(L, -1, kTimeModule);

    // --- engine.collision ---
    static constexpr LuaApiEntry kCollision[] = {
        luaFunction("aabb", lua_engine_collision_aabb,
                    "(x1:number, y1:number, w1:number, h1:number, "
                    "x2:number, y2:number, w2:number, h2:number) -> boolean\n"
                    "(a:Rect, b:Rect) -> boolean",
                    "Whether two axis-aligned rectangles overlap; touching edges do not count.",
                    "x1: first rectangle's left edge\n"
                    "y1: first rectangle's top edge\n"
                    "w1: first rectangle's width\n"
                    "h1: first rectangle's height\n"
                    "x2: second rectangle's left edge\n"
                    "y2: second rectangle's top edge\n"
                    "w2: second rectangle's width\n"
                    "h2: second rectangle's height\n"
                    "a: first rectangle\n"
                    "b: second rectangle"),
        luaFunction("circleCircle", lua_engine_collision_circleCircle,
                    "(x1:number, y1:number, r1:number, x2:number, y2:number, r2:number) -> boolean",
                    "Whether two circles overlap or touch.",
                    "x1: first centre column\n"
                    "y1: first centre row\n"
                    "r1: first radius\n"
                    "x2: second centre column\n"
                    "y2: second centre row\n"
                    "r2: second radius"),
        luaFunction("pointInRect", lua_engine_collision_pointInRect,
                    "(px:number, py:number, rx:number, ry:number, rw:number, rh:number) -> boolean\n"
                    "(p:Point|Vec2, r:Rect) -> boolean",
                    "Whether a point is inside a rectangle; the right and bottom edges are outside.",
                    "px: point column\n"
                    "py: point row\n"
                    "rx: rectangle's left edge\n"
                    "ry: rectangle's top edge\n"
                    "rw: rectangle's width\n"
                    "rh: rectangle's height\n"
                    "p: the point\n"
                    "r: the rectangle"),
        luaFunction("pointInCircle", lua_engine_collision_pointInCircle,
                    "(px:number, py:number, cx:number, cy:number, r:number) -> boolean\n"
                    "(p:Point|Vec2, cx:number, cy:number, r:number) -> boolean",
                    "Whether a point is inside or on a circle.",
                    "px: point column\n"
                    "py: point row\n"
                    "cx: centre column\n"
                    "cy: centre row\n"
                    "r: radius\n"
                    "p: the point"),
        luaFunction("lineLine", lua_engine_collision_lineLine,
                    "(x1:number, y1:number, x2:number, y2:number, "
                    "x3:number, y3:number, x4:number, y4:number) -> "
                    "hit:boolean, x:number?, y:number?",
                    "Whether two segments cross, and where; parallel segments never do.",
                    "x1: first segment's start column\n"
                    "y1: first segment's start row\n"
                    "x2: first segment's end column\n"
                    "y2: first segment's end row\n"
                    "x3: second segment's start column\n"
                    "y3: second segment's start row\n"
                    "x4: second segment's end column\n"
                    "y4: second segment's end row"),
        luaFunction("lineCircle", lua_engine_collision_lineCircle,
                    "(x1:number, y1:number, x2:number, y2:number, cx:number, cy:number, "
                    "r:number) -> boolean",
                    "Whether a segment touches a circle.",
                    "x1: start column\n"
                    "y1: start row\n"
                    "x2: end column\n"
                    "y2: end row\n"
                    "cx: centre column\n"
                    "cy: centre row\n"
                    "r: radius"),
        luaFunction("aabbOverlap", lua_engine_collision_aabbOverlap,
                    "(x1:number, y1:number, w1:number, h1:number, "
                    "x2:number, y2:number, w2:number, h2:number) -> "
                    "hit:boolean, x:number?, y:number?, w:number?, h:number?",
                    "Whether two rectangles overlap, and the rectangle they share.",
                    "x1: first rectangle's left edge\n"
                    "y1: first rectangle's top edge\n"
                    "w1: first rectangle's width\n"
                    "h1: first rectangle's height\n"
                    "x2: second rectangle's left edge\n"
                    "y2: second rectangle's top edge\n"
                    "w2: second rectangle's width\n"
                    "h2: second rectangle's height"),
        luaFunction("circleResponse", lua_engine_collision_circleResponse,
                    "(x1:number, y1:number, r1:number, x2:number, y2:number, r2:number) -> "
                    "hit:boolean, nx:number?, ny:number?, depth:number?",
                    "Whether two circles overlap, the unit normal from the first to the second, "
                    "and how deep.",
                    "x1: first centre column\n"
                    "y1: first centre row\n"
                    "r1: first radius\n"
                    "x2: second centre column\n"
                    "y2: second centre row\n"
                    "r2: second radius")
            .note("Circles on the same centre give the normal (1, 0)."),
        luaFunction("reflect", lua_engine_collision_reflect,
                    "(vx:number, vy:number, nx:number, ny:number) -> vx:number, vy:number",
                    "A velocity mirrored off a surface: v - 2(v.n)n.",
                    "vx: velocity x\n"
                    "vy: velocity y\n"
                    "nx: surface normal x (unit length)\n"
                    "ny: surface normal y (unit length)"),
    };
    static constexpr LuaApiModule kCollisionModule = luaApiModule(
        LuaApiScope::Table, "engine.collision",
        "Overlap tests and responses for points, rectangles, circles and segments.", kCollision);
    luaApiSetSubtable(L, -1, kCollisionModule);

    // --- engine.lua (GC-01, GC-02) ---
    static constexpr LuaApiEntry kLua[] = {
        luaFunction("collect", lua_engine_lua_collect, "() -> nil",
                    "Run one small incremental garbage-collection step, not a full collection."),
        luaFunction("memory", lua_engine_lua_memory, "() -> number",
                    "Bytes the Lua heap uses now."),
    };
    static constexpr LuaApiModule kLuaModule = luaApiModule(
        LuaApiScope::Table, "engine.lua", "The Lua VM's heap.", kLua);
    luaApiSetSubtable(L, -1, kLuaModule);

    // --- engine.random (seeded xorshift32 PRNG) ---
    static constexpr LuaApiEntry kRandom[] = {
        luaFunction("seed", lua_engine_random_seed, "(n:int) -> nil",
                    "Reseed the generator; 0 picks a fixed non-zero seed.",
                    "n: the seed (32 bits)")
            .note("Every reload reseeds with the same fixed value, so sequences repeat until "
                  "a script seeds it."),
        luaFunction("integer", lua_engine_random_integer, "(a:int, b:int) -> int",
                    "A random integer from a to b, both included; the bounds may come in either order.",
                    "a: one bound\n"
                    "b: the other bound"),
        luaFunction("float", lua_engine_random_float,
                    "() -> number\n"
                    "(a:number, b:number) -> number",
                    "A random number in (0, 1], or scaled to run from a to b.",
                    "a: the value for 0\n"
                    "b: the value for 1"),
    };
    static constexpr LuaApiModule kRandomModule = luaApiModule(
        LuaApiScope::Table, "engine.random",
        "A seeded xorshift32 generator, separate from math.random.", kRandom);
    luaApiSetSubtable(L, -1, kRandomModule);

    // --- engine.store (persistent key-value store) ---
    static constexpr LuaApiEntry kStore[] = {
        luaFunction("save", lua_engine_store_save,
                    "(key:string, value:number|string|boolean|table) -> boolean",
                    "Store a value under key; false when all 16 keys are taken or a table does "
                    "not fit.",
                    "key: the name; up to 63 bytes, and 15 characters on the device (longer raises)\n"
                    "value: a number, a string (up to 127 bytes), a boolean, or a flat table of up "
                    "to 16 string keys holding numbers, strings or booleans (other values are "
                    "skipped); anything else raises")
            .note("Desktop hosts write the file at once when a path is set; the web and the "
                  "device keep changes in memory until engine.store.flush()."),
        luaFunction("load", lua_engine_store_load,
                    "(key:string) -> value:number|string|boolean|table?",
                    "The value stored under key, or nil; a table comes back as a new table.",
                    "key: the name"),
        luaFunction("exists", lua_engine_store_exists, "(key:string) -> boolean",
                    "Whether key holds a value.",
                    "key: the name"),
        luaFunction("delete", lua_engine_store_delete, "(key:string) -> boolean",
                    "Remove a key; true when it existed.",
                    "key: the name"),
        luaFunction("clear", lua_engine_store_clear, "() -> nil", "Remove every key."),
        luaFunction("flush", lua_engine_store_flush, "() -> boolean",
                    "Write the store out: localStorage on the web, NVS on the device, the path "
                    "file on desktop.")
            .note("false on desktop until engine.store.path sets a file."),
        luaFunction("path", lua_engine_store_path, "(filepath:string) -> nil",
                    "Set the desktop save file and load what it holds.",
                    "filepath: the JSON file")
            .note("The web and the device ignore the path and reload their saved store."),
    };
    static constexpr LuaApiModule kStoreModule = luaApiModule(
        LuaApiScope::Table, "engine.store",
        "A small persistent key-value store: 16 keys, shared by every script the engine runs.",
        kStore);
    luaApiSetSubtable(L, -1, kStoreModule);

    // --- engine.sprite ---
    static constexpr LuaApiEntry kSprite[] = {
        luaFunction("load", lua_loadSprite, "(name:string) -> handle:int",
                    "Load <name>.njn from the asset folder into a sprite slot; -1 on failure.",
                    "name: the file name without .njn, relative to the applet's asset folder")
            .note("Fails when all 16 sprite slots are busy or the 64 KiB pixel arena is full. "
                  "The sprite starts on frame 0, looping at 8 fps."),
    };
    static constexpr LuaApiModule kSpriteModule = luaApiModule(
        LuaApiScope::Table, "engine.sprite", "Sprite sheets loaded from asset files.", kSprite);
    luaApiSetSubtable(L, -1, kSpriteModule);

    // --- engine.tilemap (ADR-0003 §6, #91; scene-free handle #256) ---
    static constexpr LuaApiEntry kTilemap[] = {
        luaFunction("load", lua_loadTilemap, "(name:string) -> Tilemap",
                    "Load <name>.njn (tileset and tile attributes) and <name>.njm (cells) into a "
                    "new map.",
                    "name: the file name without extension, relative to the applet's asset folder")
            .note("Raises when a file is missing or invalid, or all 16 sprite slots are busy. The "
                  "tileset holds its sprite slot until the next reload, even after the map is "
                  "collected. Maps larger than 64x64 are cropped."),
    };
    static constexpr LuaApiModule kTilemapModule = luaApiModule(
        LuaApiScope::Table, "engine.tilemap", "Tile maps loaded from asset files.", kTilemap);
    luaApiSetSubtable(L, -1, kTilemapModule);
    registerTilemapMethods(L);

    // --- engine.event (Phase 42: pub/sub) ---
    static constexpr LuaApiEntry kEvent[] = {
        luaFunction("on", lua_engine_event_on, "(name:string, fn:function) -> id:int",
                    "Call fn() whenever name is emitted; returns the handler's id, 0 when a limit "
                    "is reached.",
                    "name: the event\n"
                    "fn: the handler, called with no arguments"),
        luaFunction("off", lua_engine_event_off, "(id:int) -> nil",
                    "Remove a handler; unknown ids are ignored.",
                    "id: the id engine.event.on returned"),
        luaFunction("emit", lua_engine_event_emit, "(name:string) -> nil",
                    "Call every handler of name now, with no arguments.",
                    "name: the event"),
    };
    static constexpr LuaApiModule kEventModule = luaApiModule(
        LuaApiScope::Table, "engine.event",
        "Named events: handlers are dropped on every reload and scene change.", kEvent);
    luaApiSetSubtable(L, -1, kEventModule);

    // --- engine.camera (Phase 44/48/57) — switchable ---
    static constexpr LuaApiEntry kCamera[] = {
        luaFunction("setPosition", lua_engine_camera_setPosition, "(x:number, y:number) -> nil",
                    "Move the camera.",
                    "x: world column\n"
                    "y: world row"),
        luaFunction("getPosition", lua_engine_camera_getPosition, "() -> x:number, y:number",
                    "The camera's position; 0, 0 without a camera."),
        luaFunction("lookAt", lua_engine_camera_lookAt,
                    "(x:number, y:number, speed:number?=1) -> nil",
                    "Glide the camera toward a point; a speed of 1 or more jumps there.",
                    "x: world column\n"
                    "y: world row\n"
                    "speed: how fast it glides; 0.1 covers the distance in about a second"),
        luaFunction("shake", lua_engine_camera_shake, "(intensity:number, duration:number) -> nil",
                    "Shake the camera.",
                    "intensity: how far it shakes, in pixels\n"
                    "duration: seconds"),
        luaFunction("setBounds", lua_engine_camera_setBounds,
                    "(minX:number, minY:number, maxX:number, maxY:number) -> nil",
                    "Keep the camera's position inside a rectangle.",
                    "minX: left limit\n"
                    "minY: top limit\n"
                    "maxX: right limit\n"
                    "maxY: bottom limit"),
        luaFunction("clearBounds", lua_engine_camera_clearBounds, "() -> nil",
                    "Let the camera move anywhere again."),
        luaFunction("follow", lua_engine_camera_follow,
                    "(target:ObjectProxy?, speed:number?=0.1) -> nil",
                    "Glide toward an object every frame; nil or a destroyed object stops following.",
                    "target: the object to follow\n"
                    "speed: the lookAt speed"),
        luaFunction("stopFollow", lua_engine_camera_stopFollow, "() -> nil",
                    "Stop following."),
        luaFunction("setDeadZone", lua_engine_camera_setDeadZone, "(w:number, h:number) -> nil",
                    "While following, hold still as long as the target stays in a box centred on "
                    "the camera.",
                    "w: box width; 0 turns the dead zone off, negative counts as 0\n"
                    "h: box height; 0 turns the dead zone off, negative counts as 0"),
    };
    static constexpr LuaApiModule kCameraModule = luaApiModule(
        LuaApiScope::Table, "engine.camera",
        "The active scene's camera; every call is a no-op without one.", kCamera);
    if (m_features.camera) luaApiSetSubtable(L, -1, kCameraModule);

    // --- engine.config (quick-007 API-07) ---
    static constexpr LuaApiEntry kConfig[] = {
        luaFunction("resolution", lua_engine_config_resolution, "() -> width:int, height:int",
                    "The size of the layer being drawn on; 0, 0 without one."),
    };
    static constexpr LuaApiModule kConfigModule = luaApiModule(
        LuaApiScope::Table, "engine.config", "The drawing surface's configuration.", kConfig);
    luaApiSetSubtable(L, -1, kConfigModule);

    // --- engine.state (quick-007 API-07: named states with enter/exit callbacks) ---
    static constexpr LuaApiEntry kState[] = {
        luaFunction("switch", lua_engine_state_switch, "(name:string) -> nil",
                    "Run the current state's on_exit, change state, then run the new state's "
                    "on_enter.",
                    "name: the new state; it need not have callbacks")
            .note("An error inside a callback is discarded silently."),
        luaFunction("current", lua_engine_state_current, "() -> string",
                    "The current state's name; \"none\" after a reload."),
        luaFunction("on_enter", lua_engine_state_on_enter, "(name:string, fn:function) -> nil",
                    "Set the callback run when switching into a state, replacing any earlier one.",
                    "name: the state; up to 63 bytes\n"
                    "fn: called with no arguments")
            .note("Up to 16 states have callbacks; more are ignored."),
        luaFunction("on_exit", lua_engine_state_on_exit, "(name:string, fn:function) -> nil",
                    "Set the callback run when switching out of a state, replacing any earlier one.",
                    "name: the state; up to 63 bytes\n"
                    "fn: called with no arguments")
            .note("Up to 16 states have callbacks; more are ignored."),
    };
    static constexpr LuaApiModule kStateModule = luaApiModule(
        LuaApiScope::Table, "engine.state",
        "One global named state with enter and exit callbacks, reset on every reload.", kState);
    luaApiSetSubtable(L, -1, kStateModule);

    // --- engine.physics (Phase 45: PHYS-09..PHYS-13); raycast is switchable ---
    static constexpr LuaApiEntry kPhysics[] = {
        luaFunction("setGravity", lua_engine_physics_setGravity, "(gx:number, gy:number) -> nil",
                    "Set the gravity applyGravity uses when a call gives none.",
                    "gx: acceleration along x\n"
                    "gy: acceleration along y (positive is down)"),
        luaFunction("getGravity", lua_engine_physics_getGravity, "() -> gx:number, gy:number",
                    "The gravity set by setGravity."),
        luaFunction("applyGravity", lua_engine_physics_applyGravity,
                    "(vx:number, vy:number, dt:number) -> vx:number, vy:number\n"
                    "(vx:number, vy:number, gx:number, gy:number, dt:number) -> vx:number, vy:number\n"
                    "(v:Vec2, dt:number) -> vx:number, vy:number\n"
                    "(v:Vec2, gx:number, gy:number, dt:number) -> vx:number, vy:number",
                    "A velocity after dt seconds of gravity: the set gravity, or gx, gy for this "
                    "call.",
                    "vx: velocity x\n"
                    "vy: velocity y\n"
                    "dt: seconds\n"
                    "gx: gravity x for this call\n"
                    "gy: gravity y for this call\n"
                    "v: the velocity"),
        luaFunction("bounce", lua_engine_physics_bounce,
                    "(vx:number, vy:number, nx:number, ny:number, restitution:number) -> "
                    "vx:number, vy:number\n"
                    "(v:Vec2, n:Vec2, restitution:number) -> vx:number, vy:number\n"
                    "(v:Vec2, nx:number, ny:number, restitution:number) -> vx:number, vy:number",
                    "A velocity reflected off a surface and scaled by restitution.",
                    "vx: velocity x\n"
                    "vy: velocity y\n"
                    "nx: surface normal x (unit length)\n"
                    "ny: surface normal y (unit length)\n"
                    "restitution: 1 keeps all the speed, 0 stops dead\n"
                    "v: the velocity\n"
                    "n: the surface normal (unit length)"),
        luaFunction("applyDrag", lua_engine_physics_applyDrag,
                    "(vx:number, vy:number, drag:number, dt:number) -> vx:number, vy:number\n"
                    "(v:Vec2, drag:number, dt:number) -> vx:number, vy:number",
                    "A velocity scaled by 1 - drag * dt, never below 0.",
                    "vx: velocity x\n"
                    "vy: velocity y\n"
                    "drag: the fraction lost per second\n"
                    "dt: seconds\n"
                    "v: the velocity"),
        luaFunction("springForce", lua_engine_physics_springForce,
                    "(pos:number, target:number, vel:number, stiffness:number, damping:number, "
                    "dt:number) -> vel:number",
                    "One axis of a damped spring: the velocity after dt seconds.",
                    "pos: the position\n"
                    "target: the rest position\n"
                    "vel: the velocity\n"
                    "stiffness: pull per unit of distance\n"
                    "damping: drag per unit of velocity\n"
                    "dt: seconds"),
        luaFunction("attract", lua_engine_physics_attract,
                    "(x:number, y:number, ax:number, ay:number, strength:number, "
                    "maxForce:number) -> fx:number, fy:number\n"
                    "(p:Vec2, a:Vec2, strength:number, maxForce:number) -> fx:number, fy:number",
                    "A force toward an attractor: strength / distance squared, capped at maxForce.",
                    "x: the point's column\n"
                    "y: the point's row\n"
                    "ax: the attractor's column\n"
                    "ay: the attractor's row\n"
                    "strength: force at distance 1\n"
                    "maxForce: the cap\n"
                    "p: the point\n"
                    "a: the attractor"),
        luaFunction("orbitVelocity", lua_engine_physics_orbitVelocity,
                    "(x:number, y:number, cx:number, cy:number, speed:number) -> "
                    "vx:number, vy:number\n"
                    "(p:Vec2, c:Vec2, speed:number) -> vx:number, vy:number",
                    "A velocity of the given speed that circles the centre clockwise on screen; "
                    "0, 0 at the centre.",
                    "x: the body's column\n"
                    "y: the body's row\n"
                    "cx: the centre's column\n"
                    "cy: the centre's row\n"
                    "speed: the speed; negative circles the other way\n"
                    "p: the body\n"
                    "c: the centre"),
        luaFunction("applyVelocity", lua_engine_physics_applyVelocity,
                    "(x:number, y:number, vx:number, vy:number, dt:number) -> x:number, y:number\n"
                    "(p:Vec2, v:Vec2, dt:number) -> x:number, y:number",
                    "A position moved by velocity times dt.",
                    "x: column\n"
                    "y: row\n"
                    "vx: velocity x\n"
                    "vy: velocity y\n"
                    "dt: seconds\n"
                    "p: the position\n"
                    "v: the velocity"),
    };
    static constexpr LuaApiModule kPhysicsModule = luaApiModule(
        LuaApiScope::Table, "engine.physics",
        "Stateless motion helpers: gravity, bounce, drag, springs and orbits.", kPhysics);
    static constexpr LuaApiEntry kRaycast[] = {
        luaFunction("raycast", lua_engine_physics_raycast,
                    "(x1:number, y1:number, x2:number, y2:number) -> "
                    "hit:boolean, x:number?, y:number?, dist:number?, what:string?",
                    "Cast a ray through the active scene: the first SOLID tile of its tilemap, "
                    "else the nearest object within 8 px.",
                    "x1: start column\n"
                    "y1: start row\n"
                    "x2: end column\n"
                    "y2: end row")
            .note("false without an active scene. what is \"tilemap\" or \"object\"; dist is "
                  "the fraction along the ray for a tile but pixels for an object, and a tile "
                  "hit may lie past the end point. Only the scene's first tilemap is tested."),
    };
    static constexpr LuaApiModule kRaycastModule = luaApiModule(
        LuaApiScope::Table, "engine.physics", "Ray casts through the active scene.", kRaycast);
    luaApiSetSubtable(L, -1, kPhysicsModule);
    if (m_features.raycast) {
        lua_getfield(L, -1, "physics");
        luaApiSetFields(L, -1, kRaycastModule);
        lua_pop(L, 1);
    }

    // --- engine.debug (Phase 47) — switchable ---
    if (m_features.debug) registerDebugSubtable(L);

    // --- engine.async (Phase 49: ASYNC-01..ASYNC-03) ---
    registerAsyncSubtable(L);

    // --- engine.tween (Phase 50: TWEEN-01..TWEEN-03) ---
    registerTweenSubtable(L);

    // --- engine.ui (Phase 52: UI-01..UI-04) ---
    registerUISubtable(L);

    // --- engine.hud (#83: RollingCounter / Timer value objects) ---
    registerHudSubtable(L);

    // --- engine.log (ENG-05) ---
    static constexpr LuaApiEntry kEngine[] = {
        luaFunction("log", lua_engine_log, "(...:any) -> nil",
                    "Write values to the log, tab-separated, ending with a newline.",
                    "...: strings and numbers print as text; other values print as their type")
            .note("The same output as print."),
    };
    static constexpr LuaApiModule kEngineModule = luaApiModule(
        LuaApiScope::Table, "engine", "The engine's services, one sub-table each.", kEngine);
    luaApiSetFields(L, -1, kEngineModule);

    lua_setglobal(L, "engine");                    // pops engine_table; stack is now balanced
}

// --- engine.config.resolution() -> width, height ---
int LuaBindings::lua_engine_config_resolution(lua_State* L) {
    LuaBindings* b = getBindings(L);
    if (!b || !b->currentCanvas) {
        lua_pushinteger(L, 0);
        lua_pushinteger(L, 0);
        return 2;
    }
    lua_pushinteger(L, b->currentCanvas->getWidth());
    lua_pushinteger(L, b->currentCanvas->getHeight());
    return 2;
}

// --- engine.state.current() -> string ---
int LuaBindings::lua_engine_state_current(lua_State* L) {
    LuaBindings* b = getBindings(L);
    if (!b) { lua_pushstring(L, "none"); return 1; }
    lua_pushstring(L, b->m_currentGameState);
    return 1;
}

// --- engine.state.switch(name) ---
// Fires on_exit for current state, changes state, fires on_enter for new state.
int LuaBindings::lua_engine_state_switch(lua_State* L) {
    LuaBindings* b = getBindings(L);
    if (!b) return 0;
    const char* newState = luaL_checkstring(L, 1);

    // Fire on_exit for current state
    for (int i = 0; i < b->m_stateCount; ++i) {
        if (strcmp(b->m_stateNames[i], b->m_currentGameState) == 0) {
            if (b->m_stateOnExitRefs[i] != LUA_NOREF) {
                lua_rawgeti(L, LUA_REGISTRYINDEX, b->m_stateOnExitRefs[i]);
                lua_pcall(L, 0, 0, 0);
            }
            break;
        }
    }

    // Change state
    strncpy(b->m_currentGameState, newState, sizeof(b->m_currentGameState) - 1);
    b->m_currentGameState[sizeof(b->m_currentGameState) - 1] = '\0';

    // Fire on_enter for new state
    for (int i = 0; i < b->m_stateCount; ++i) {
        if (strcmp(b->m_stateNames[i], newState) == 0) {
            if (b->m_stateOnEnterRefs[i] != LUA_NOREF) {
                lua_rawgeti(L, LUA_REGISTRYINDEX, b->m_stateOnEnterRefs[i]);
                lua_pcall(L, 0, 0, 0);
            }
            break;
        }
    }

    return 0;
}

// --- engine.state.on_enter(state_name, callback) ---
int LuaBindings::lua_engine_state_on_enter(lua_State* L) {
    LuaBindings* b = getBindings(L);
    if (!b) return 0;
    const char* name = luaL_checkstring(L, 1);
    luaL_checktype(L, 2, LUA_TFUNCTION);

    // Find or create slot (linear scan)
    int idx = -1;
    for (int i = 0; i < b->m_stateCount; ++i) {
        if (strcmp(b->m_stateNames[i], name) == 0) { idx = i; break; }
    }
    if (idx < 0) {
        if (b->m_stateCount >= MAX_GAME_STATES) return 0;  // full
        idx = b->m_stateCount++;
        strncpy(b->m_stateNames[idx], name, 63);
        b->m_stateNames[idx][63] = '\0';
        b->m_stateOnEnterRefs[idx] = LUA_NOREF;
        b->m_stateOnExitRefs[idx]  = LUA_NOREF;
    }

    // Unref existing callback if any
    if (b->m_stateOnEnterRefs[idx] != LUA_NOREF) {
        luaL_unref(L, LUA_REGISTRYINDEX, b->m_stateOnEnterRefs[idx]);
    }
    lua_pushvalue(L, 2);
    b->m_stateOnEnterRefs[idx] = luaL_ref(L, LUA_REGISTRYINDEX);
    return 0;
}

// --- engine.state.on_exit(state_name, callback) ---
int LuaBindings::lua_engine_state_on_exit(lua_State* L) {
    LuaBindings* b = getBindings(L);
    if (!b) return 0;
    const char* name = luaL_checkstring(L, 1);
    luaL_checktype(L, 2, LUA_TFUNCTION);

    // Find or create slot (linear scan)
    int idx = -1;
    for (int i = 0; i < b->m_stateCount; ++i) {
        if (strcmp(b->m_stateNames[i], name) == 0) { idx = i; break; }
    }
    if (idx < 0) {
        if (b->m_stateCount >= MAX_GAME_STATES) return 0;  // full
        idx = b->m_stateCount++;
        strncpy(b->m_stateNames[idx], name, 63);
        b->m_stateNames[idx][63] = '\0';
        b->m_stateOnEnterRefs[idx] = LUA_NOREF;
        b->m_stateOnExitRefs[idx]  = LUA_NOREF;
    }

    // Unref existing callback if any
    if (b->m_stateOnExitRefs[idx] != LUA_NOREF) {
        luaL_unref(L, LUA_REGISTRYINDEX, b->m_stateOnExitRefs[idx]);
    }
    lua_pushvalue(L, 2);
    b->m_stateOnExitRefs[idx] = luaL_ref(L, LUA_REGISTRYINDEX);
    return 0;
}

// --- engine.scene.switch(id) — ENG-01 ---
// Calls SceneStateMachine::switchTo(uint32_t). Silent no-op when SSM is nullptr
// (SDL standalone mode has no SceneStateMachine).
int LuaBindings::lua_engine_scene_switch(lua_State* L) {
    lua_getfield(L, LUA_REGISTRYINDEX, "enjin_ssm");
    auto** ssmPP = static_cast<SceneStateMachine**>(lua_touserdata(L, -1));
    lua_pop(L, 1);
    if (ssmPP == nullptr || *ssmPP == nullptr) { return 0; }  // no SSM installed — silent no-op
    uint32_t id = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    (*ssmPP)->switchTo(id);
    return 0;
}

// --- engine.scene.find(name) — ENG-02 ---
// Returns an ObjectProxy userdata with "ObjectProxy" metatable when found; nil when not found.
// Phase 37 upgrade complete: lightuserdata replaced with full ObjectProxy userdata.
// The proxy's valid flag is set false by Object::~Object() when the Object is destroyed.
// Silent nil-return when activeScene is nullptr.
int LuaBindings::lua_engine_scene_find(lua_State* L) {
    lua_getfield(L, LUA_REGISTRYINDEX, "enjin_active_scene");
    auto** scenePP = static_cast<Scene**>(lua_touserdata(L, -1));
    lua_pop(L, 1);
    if (scenePP == nullptr || *scenePP == nullptr) { lua_pushnil(L); return 1; }
    Scene* scene = *scenePP;
    const char* name = luaL_checkstring(L, 1);
    Object* obj = scene->findByName(name);
    // PERSIST-03: fallback — search persistent registry if not found in active scene
    if (!obj) {
        LuaBindings* b = LuaBindings::getBindings(L);
        if (b && b->m_ssm) {
            obj = b->m_ssm->findPersistentByName(name);
        }
    }
    if (!obj) {
        lua_pushnil(L);
        return 1;
    }

    // Allocate ObjectProxy userdata and attach the "ObjectProxy" metatable
    auto* proxy = static_cast<enjin2::ObjectProxy*>(
        lua_newuserdata(L, sizeof(enjin2::ObjectProxy)));
    proxy->object = obj;
    proxy->valid  = true;
    luaL_getmetatable(L, "ObjectProxy");
    lua_setmetatable(L, -2);

    // Register proxy with Object so its destructor can set valid = false
    obj->setLuaProxy(proxy);

    return 1;
}

// --- engine.scene.colliders() — ADR-0003 §4, Tomodachi #80 ---
// Returns a ColliderSet proxy over the active scene's scene-level collider
// resource (non-owning userdata). nil when no scene is active.
int LuaBindings::lua_engine_scene_colliders(lua_State* L) {
    lua_getfield(L, LUA_REGISTRYINDEX, "enjin_active_scene");
    auto** scenePP = static_cast<Scene**>(lua_touserdata(L, -1));
    lua_pop(L, 1);
    if (scenePP == nullptr || *scenePP == nullptr) { lua_pushnil(L); return 1; }
    return pushColliderSetProxy(L, &(*scenePP)->colliders());
}

// --- engine.scene.persist(proxy) — PERSIST-01 ---
// Extracts the object from the active scene and registers it as persistent.
// Returns true on success, nil when pool is full or proxy is invalid.
// Does NOT invalidate the proxy — object remains live as an external in the current scene.
int LuaBindings::lua_engine_scene_persist(lua_State* L) {
    auto* proxy = static_cast<enjin2::ObjectProxy*>(
        luaL_testudata(L, 1, "ObjectProxy"));
    if (!proxy || !proxy->valid || !proxy->object) {
        lua_pushnil(L);
        return 1;
    }

    LuaBindings* b = getBindings(L);
    if (!b || !b->m_ssm) {
        printf("[enjin] WARNING: engine.scene.persist() called without SceneStateMachine context — no-op\n");
        lua_pushnil(L);
        return 1;
    }

    bool ok = b->m_ssm->persistObject(proxy->object);
    if (!ok) {
        lua_pushnil(L);
        return 1;
    }
    lua_pushboolean(L, 1);
    return 1;
}

// --- engine.scene.unpersist(proxy) — PERSIST-02 ---
// Marks a persistent object for removal on the next scene transition.
// The object is destroyed (and proxy invalidated) when the transition fires.
// Returns true; silent no-op for invalid proxies.
int LuaBindings::lua_engine_scene_unpersist(lua_State* L) {
    auto* proxy = static_cast<enjin2::ObjectProxy*>(
        luaL_testudata(L, 1, "ObjectProxy"));
    if (!proxy || !proxy->valid || !proxy->object) {
        lua_pushnil(L);
        return 1;
    }

    LuaBindings* b = getBindings(L);
    if (!b || !b->m_ssm) {
        lua_pushnil(L);
        return 1;
    }

    b->m_ssm->unpersistObject(proxy->object);
    lua_pushboolean(L, 1);
    return 1;
}

// --- engine.scene.spawn([name]) ---
// Creates a new Object in the active scene. Every Object automatically gets a
// C_Position component. Returns an ObjectProxy userdata (same type as find()).
// Optional string argument sets the object's name.
// Returns nil when no active scene is available.
int LuaBindings::lua_engine_scene_spawn(lua_State* L) {
    lua_getfield(L, LUA_REGISTRYINDEX, "enjin_active_scene");
    auto** scenePP = static_cast<Scene**>(lua_touserdata(L, -1));
    lua_pop(L, 1);
    if (scenePP == nullptr || *scenePP == nullptr) { lua_pushnil(L); return 1; }
    Scene* scene = *scenePP;

    // Create a plain Object (which auto-adds C_Position)
    Object* obj = scene->addObject<Object>();
    if (!obj) {
        lua_pushnil(L);
        return 1;
    }

    // Optional name argument
    if (lua_gettop(L) >= 1 && lua_isstring(L, 1)) {
        // Intern the string in the Lua registry so it lives as long as the Lua state
        lua_pushvalue(L, 1);                                   // push the string
        int ref = luaL_ref(L, LUA_REGISTRYINDEX);              // anchor it
        // Retrieve the interned pointer (stable for the Lua state's lifetime)
        lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
        const char* interned = lua_tostring(L, -1);
        lua_pop(L, 1);
        obj->setName(interned);
        (void)ref;  // ref keeps string alive; leaked intentionally (lives until lua_close)
    }

    // Allocate ObjectProxy userdata and attach the "ObjectProxy" metatable
    auto* proxy = static_cast<enjin2::ObjectProxy*>(
        lua_newuserdata(L, sizeof(enjin2::ObjectProxy)));
    proxy->object = obj;
    proxy->valid  = true;
    luaL_getmetatable(L, "ObjectProxy");
    lua_setmetatable(L, -2);

    // Register proxy with Object so its destructor can set valid = false
    obj->setLuaProxy(proxy);

    return 1;
}

// --- engine.scene.destroy(proxy) ---
// Removes the Object referenced by the ObjectProxy from the active scene.
// The Object destructor automatically sets proxy->valid = false.
// Silently returns if the proxy is already invalid or no scene is active.
int LuaBindings::lua_engine_scene_destroy(lua_State* L) {
    auto* proxy = static_cast<enjin2::ObjectProxy*>(
        luaL_testudata(L, 1, "ObjectProxy"));
    if (!proxy || !proxy->valid || !proxy->object) { return 0; }

    lua_getfield(L, LUA_REGISTRYINDEX, "enjin_active_scene");
    auto** scenePP = static_cast<Scene**>(lua_touserdata(L, -1));
    lua_pop(L, 1);
    if (scenePP == nullptr || *scenePP == nullptr) { return 0; }

    (*scenePP)->removeObject(proxy->object);
    // Object::~Object() has already set proxy->valid = false at this point
    return 0;
}

// --- engine.time.delta() — ENG-04 ---
int LuaBindings::lua_engine_time_delta(lua_State* L) {
    lua_getfield(L, LUA_REGISTRYINDEX, "enjin_time");
    auto* ts = static_cast<EngineTimeState*>(lua_touserdata(L, -1));
    lua_pop(L, 1);
    lua_pushnumber(L, ts ? static_cast<lua_Number>(ts->dt) : 0.0);
    return 1;
}

// --- engine.time.now() — ENG-04 ---
int LuaBindings::lua_engine_time_now(lua_State* L) {
    lua_getfield(L, LUA_REGISTRYINDEX, "enjin_time");
    auto* ts = static_cast<EngineTimeState*>(lua_touserdata(L, -1));
    lua_pop(L, 1);
    lua_pushnumber(L, ts ? static_cast<lua_Number>(ts->totalTime) : 0.0);
    return 1;
}

// --- engine.time.frame() — ENG-04 ---
int LuaBindings::lua_engine_time_frame(lua_State* L) {
    lua_getfield(L, LUA_REGISTRYINDEX, "enjin_time");
    auto* ts = static_cast<EngineTimeState*>(lua_touserdata(L, -1));
    lua_pop(L, 1);
    lua_pushinteger(L, ts ? static_cast<lua_Integer>(ts->frameCount) : 0);
    return 1;
}

// --- engine.log(...) — ENG-05 ---
// Uses printf (not std::cout) — compatible with ESP32, Emscripten, and desktop.
// Handles all Lua types: strings/numbers coerce via lua_tostring;
// booleans/tables/nil fall back to lua_typename to avoid nullptr deref in printf.
int LuaBindings::lua_engine_log(lua_State* L) {
    int n = lua_gettop(L);
    for (int i = 1; i <= n; ++i) {
        const char* s = lua_tostring(L, i);
        if (s) {
            printf("%s", s);
        } else {
            // lua_tostring returns nullptr for boolean, table, function, nil
            printf("(%s)", lua_typename(L, lua_type(L, i)));
        }
        if (i < n) printf("\t");
    }
    printf("\n");
    return 0;
}

//==============================================================================
// engine.collision.* bindings
//==============================================================================

int LuaBindings::lua_engine_collision_aabb(lua_State* L) {
    float x1, y1, w1, h1, x2, y2, w2, h2;
    auto* r1 = static_cast<Rect*>(luaL_testudata(L, 1, "Rect"));
    auto* r2 = static_cast<Rect*>(luaL_testudata(L, 2, "Rect"));
    if (r1 && r2) {
        x1 = static_cast<float>(r1->x); y1 = static_cast<float>(r1->y);
        w1 = static_cast<float>(r1->width); h1 = static_cast<float>(r1->height);
        x2 = static_cast<float>(r2->x); y2 = static_cast<float>(r2->y);
        w2 = static_cast<float>(r2->width); h2 = static_cast<float>(r2->height);
    } else {
        x1 = static_cast<float>(luaL_checknumber(L, 1));
        y1 = static_cast<float>(luaL_checknumber(L, 2));
        w1 = static_cast<float>(luaL_checknumber(L, 3));
        h1 = static_cast<float>(luaL_checknumber(L, 4));
        x2 = static_cast<float>(luaL_checknumber(L, 5));
        y2 = static_cast<float>(luaL_checknumber(L, 6));
        w2 = static_cast<float>(luaL_checknumber(L, 7));
        h2 = static_cast<float>(luaL_checknumber(L, 8));
    }
    lua_pushboolean(L, enjin2::collision::aabb(x1, y1, w1, h1, x2, y2, w2, h2) ? 1 : 0);
    return 1;
}

int LuaBindings::lua_engine_collision_circleCircle(lua_State* L) {
    float x1 = static_cast<float>(luaL_checknumber(L, 1));
    float y1 = static_cast<float>(luaL_checknumber(L, 2));
    float r1 = static_cast<float>(luaL_checknumber(L, 3));
    float x2 = static_cast<float>(luaL_checknumber(L, 4));
    float y2 = static_cast<float>(luaL_checknumber(L, 5));
    float r2 = static_cast<float>(luaL_checknumber(L, 6));
    lua_pushboolean(L, enjin2::collision::circleCircle(x1, y1, r1, x2, y2, r2) ? 1 : 0);
    return 1;
}

int LuaBindings::lua_engine_collision_pointInRect(lua_State* L) {
    float px, py, rx, ry, rw, rh;
    auto* p = static_cast<Point*>(luaL_testudata(L, 1, "Point"));
    auto* v = static_cast<Vec2*>(luaL_testudata(L, 1, "Vec2"));
    auto* r = static_cast<Rect*>(luaL_testudata(L, 2, "Rect"));
    if ((p || v) && r) {
        px = p ? static_cast<float>(p->x) : v->x;
        py = p ? static_cast<float>(p->y) : v->y;
        rx = static_cast<float>(r->x); ry = static_cast<float>(r->y);
        rw = static_cast<float>(r->width); rh = static_cast<float>(r->height);
    } else {
        px = static_cast<float>(luaL_checknumber(L, 1));
        py = static_cast<float>(luaL_checknumber(L, 2));
        rx = static_cast<float>(luaL_checknumber(L, 3));
        ry = static_cast<float>(luaL_checknumber(L, 4));
        rw = static_cast<float>(luaL_checknumber(L, 5));
        rh = static_cast<float>(luaL_checknumber(L, 6));
    }
    lua_pushboolean(L, enjin2::collision::pointInRect(px, py, rx, ry, rw, rh) ? 1 : 0);
    return 1;
}

int LuaBindings::lua_engine_collision_pointInCircle(lua_State* L) {
    float px, py, cx, cy, r;
    auto* p = static_cast<Point*>(luaL_testudata(L, 1, "Point"));
    auto* v = static_cast<Vec2*>(luaL_testudata(L, 1, "Vec2"));
    if (p || v) {
        px = p ? static_cast<float>(p->x) : v->x;
        py = p ? static_cast<float>(p->y) : v->y;
        cx = static_cast<float>(luaL_checknumber(L, 2));
        cy = static_cast<float>(luaL_checknumber(L, 3));
        r  = static_cast<float>(luaL_checknumber(L, 4));
    } else {
        px = static_cast<float>(luaL_checknumber(L, 1));
        py = static_cast<float>(luaL_checknumber(L, 2));
        cx = static_cast<float>(luaL_checknumber(L, 3));
        cy = static_cast<float>(luaL_checknumber(L, 4));
        r  = static_cast<float>(luaL_checknumber(L, 5));
    }
    lua_pushboolean(L, enjin2::collision::pointInCircle(px, py, cx, cy, r) ? 1 : 0);
    return 1;
}

int LuaBindings::lua_engine_collision_lineLine(lua_State* L) {
    float x1 = static_cast<float>(luaL_checknumber(L, 1));
    float y1 = static_cast<float>(luaL_checknumber(L, 2));
    float x2 = static_cast<float>(luaL_checknumber(L, 3));
    float y2 = static_cast<float>(luaL_checknumber(L, 4));
    float x3 = static_cast<float>(luaL_checknumber(L, 5));
    float y3 = static_cast<float>(luaL_checknumber(L, 6));
    float x4 = static_cast<float>(luaL_checknumber(L, 7));
    float y4 = static_cast<float>(luaL_checknumber(L, 8));
    float ix, iy;
    bool hit = enjin2::collision::lineLine(x1, y1, x2, y2, x3, y3, x4, y4, &ix, &iy);
    lua_pushboolean(L, hit ? 1 : 0);
    if (hit) {
        lua_pushnumber(L, static_cast<lua_Number>(ix));
        lua_pushnumber(L, static_cast<lua_Number>(iy));
        return 3;
    }
    return 1;
}

int LuaBindings::lua_engine_collision_lineCircle(lua_State* L) {
    float x1 = static_cast<float>(luaL_checknumber(L, 1));
    float y1 = static_cast<float>(luaL_checknumber(L, 2));
    float x2 = static_cast<float>(luaL_checknumber(L, 3));
    float y2 = static_cast<float>(luaL_checknumber(L, 4));
    float cx = static_cast<float>(luaL_checknumber(L, 5));
    float cy = static_cast<float>(luaL_checknumber(L, 6));
    float r  = static_cast<float>(luaL_checknumber(L, 7));
    lua_pushboolean(L, enjin2::collision::lineCircle(x1, y1, x2, y2, cx, cy, r) ? 1 : 0);
    return 1;
}

// --- engine.lua.collect() — GC-01 ---
// Performs one incremental GC step. Uses LUA_GCSTEP (NOT LUA_GCCOLLECT) to avoid
// a stop-the-world pause that would spike frame budget on embedded targets (ESP32).
// data=0: one minimal step. Scripts can call multiple times per frame if needed.
int LuaBindings::lua_engine_lua_collect(lua_State* L) {
    lua_gc(L, LUA_GCSTEP, 0);
    return 0;
}

// --- engine.lua.memory() — GC-02 ---
// Returns Lua heap size in bytes as a number.
// Combines LUA_GCCOUNT (whole KB) + LUA_GCCOUNTB (remaining bytes) for exact byte count.
// Identical formula to LuaPlatform::getMemoryUsage() in lua_platform.cpp.
int LuaBindings::lua_engine_lua_memory(lua_State* L) {
    int kb  = lua_gc(L, LUA_GCCOUNT,  0);
    int rem = lua_gc(L, LUA_GCCOUNTB, 0);
    lua_pushnumber(L, static_cast<lua_Number>(kb * 1024 + rem));
    return 1;
}

//==============================================================================
// engine.collision.* response bindings
//==============================================================================

// --- engine.collision.aabbOverlap(x1,y1,w1,h1, x2,y2,w2,h2) ---
// Returns: hit, overlapX, overlapY, overlapW, overlapH
int LuaBindings::lua_engine_collision_aabbOverlap(lua_State* L) {
    float x1 = static_cast<float>(luaL_checknumber(L, 1));
    float y1 = static_cast<float>(luaL_checknumber(L, 2));
    float w1 = static_cast<float>(luaL_checknumber(L, 3));
    float h1 = static_cast<float>(luaL_checknumber(L, 4));
    float x2 = static_cast<float>(luaL_checknumber(L, 5));
    float y2 = static_cast<float>(luaL_checknumber(L, 6));
    float w2 = static_cast<float>(luaL_checknumber(L, 7));
    float h2 = static_cast<float>(luaL_checknumber(L, 8));
    float ox, oy, ow, oh;
    bool hit = enjin2::collision::aabbOverlap(x1, y1, w1, h1, x2, y2, w2, h2,
                                              &ox, &oy, &ow, &oh);
    lua_pushboolean(L, hit ? 1 : 0);
    if (hit) {
        lua_pushnumber(L, static_cast<lua_Number>(ox));
        lua_pushnumber(L, static_cast<lua_Number>(oy));
        lua_pushnumber(L, static_cast<lua_Number>(ow));
        lua_pushnumber(L, static_cast<lua_Number>(oh));
        return 5;
    }
    return 1;
}

// --- engine.collision.circleResponse(x1,y1,r1, x2,y2,r2) ---
// Returns: hit, normalX, normalY, depth
int LuaBindings::lua_engine_collision_circleResponse(lua_State* L) {
    float x1 = static_cast<float>(luaL_checknumber(L, 1));
    float y1 = static_cast<float>(luaL_checknumber(L, 2));
    float r1 = static_cast<float>(luaL_checknumber(L, 3));
    float x2 = static_cast<float>(luaL_checknumber(L, 4));
    float y2 = static_cast<float>(luaL_checknumber(L, 5));
    float r2 = static_cast<float>(luaL_checknumber(L, 6));
    float nx, ny, depth;
    bool hit = enjin2::collision::circleCircleResponse(x1, y1, r1, x2, y2, r2,
                                                       &nx, &ny, &depth);
    lua_pushboolean(L, hit ? 1 : 0);
    if (hit) {
        lua_pushnumber(L, static_cast<lua_Number>(nx));
        lua_pushnumber(L, static_cast<lua_Number>(ny));
        lua_pushnumber(L, static_cast<lua_Number>(depth));
        return 4;
    }
    return 1;
}

// --- engine.collision.reflect(vx, vy, nx, ny) ---
// Returns: reflectedVx, reflectedVy
int LuaBindings::lua_engine_collision_reflect(lua_State* L) {
    float vx = static_cast<float>(luaL_checknumber(L, 1));
    float vy = static_cast<float>(luaL_checknumber(L, 2));
    float nx = static_cast<float>(luaL_checknumber(L, 3));
    float ny = static_cast<float>(luaL_checknumber(L, 4));
    float outVx, outVy;
    enjin2::collision::reflect(vx, vy, nx, ny, &outVx, &outVy);
    lua_pushnumber(L, static_cast<lua_Number>(outVx));
    lua_pushnumber(L, static_cast<lua_Number>(outVy));
    return 2;
}

//==============================================================================
// engine.random.* bindings (seeded xorshift32 PRNG)
//==============================================================================

// xorshift32 step — inline helper
static uint32_t xorshift32(uint32_t& state) {
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return state;
}

// --- engine.random.seed(n) ---
// Sets the PRNG seed. If n==0, uses a non-zero default to avoid xorshift zero-lock.
int LuaBindings::lua_engine_random_seed(lua_State* L) {
    LuaBindings* b = getBindings(L);
    if (!b) return 0;
    uint32_t seed = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    b->m_rngState = (seed != 0) ? seed : 0x12345678;
    return 0;
}

// --- engine.random.integer(a, b) ---
// Returns a random integer in [a, b] inclusive.
int LuaBindings::lua_engine_random_integer(lua_State* L) {
    LuaBindings* b = getBindings(L);
    if (!b) { lua_pushinteger(L, 0); return 1; }
    int a = static_cast<int>(luaL_checkinteger(L, 1));
    int lo = static_cast<int>(luaL_checkinteger(L, 2));
    if (a > lo) { int tmp = a; a = lo; lo = tmp; }  // ensure a <= lo
    uint32_t r = xorshift32(b->m_rngState);
    int range = lo - a + 1;
    lua_pushinteger(L, a + static_cast<int>(r % static_cast<uint32_t>(range)));
    return 1;
}

// --- engine.random.float([a, b]) ---
// No args: returns [0, 1). Two args: returns [a, b).
int LuaBindings::lua_engine_random_float(lua_State* L) {
    LuaBindings* b = getBindings(L);
    if (!b) { lua_pushnumber(L, 0.0); return 1; }
    uint32_t r = xorshift32(b->m_rngState);
    double normalized = static_cast<double>(r) / static_cast<double>(0xFFFFFFFFu);
    int nargs = lua_gettop(L);
    if (nargs >= 2) {
        double a = luaL_checknumber(L, 1);
        double lo = luaL_checknumber(L, 2);
        lua_pushnumber(L, a + normalized * (lo - a));
    } else {
        lua_pushnumber(L, normalized);
    }
    return 1;
}

//==============================================================================
// engine.event.* bindings (Phase 42: scene-scoped pub/sub event bus)
//==============================================================================

// Helper: retrieve LuaEventBus* from Lua registry
static LuaEventBus* getEventBus(lua_State* L) {
    lua_getfield(L, LUA_REGISTRYINDEX, "enjin_event_bus");
    auto* bus = static_cast<LuaEventBus*>(lua_touserdata(L, -1));
    lua_pop(L, 1);
    return bus;
}

// --- engine.event.on(name, callback) -- EVENT-01 ---
// Returns subscription ID (integer > 0) on success, or 0 on failure.
static int lua_engine_event_on(lua_State* L) {
    LuaEventBus* bus = getEventBus(L);
    if (!bus) { lua_pushinteger(L, 0); return 1; }

    const char* name = luaL_checkstring(L, 1);
    luaL_checktype(L, 2, LUA_TFUNCTION);

    // Anchor the callback function in the Lua registry
    lua_pushvalue(L, 2);
    int ref = luaL_ref(L, LUA_REGISTRYINDEX);

    int id = bus->subscribe(name, ref);
    lua_pushinteger(L, static_cast<lua_Integer>(id));
    return 1;
}

// --- engine.event.off(id) -- EVENT-03 ---
// Unregisters a handler by subscription ID. Silent no-op for invalid IDs.
static int lua_engine_event_off(lua_State* L) {
    LuaEventBus* bus = getEventBus(L);
    if (!bus) return 0;

    int id = static_cast<int>(luaL_checkinteger(L, 1));
    bus->unsubscribe(id);
    return 0;
}

// --- engine.event.emit(name) -- EVENT-02 ---
// Fires all active callbacks for the named event. No payload arguments.
static int lua_engine_event_emit(lua_State* L) {
    LuaEventBus* bus = getEventBus(L);
    if (!bus) return 0;

    const char* name = luaL_checkstring(L, 1);
    bus->emit(name);
    return 0;
}

//==============================================================================
// engine.camera.* bindings (Phase 44: 2D camera system)
//==============================================================================

// Helper: retrieve active C_Camera from LuaBindings (follows input pointer pattern)
static C_Camera* getActiveCamera(lua_State* L) {
    LuaBindings* b = LuaBindings::getBindings(L);
    return b ? b->getActiveCamera() : nullptr;
}

// --- engine.camera.setPosition(x, y) ---
static int lua_engine_camera_setPosition(lua_State* L) {
    C_Camera* cam = getActiveCamera(L);
    if (!cam) return 0;  // silent no-op when no camera is active
    float x = static_cast<float>(luaL_checknumber(L, 1));
    float y = static_cast<float>(luaL_checknumber(L, 2));
    cam->setPosition(x, y);
    return 0;
}

// --- engine.camera.getPosition() -> x, y ---
static int lua_engine_camera_getPosition(lua_State* L) {
    C_Camera* cam = getActiveCamera(L);
    if (!cam) { lua_pushnumber(L, 0); lua_pushnumber(L, 0); return 2; }
    Vec2 pos = cam->getPosition();
    lua_pushnumber(L, static_cast<lua_Number>(pos.x));
    lua_pushnumber(L, static_cast<lua_Number>(pos.y));
    return 2;
}

// --- engine.camera.lookAt(x, y [, speed]) ---
static int lua_engine_camera_lookAt(lua_State* L) {
    C_Camera* cam = getActiveCamera(L);
    if (!cam) return 0;
    float x = static_cast<float>(luaL_checknumber(L, 1));
    float y = static_cast<float>(luaL_checknumber(L, 2));
    float speed = lua_gettop(L) >= 3 ? static_cast<float>(luaL_checknumber(L, 3)) : 1.0f;
    cam->lookAt(x, y, speed);
    return 0;
}

// --- engine.camera.shake(intensity, duration) ---
static int lua_engine_camera_shake(lua_State* L) {
    C_Camera* cam = getActiveCamera(L);
    if (!cam) return 0;
    float intensity = static_cast<float>(luaL_checknumber(L, 1));
    float duration = static_cast<float>(luaL_checknumber(L, 2));
    cam->shake(intensity, duration);
    return 0;
}

// --- engine.camera.setBounds(minX, minY, maxX, maxY) ---
static int lua_engine_camera_setBounds(lua_State* L) {
    C_Camera* cam = getActiveCamera(L);
    if (!cam) return 0;
    float minX = static_cast<float>(luaL_checknumber(L, 1));
    float minY = static_cast<float>(luaL_checknumber(L, 2));
    float maxX = static_cast<float>(luaL_checknumber(L, 3));
    float maxY = static_cast<float>(luaL_checknumber(L, 4));
    cam->setBounds(minX, minY, maxX, maxY);
    return 0;
}

// --- engine.camera.clearBounds() ---
static int lua_engine_camera_clearBounds(lua_State* L) {
    C_Camera* cam = getActiveCamera(L);
    if (!cam) return 0;
    cam->clearBounds();
    return 0;
}

//==============================================================================
// engine.camera.follow / stopFollow bindings (Phase 48: CAM-01, CAM-02)
//==============================================================================

// --- engine.camera.setDeadZone(w, h) --- Phase 57: QOL-03 ---
// Sets a rectangular dead zone centered on the camera's current position.
// While the follow target is inside the rectangle, the camera freezes (lookAt not called).
// setDeadZone(0, 0) disables the dead zone (camera follows normally).
// Negative values are clamped to 0.
int LuaBindings::lua_engine_camera_setDeadZone(lua_State* L) {
    LuaBindings* b = LuaBindings::getBindings(L);
    if (!b) return 0;
    float w = static_cast<float>(luaL_checknumber(L, 1));
    float h = static_cast<float>(luaL_checknumber(L, 2));
    if (w < 0.0f) w = 0.0f;
    if (h < 0.0f) h = 0.0f;
    b->m_deadZoneW = w;
    b->m_deadZoneH = h;
    return 0;
}

// --- engine.camera.follow(proxy [, speed]) --- CAM-01 ---
// Stores the follow target proxy in LuaBindings so tickCameraFollow() can track it each frame.
// If proxy is nil, invalid, or has a null object, silently clears the follow target (no error).
// Optional arg 2: lerp speed (default 0.1f).
// Implemented as LuaBindings member function to access private m_followTargetProxy/m_followSpeed.
int LuaBindings::lua_engine_camera_follow(lua_State* L) {
    LuaBindings* b = LuaBindings::getBindings(L);
    if (!b) return 0;

    auto* proxy = static_cast<ObjectProxy*>(luaL_testudata(L, 1, "ObjectProxy"));
    if (!proxy || !proxy->valid || !proxy->object) {
        // nil, non-proxy, invalid proxy, or destroyed object — silent stop
        b->m_followTargetProxy = nullptr;
        return 0;
    }

    float speed = (lua_gettop(L) >= 2) ? static_cast<float>(luaL_optnumber(L, 2, 0.1)) : 0.1f;
    b->m_followTargetProxy = proxy;
    b->m_followSpeed       = speed;
    return 0;
}

// --- engine.camera.stopFollow() --- CAM-02 ---
// Clears the follow target so the camera stops tracking. Silent no-op if not following.
// Implemented as LuaBindings member function to access private m_followTargetProxy.
int LuaBindings::lua_engine_camera_stopFollow(lua_State* L) {
    LuaBindings* b = LuaBindings::getBindings(L);
    if (!b) return 0;
    b->m_followTargetProxy = nullptr;
    return 0;
}

//==============================================================================
// LuaBindings::tickCameraFollow — per-frame follow tick (called from sdl_main.cpp)
//==============================================================================

void LuaBindings::tickCameraFollow(float /*dt*/) {
    if (!m_followTargetProxy) return;

    // Check if the proxy's target has been destroyed
    if (!m_followTargetProxy->valid || !m_followTargetProxy->object) {
        m_followTargetProxy = nullptr;  // target destroyed — silent stop
        return;
    }

    C_Camera* cam = getActiveCamera();
    if (!cam) return;

    auto* pos = m_followTargetProxy->object->getComponent<C_Position>();
    if (!pos) return;

    float targetX = static_cast<float>(pos->getPosition().x);
    float targetY = static_cast<float>(pos->getPosition().y);

    // Phase 57 QOL-03: Dead zone check — freeze camera if target is inside rectangle centered on camera
    if (m_deadZoneW > 0.0f && m_deadZoneH > 0.0f) {
        Vec2 camPos = cam->getPosition();
        float dx = targetX - camPos.x;
        float dy = targetY - camPos.y;
        if (dx < 0.0f) dx = -dx;
        if (dy < 0.0f) dy = -dy;
        if (dx <= m_deadZoneW * 0.5f && dy <= m_deadZoneH * 0.5f) {
            return;  // target inside dead zone — freeze camera
        }
    }

    cam->lookAt(targetX, targetY, m_followSpeed);
}

} // namespace enjin2
