#include "bindings_internal.hpp"
#include "../../include/enjin2/scripting/component_registry.hpp"
#include "../../include/enjin2/scripting/tilemap_lua.hpp"
#include "../../include/enjin2/scripting/lua_api.hpp"
#include "../../include/enjin2/components/position.hpp"
#include "../../include/enjin2/components/timer.hpp"
#include "../../include/enjin2/components/state_machine.hpp"
#include "../../include/enjin2/components/tilemap.hpp"
#include "../../include/enjin2/components/camera.hpp"
#include "../../include/enjin2/components/sprite.hpp"
#include "../../include/enjin2/components/body.hpp"
#include "../../include/enjin2/core/colliders.hpp"
#include "../../include/enjin2/components/lua_script.hpp"
#include "../../include/enjin2/core/object.hpp"
#include "../../include/enjin2/core/scene.hpp"
#include <cctype>

namespace enjin2 {

// Registry tables of the proxy methods their __index looks up (setProxyMethods).
static constexpr const char* kObjectProxyMethods = "ObjectProxy.methods";
static constexpr const char* kPositionMethods    = "C_Position.methods";
static constexpr const char* kSpriteMethods      = "C_Sprite.methods";
static constexpr const char* kBodyMethods        = "C_Body.methods";

//==============================================================================
// Registry-generated component dispatch (ADR-0003 §2)
//
// The `add`/`get` verbs on both object-level proxies bottom out here, so the
// only place that maps a component name string to a C++ type is the
// ENJIN2_COMPONENT_LIST in component_registry.hpp. These push one Lua value
// (a ComponentProxy userdata, or nil) and return 1.
//==============================================================================

// Allocate a ComponentProxy userdata for `comp`, attach its per-type metatable,
// and register it for destructor invalidation. Pushes nil for a null component.
static int pushComponentProxyUserdata(lua_State* L, Component* comp, const char* metaName) {
    if (!comp) { lua_pushnil(L); return 1; }
    auto* cproxy = static_cast<enjin2::ComponentProxy*>(
        lua_newuserdata(L, sizeof(enjin2::ComponentProxy)));
    cproxy->component = comp;
    cproxy->valid = true;
    luaL_getmetatable(L, metaName);
    lua_setmetatable(L, -2);
    // Overwrites any previous proxy (single-proxy-per-component constraint —
    // accepted v1.6 limitation, matching self:get()).
    comp->setLuaProxy(cproxy);
    return 1;
}

int pushComponentGet(lua_State* L, Object* owner, const char* typeName) {
    if (!owner) { lua_pushnil(L); return 1; }
    const ComponentRegistryEntry* e = findComponentEntry(typeName);
    if (!e) { lua_pushnil(L); return 1; }
    return pushComponentProxyUserdata(L, e->get(owner), e->proxyMeta);
}

int pushComponentAdd(lua_State* L, Object* owner, const char* typeName, int paramsIdx) {
    if (!owner) { lua_pushnil(L); return 1; }
    const ComponentRegistryEntry* e = findComponentEntry(typeName);
    if (!e) {
        luaL_error(L, "add: unknown component '%s'", typeName ? typeName : "?");
        return 0;  // unreachable — luaL_error longjmps
    }
    // Attach-or-fetch: never duplicate a component already on the object.
    Component* comp = e->get(owner);
    if (!comp) comp = e->add(owner);
    pushComponentProxyUserdata(L, comp, e->proxyMeta);  // proxy (or nil) now on top

    // Apply an optional params table by writing each field through the proxy's
    // __newindex — the writable-add half of ADR-0003 §2. Fields the component's
    // proxy does not handle are silently dropped (matching its own __newindex).
    if (comp && paramsIdx > 0 && lua_istable(L, paramsIdx)) {
        int proxyIdx = lua_gettop(L);  // absolute index of the proxy userdata
        lua_pushnil(L);
        while (lua_next(L, paramsIdx) != 0) {
            // stack: ... key value  → write proxy[key] = value via __newindex
            lua_pushvalue(L, -2);       // key
            lua_pushvalue(L, -2);       // value
            lua_settable(L, proxyIdx);
            lua_pop(L, 1);              // pop value, keep key for the next lua_next
        }
    }
    return 1;
}

//==============================================================================
// C_Position_Proxy Metatable Implementation (Phase 39: ComponentProxy proof-of-concept)
//==============================================================================

// pos:getX() / pos:getY() — kept for back-compat beside the x/y fields.
static int lua_cposition_getX(lua_State* L) {
    auto* p = static_cast<enjin2::ComponentProxy*>(luaL_checkudata(L, 1, CPOSITION_PROXY_METATABLE));
    if (!p || !p->valid || !p->component) return luaL_error(L, "component has been destroyed");
    auto* pos = static_cast<enjin2::C_Position*>(p->component);
    lua_pushinteger(L, static_cast<lua_Integer>(pos->getPosition().x));
    return 1;
}

static int lua_cposition_getY(lua_State* L) {
    auto* p = static_cast<enjin2::ComponentProxy*>(luaL_checkudata(L, 1, CPOSITION_PROXY_METATABLE));
    if (!p || !p->valid || !p->component) return luaL_error(L, "component has been destroyed");
    auto* pos = static_cast<enjin2::C_Position*>(p->component);
    lua_pushinteger(L, static_cast<lua_Integer>(pos->getPosition().y));
    return 1;
}

// __index metamethod for C_Position_Proxy: x/y fields, then the methods.
// Stale access raises luaL_error (PROXY-04).
static int lua_cposition_proxy_index_impl(lua_State* L) {
    enjin2::ComponentProxy* proxy = static_cast<enjin2::ComponentProxy*>(
        luaL_checkudata(L, 1, CPOSITION_PROXY_METATABLE));
    if (!proxy || !proxy->valid || !proxy->component) {
        luaL_error(L, "component has been destroyed");
        return 0;
    }

    const char* key = lua_tostring(L, 2);
    if (!key) { lua_pushnil(L); return 1; }

    // Writable position properties (ADR-0003 §2: every component proxy gains the
    // write side). `p.x`/`p.y` read here; the paired __newindex writes via the
    // C_Position setter.
    auto* pos = static_cast<enjin2::C_Position*>(proxy->component);
    if (strcmp(key, "x") == 0) {
        lua_pushinteger(L, static_cast<lua_Integer>(pos->getPosition().x));
        return 1;
    } else if (strcmp(key, "y") == 0) {
        lua_pushinteger(L, static_cast<lua_Integer>(pos->getPosition().y));
        return 1;
    }
    if (pushProxyMethod(L, kPositionMethods, 2)) return 1;

    lua_pushnil(L);
    return 1;
}

// __newindex for C_Position_Proxy: `p.x = N` / `p.y = M` route straight to the
// C_Position setter — the writable-proxy half of ADR-0003 §2. Writes to any
// other key are silently ignored, matching the object-level proxies.
static int lua_cposition_proxy_newindex_impl(lua_State* L) {
    enjin2::ComponentProxy* proxy = static_cast<enjin2::ComponentProxy*>(
        luaL_checkudata(L, 1, CPOSITION_PROXY_METATABLE));
    if (!proxy || !proxy->valid || !proxy->component) {
        luaL_error(L, "component has been destroyed");
        return 0;
    }
    const char* key = lua_tostring(L, 2);
    if (!key) return 0;

    auto* pos = static_cast<enjin2::C_Position*>(proxy->component);
    if (strcmp(key, "x") == 0) {
        pos->setPosition(static_cast<int16_t>(luaL_checkinteger(L, 3)),
                         pos->getPosition().y);
    } else if (strcmp(key, "y") == 0) {
        pos->setPosition(pos->getPosition().x,
                         static_cast<int16_t>(luaL_checkinteger(L, 3)));
    }
    return 0;
}

//==============================================================================
// C_Timer_Proxy Metatable Implementation (Phase 40: timer:after/every/cancel)
//==============================================================================

// timer:after(seconds, fn) — TIMER-01
static int lua_timer_after(lua_State* L) {
    auto* proxy = static_cast<enjin2::ComponentProxy*>(
        luaL_checkudata(L, 1, CTIMER_PROXY_METATABLE));
    if (!proxy || !proxy->valid || !proxy->component) {
        luaL_error(L, "component has been destroyed");
        return 0;
    }
    float seconds = static_cast<float>(luaL_checknumber(L, 2));
    luaL_checktype(L, 3, LUA_TFUNCTION);

    // Anchor the callback in the Lua registry
    lua_pushvalue(L, 3);
    int ref = luaL_ref(L, LUA_REGISTRYINDEX);

    auto* timer = static_cast<enjin2::C_Timer*>(proxy->component);
    timer->setLuaState(L);
    int id = timer->scheduleAfter(seconds, ref);
    lua_pushinteger(L, static_cast<lua_Integer>(id));
    return 1;
}

// timer:every(seconds, fn) — TIMER-02
static int lua_timer_every(lua_State* L) {
    auto* proxy = static_cast<enjin2::ComponentProxy*>(
        luaL_checkudata(L, 1, CTIMER_PROXY_METATABLE));
    if (!proxy || !proxy->valid || !proxy->component) {
        luaL_error(L, "component has been destroyed");
        return 0;
    }
    float seconds = static_cast<float>(luaL_checknumber(L, 2));
    luaL_checktype(L, 3, LUA_TFUNCTION);

    lua_pushvalue(L, 3);
    int ref = luaL_ref(L, LUA_REGISTRYINDEX);

    auto* timer = static_cast<enjin2::C_Timer*>(proxy->component);
    timer->setLuaState(L);
    int id = timer->scheduleEvery(seconds, ref);
    lua_pushinteger(L, static_cast<lua_Integer>(id));
    return 1;
}

// timer:cancel(id) — TIMER-03
static int lua_timer_cancel(lua_State* L) {
    auto* proxy = static_cast<enjin2::ComponentProxy*>(
        luaL_checkudata(L, 1, CTIMER_PROXY_METATABLE));
    if (!proxy || !proxy->valid || !proxy->component) {
        luaL_error(L, "component has been destroyed");
        return 0;
    }
    int timerId = static_cast<int>(luaL_checkinteger(L, 2));
    auto* timer = static_cast<enjin2::C_Timer*>(proxy->component);
    timer->cancel(timerId);
    return 0;
}

//==============================================================================
// C_StateMachine_Proxy Metatable Implementation (Phase 41: fsm:addState/setState/getState)
//==============================================================================

// fsm:addState(name, {enter=fn, exit=fn, update=fn}) — FSM-01
static int lua_fsm_addState(lua_State* L) {
    auto* proxy = static_cast<enjin2::ComponentProxy*>(
        luaL_checkudata(L, 1, CFSM_PROXY_METATABLE));
    if (!proxy || !proxy->valid || !proxy->component) {
        luaL_error(L, "component has been destroyed");
        return 0;
    }
    const char* name = luaL_checkstring(L, 2);
    luaL_checktype(L, 3, LUA_TTABLE);

    auto* fsm = static_cast<enjin2::C_StateMachine*>(proxy->component);
    fsm->setLuaState(L);

    // Extract optional callbacks from table
    int enterRef = LUA_NOREF, exitRef = LUA_NOREF, updateRef = LUA_NOREF;

    lua_getfield(L, 3, "enter");
    if (lua_isfunction(L, -1)) enterRef = luaL_ref(L, LUA_REGISTRYINDEX);
    else lua_pop(L, 1);

    lua_getfield(L, 3, "exit");
    if (lua_isfunction(L, -1)) exitRef = luaL_ref(L, LUA_REGISTRYINDEX);
    else lua_pop(L, 1);

    lua_getfield(L, 3, "update");
    if (lua_isfunction(L, -1)) updateRef = luaL_ref(L, LUA_REGISTRYINDEX);
    else lua_pop(L, 1);

    if (!fsm->addState(name, enterRef, exitRef, updateRef)) {
        // Cleanup refs if addState fails (name too long or array full)
        if (enterRef  != LUA_NOREF) luaL_unref(L, LUA_REGISTRYINDEX, enterRef);
        if (exitRef   != LUA_NOREF) luaL_unref(L, LUA_REGISTRYINDEX, exitRef);
        if (updateRef != LUA_NOREF) luaL_unref(L, LUA_REGISTRYINDEX, updateRef);
        luaL_error(L, "C_StateMachine: too many states or name too long");
    }
    return 0;
}

// fsm:setState(name) — FSM-02, FSM-04 (deferred)
static int lua_fsm_setState(lua_State* L) {
    auto* proxy = static_cast<enjin2::ComponentProxy*>(
        luaL_checkudata(L, 1, CFSM_PROXY_METATABLE));
    if (!proxy || !proxy->valid || !proxy->component) {
        luaL_error(L, "component has been destroyed");
        return 0;
    }
    const char* name = luaL_checkstring(L, 2);
    auto* fsm = static_cast<enjin2::C_StateMachine*>(proxy->component);
    fsm->setState(name);
    return 0;
}

// fsm:getState() — FSM-03
static int lua_fsm_getState(lua_State* L) {
    auto* proxy = static_cast<enjin2::ComponentProxy*>(
        luaL_checkudata(L, 1, CFSM_PROXY_METATABLE));
    if (!proxy || !proxy->valid || !proxy->component) {
        luaL_error(L, "component has been destroyed");
        return 0;
    }
    auto* fsm = static_cast<enjin2::C_StateMachine*>(proxy->component);
    lua_pushstring(L, fsm->getState());
    return 1;
}

//==============================================================================
// C_Tilemap_Proxy Metatable Implementation (Phase 43: tilemap Lua API)
//==============================================================================

// Every method resolves `self` — a map handle or a scene C_Tilemap proxy — with
// lua::checkTilemap (tilemap_lua.hpp).

// tilemap:setTile(tx, ty, cell) — TMAP-05
// `cell` is the full 16-bit packed cell (tile id in the low 9 bits); a plain
// tile id 0-511 works as-is (band/flip/palbank = 0).
static int lua_tilemap_setTile(lua_State* L) {
    auto* tm = lua::checkTilemap(L, 1);
    uint8_t  tx   = static_cast<uint8_t>(luaL_checkinteger(L, 2));
    uint8_t  ty   = static_cast<uint8_t>(luaL_checkinteger(L, 3));
    uint16_t cell = static_cast<uint16_t>(luaL_checkinteger(L, 4) & 0xFFFF);
    tm->setTile(tx, ty, cell);
    return 0;
}

// tilemap:getTile(tx, ty) -> tileId — TMAP-05
static int lua_tilemap_getTile(lua_State* L) {
    auto* tm = lua::checkTilemap(L, 1);
    uint8_t tx = static_cast<uint8_t>(luaL_checkinteger(L, 2));
    uint8_t ty = static_cast<uint8_t>(luaL_checkinteger(L, 3));
    lua_pushinteger(L, static_cast<lua_Integer>(tm->getTile(tx, ty)));
    return 1;
}

// tilemap:setTiles(flat_table, w, h) — TMAP-07
static int lua_tilemap_setTiles(lua_State* L) {
    auto* tm = lua::checkTilemap(L, 1);
    luaL_checktype(L, 2, LUA_TTABLE);
    uint8_t w = static_cast<uint8_t>(luaL_checkinteger(L, 3));
    uint8_t h = static_cast<uint8_t>(luaL_checkinteger(L, 4));
    if (w > 64) w = 64;
    if (h > 64) h = 64;
    uint8_t buf[64 * 64] = {};
    int count = w * h;
    for (int i = 0; i < count; ++i) {
        lua_rawgeti(L, 2, i + 1);  // Lua 1-indexed
        buf[i] = static_cast<uint8_t>(lua_tointeger(L, -1) & 0xFF);
        lua_pop(L, 1);
    }
    tm->setTiles(buf, w, h);
    return 0;
}

// tilemap:setSheet(handle) — TMAP-08
// handle is an integer index into the LuaBindings sprite pool.
static int lua_tilemap_setSheet(lua_State* L) {
    auto* tm = lua::checkTilemap(L, 1);
    int handle = static_cast<int>(luaL_checkinteger(L, 2));
    LuaBindings* b = LuaBindings::getBindings(L);
    if (!b) {
        luaL_error(L, "C_Tilemap.setSheet: LuaBindings not available");
        return 0;
    }
    const enjin2::SpriteSheet* sheet = b->getSpriteSheet(handle);
    if (!sheet) {
        luaL_error(L, "C_Tilemap.setSheet: invalid sprite handle %d", handle);
        return 0;
    }
    tm->setSheet(*sheet);
    return 0;
}

// tilemap:setScroll(sx, sy) — TMAP-04/05
static int lua_tilemap_setScroll(lua_State* L) {
    auto* tm = lua::checkTilemap(L, 1);
    int16_t sx = static_cast<int16_t>(luaL_checkinteger(L, 2));
    int16_t sy = static_cast<int16_t>(luaL_checkinteger(L, 3));
    tm->setScroll(sx, sy);
    return 0;
}

// tilemap:getScroll() -> sx, sy — TMAP-04/05
static int lua_tilemap_getScroll(lua_State* L) {
    auto* tm = lua::checkTilemap(L, 1);
    lua_pushinteger(L, static_cast<lua_Integer>(tm->getScrollX()));
    lua_pushinteger(L, static_cast<lua_Integer>(tm->getScrollY()));
    return 2;
}

// tilemap:pixelToTile(px, py) -> tx, ty — TMAP-06
static int lua_tilemap_pixelToTile(lua_State* L) {
    auto* tm = lua::checkTilemap(L, 1);
    int16_t px = static_cast<int16_t>(luaL_checkinteger(L, 2));
    int16_t py = static_cast<int16_t>(luaL_checkinteger(L, 3));
    int16_t tx = 0, ty = 0;
    tm->pixelToTile(px, py, tx, ty);
    lua_pushinteger(L, static_cast<lua_Integer>(tx));
    lua_pushinteger(L, static_cast<lua_Integer>(ty));
    return 2;
}

// tilemap:tileToPixel(tx, ty) -> px, py — TMAP-06
static int lua_tilemap_tileToPixel(lua_State* L) {
    auto* tm = lua::checkTilemap(L, 1);
    int16_t tx = static_cast<int16_t>(luaL_checkinteger(L, 2));
    int16_t ty = static_cast<int16_t>(luaL_checkinteger(L, 3));
    int16_t px = 0, py = 0;
    tm->tileToPixel(tx, ty, px, py);
    lua_pushinteger(L, static_cast<lua_Integer>(px));
    lua_pushinteger(L, static_cast<lua_Integer>(py));
    return 2;
}

// tilemap:tileAtPixel(px, py) -> tileId — TMAP-06
static int lua_tilemap_tileAtPixel(lua_State* L) {
    auto* tm = lua::checkTilemap(L, 1);
    int16_t px = static_cast<int16_t>(luaL_checkinteger(L, 2));
    int16_t py = static_cast<int16_t>(luaL_checkinteger(L, 3));
    lua_pushinteger(L, static_cast<lua_Integer>(tm->tileAtPixel(px, py)));
    return 1;
}

// tilemap:getMapSize() -> w, h — TMAP-05
static int lua_tilemap_getMapSize(lua_State* L) {
    auto* tm = lua::checkTilemap(L, 1);
    lua_pushinteger(L, static_cast<lua_Integer>(tm->getMapWidth()));
    lua_pushinteger(L, static_cast<lua_Integer>(tm->getMapHeight()));
    return 2;
}

// tilemap:setAttrs(flatTable) — flat {flags,kind, flags,kind, …} (ADR-0003 §3)
static int lua_tilemap_setAttrs(lua_State* L) {
    auto* tm = lua::checkTilemap(L, 1);
    luaL_checktype(L, 2, LUA_TTABLE);
    const int n = static_cast<int>(lua_rawlen(L, 2));
    int count = n / 2;
    if (count > TM_MAX_TILES) count = TM_MAX_TILES;

    TileAttr attrs[TM_MAX_TILES];
    for (int i = 0; i < count; ++i) {
        lua_rawgeti(L, 2, i * 2 + 1);
        attrs[i].flags = static_cast<uint8_t>(lua_tointeger(L, -1) & 0xFF);
        lua_pop(L, 1);
        lua_rawgeti(L, 2, i * 2 + 2);
        attrs[i].kind = static_cast<uint8_t>(lua_tointeger(L, -1) & 0xFF);
        lua_pop(L, 1);
    }
    tm->setAttrs(attrs, static_cast<uint16_t>(count));
    return 0;
}

// tilemap:setPalbank(index, lutTable) — a 16-entry index→index remap
static int lua_tilemap_setPalbank(lua_State* L) {
    auto* tm = lua::checkTilemap(L, 1);
    const uint8_t index = static_cast<uint8_t>(luaL_checkinteger(L, 2) & 0x0F);
    luaL_checktype(L, 3, LUA_TTABLE);
    Remap r;
    for (int i = 0; i < 16; ++i) {
        lua_rawgeti(L, 3, i + 1);
        r.lut[i] = static_cast<uint8_t>(lua_tointeger(L, -1) & 0x0F);
        lua_pop(L, 1);
    }
    tm->setPalbank(index, r);
    return 0;
}

// tilemap:attrAt(tx, ty) -> flags, kind (DIR already flip-resolved)
static int lua_tilemap_attrAt(lua_State* L) {
    auto* tm = lua::checkTilemap(L, 1);
    const uint8_t tx = static_cast<uint8_t>(luaL_checkinteger(L, 2));
    const uint8_t ty = static_cast<uint8_t>(luaL_checkinteger(L, 3));
    const TileAttr a = tm->attrAt(tx, ty);
    lua_pushinteger(L, a.flags);
    lua_pushinteger(L, a.kind);
    return 2;
}

// tilemap:attrAtPixel(px, py) -> flags, kind
static int lua_tilemap_attrAtPixel(lua_State* L) {
    auto* tm = lua::checkTilemap(L, 1);
    const int16_t px = static_cast<int16_t>(luaL_checkinteger(L, 2));
    const int16_t py = static_cast<int16_t>(luaL_checkinteger(L, 3));
    const TileAttr a = tm->attrAtPixel(px, py);
    lua_pushinteger(L, a.flags);
    lua_pushinteger(L, a.kind);
    return 2;
}

// Helper: push a SweepResult's fields onto the Lua stack (x, y, t, nx, ny, hit).
static void pushSweepResult(lua_State* L, const SweepResult& r) {
    lua_pushnumber(L, static_cast<lua_Number>(r.x));
    lua_pushnumber(L, static_cast<lua_Number>(r.y));
    lua_pushnumber(L, static_cast<lua_Number>(r.t));
    lua_pushnumber(L, static_cast<lua_Number>(r.normalX));
    lua_pushnumber(L, static_cast<lua_Number>(r.normalY));
    lua_pushboolean(L, r.hit ? 1 : 0);
}

// tilemap:sweepAabb(x, y, w, h, vx, vy, dt) -> x, y, t, nx, ny, hit
static int lua_tilemap_sweepAabb(lua_State* L) {
    auto* tm = lua::checkTilemap(L, 1);
    const float x  = static_cast<float>(luaL_checknumber(L, 2));
    const float y  = static_cast<float>(luaL_checknumber(L, 3));
    const float w  = static_cast<float>(luaL_checknumber(L, 4));
    const float h  = static_cast<float>(luaL_checknumber(L, 5));
    const float vx = static_cast<float>(luaL_checknumber(L, 6));
    const float vy = static_cast<float>(luaL_checknumber(L, 7));
    const float dt = static_cast<float>(luaL_checknumber(L, 8));
    pushSweepResult(L, tm->sweepAabb(x, y, w, h, vx, vy, dt));
    return 6;
}

// tilemap:sweepCircle(cx, cy, r, vx, vy, dt) -> x, y, t, nx, ny, hit
static int lua_tilemap_sweepCircle(lua_State* L) {
    auto* tm = lua::checkTilemap(L, 1);
    const float cx = static_cast<float>(luaL_checknumber(L, 2));
    const float cy = static_cast<float>(luaL_checknumber(L, 3));
    const float r  = static_cast<float>(luaL_checknumber(L, 4));
    const float vx = static_cast<float>(luaL_checknumber(L, 5));
    const float vy = static_cast<float>(luaL_checknumber(L, 6));
    const float dt = static_cast<float>(luaL_checknumber(L, 7));
    pushSweepResult(L, tm->sweepCircle(cx, cy, r, vx, vy, dt));
    return 6;
}

// tilemap:forEachCellIn(x, y, w, h, fn) — calls fn(tx, ty, cell) per overlapped cell
static int lua_tilemap_forEachCellIn(lua_State* L) {
    auto* tm = lua::checkTilemap(L, 1);
    const float x = static_cast<float>(luaL_checknumber(L, 2));
    const float y = static_cast<float>(luaL_checknumber(L, 3));
    const float w = static_cast<float>(luaL_checknumber(L, 4));
    const float h = static_cast<float>(luaL_checknumber(L, 5));
    luaL_checktype(L, 6, LUA_TFUNCTION);
    tm->forEachCellIn(x, y, w, h, [&](uint8_t tx, uint8_t ty, uint16_t cell) {
        lua_pushvalue(L, 6);                    // the callback (still at index 6)
        lua_pushinteger(L, static_cast<lua_Integer>(tx));
        lua_pushinteger(L, static_cast<lua_Integer>(ty));
        lua_pushinteger(L, static_cast<lua_Integer>(cell));
        lua_call(L, 3, 0);
    });
    return 0;
}

// tilemap:buildSolidRects() -> flat table {x,y,w,h, x,y,w,h, …}
static int lua_tilemap_buildSolidRects(lua_State* L) {
    auto* tm = lua::checkTilemap(L, 1);
    const std::vector<Rect> rects = tm->buildSolidRects();
    lua_createtable(L, static_cast<int>(rects.size() * 4), 0);
    int idx = 1;
    for (const Rect& r : rects) {
        lua_pushinteger(L, r.x);               lua_rawseti(L, -2, idx++);
        lua_pushinteger(L, r.y);               lua_rawseti(L, -2, idx++);
        lua_pushinteger(L, r.width);           lua_rawseti(L, -2, idx++);
        lua_pushinteger(L, r.height);          lua_rawseti(L, -2, idx++);
    }
    return 1;
}

// engine.tilemap.load(name) -> map handle (ADR-0003 §6, #91; scene-free #256).
//
// Returns a Lua-owned map handle (tilemap_lua.hpp) populated from the per-applet
// asset root: `<name>.njn` supplies the tileset sheet + the inline ATTR table
// (#51), `<name>.njm` supplies the packed cells. No scene is involved: a host
// draws the handle through its restore source (gfx.setTilemap) and the
// collision helpers (attrAt, sweepAabb, …) are its methods.
//
// Lifetime: the handle is collected like any Lua value. The tileset pixels live
// in the shared asset arena and occupy one sprite-pool slot until
// resetSpritePool() runs in registerAll(). Hosts call that on a fresh VM, so no
// handle outlives the pixels; a host that re-runs registerAll() on a live VM
// must drop its map handles first. Same model (and
// the same 16-slot / 64 KB ceiling) as engine.sprite.load. Collecting the handle
// does NOT reclaim the slot; loading many maps in one VM exhausts the pool.
// Maps larger than 64×64 are cropped to the component's fixed grid.
int LuaBindings::lua_loadTilemap(lua_State* L) {
    LuaBindings* b = getBindings(L);
    if (!b) { lua_pushnil(L); return 1; }

    const char* name = luaL_checkstring(L, 1);
    if (!name) { lua_pushnil(L); return 1; }

    // Allocate the handle first: a Lua memory error then fails before any slot
    // is pinned, and an error below just leaves the empty handle to the GC.
    C_Tilemap* tm = lua::pushTilemap(L);

    // Reserve a sprite-pool slot to own the tileset pixels for the map's lifetime.
    int slot = -1;
    for (int i = 0; i < LUA_SPRITE_POOL_SIZE; ++i) {
        if (!b->spritePool[i].active) { slot = i; break; }
    }
    if (slot < 0) {
        luaL_error(L, "engine.tilemap.load: sprite pool full (max %d)", LUA_SPRITE_POOL_SIZE);
        return 0;
    }

    std::string base = b->assetPath_;
    if (!base.empty() && base.back() != '/') base += '/';
    base += name;

    // Load the tileset .njn (sheet + inline ATTR).
    std::vector<TileAttr> attrs;
    if (!b->loadNjnAsset(base + ".njn", slot, attrs)) {
        luaL_error(L, "engine.tilemap.load: failed to load tileset '%s.njn'", name);
        return 0;
    }
    // Give the pinned slot well-defined animation state (mirrors sprite.load) so
    // it never carries garbage from a prior occupant while marked active.
    {
        auto& s = b->spritePool[slot];
        s.fps = 8.0f; s.accumSec = 0.0f; s.frame = 0;
        s.mode = AnimMode::Loop; s.forward = true; s.done = false;
        s.active = true;  // pin the arena; the sheet points into it
    }

    // On any failure past this point, release the pinned slot AND reclaim the
    // tileset pixels we just bump-allocated (they still sit at the arena tip),
    // so a pcall-guarded retry does not permanently leak the 64KB asset arena.
    auto releaseSlot = [&]() {
        auto& asset = b->loadedAssets_[slot];
        if (asset.pixelDataSize > 0 &&
            asset.pixelData + asset.pixelDataSize == b->assetBuffer_ + b->assetBufferUsed_) {
            b->assetBufferUsed_ -= asset.pixelDataSize;
        }
        asset = SpriteAsset{};
        b->loadedClips_[slot].clear();
        b->spritePool[slot].active = false;
    };

    // Load and validate the .njm map.
    FILE* fp = fopen((base + ".njm").c_str(), "rb");
    if (!fp) {
        releaseSlot();
        luaL_error(L, "engine.tilemap.load: failed to read '%s.njm'", name);
        return 0;
    }
    fseek(fp, 0, SEEK_END);
    long njmSize = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (njmSize <= 0) {
        fclose(fp);
        releaseSlot();
        luaL_error(L, "engine.tilemap.load: empty '%s.njm'", name);
        return 0;
    }
    std::string njm(static_cast<size_t>(njmSize), '\0');
    size_t got = fread(&njm[0], 1, static_cast<size_t>(njmSize), fp);
    fclose(fp);
    const uint8_t* njmData = reinterpret_cast<const uint8_t*>(njm.data());
    NjmHeader mh;
    if (got != static_cast<size_t>(njmSize) ||
        !parseNjmHeader(njmData, njm.size(), mh)) {
        releaseSlot();
        luaL_error(L, "engine.tilemap.load: invalid '%s.njm'", name);
        return 0;
    }

    // Unpack cells (clamped to the component's fixed grid).
    uint8_t mw = mh.mapW > C_Tilemap::MAX_MAP_W ? C_Tilemap::MAX_MAP_W : mh.mapW;
    uint8_t mhgt = mh.mapH > C_Tilemap::MAX_MAP_H ? C_Tilemap::MAX_MAP_H : mh.mapH;
    static uint16_t cells[C_Tilemap::MAX_MAP_W * C_Tilemap::MAX_MAP_H];
    for (uint8_t ty = 0; ty < mhgt; ++ty) {
        for (uint8_t tx = 0; tx < mw; ++tx) {
            cells[ty * mw + tx] = njmCellAt(njmData, mh, tx, ty);
        }
    }

    tm->setSheet(b->spritePool[slot].sheet);
    tm->setTiles(cells, mw, mhgt);
    if (!attrs.empty()) {
        tm->setAttrs(attrs.data(), static_cast<uint16_t>(attrs.size()));
    }
    return 1;
}

// The map's methods, shared by the scene-free handle (always registered) and a
// scene C_Tilemap's proxy (with the proxies): each metatable's __index is a
// table built from this module.
static constexpr LuaApiEntry kTilemapMethods[] = {
    luaFunction("setTile", lua_tilemap_setTile, "(tx:int, ty:int, cell:int) -> nil",
                "Set one cell; cells outside the map are ignored.",
                "tx: column, from 0\n"
                "ty: row, from 0\n"
                "cell: a tile id 0..511, or a packed cell (id in the low 9 bits, then palette "
                "bank, hflip, vflip and band); tile 0 is empty"),
    luaFunction("getTile", lua_tilemap_getTile, "(tx:int, ty:int) -> cell:int",
                "The packed cell at a grid position (tile id in the low 9 bits); 0 outside the map.",
                "tx: column, from 0\n"
                "ty: row, from 0"),
    luaFunction("setTiles", lua_tilemap_setTiles, "(tiles:table, w:int, h:int) -> nil",
                "Replace the whole map from a flat, row-major list of tile ids.",
                "tiles: w * h tile ids 0..255 (band, flips and palette bank 0)\n"
                "w: map width in tiles, up to 64\n"
                "h: map height in tiles, up to 64"),
    luaFunction("setSheet", lua_tilemap_setSheet, "(handle:int) -> nil",
                "Use a loaded sprite sheet as the tileset; an invalid handle raises.",
                "handle: a sprite handle; its frame size is the tile size"),
    luaFunction("setScroll", lua_tilemap_setScroll, "(sx:int, sy:int) -> nil",
                "Set the map's scroll offset in pixels.",
                "sx: horizontal scroll\n"
                "sy: vertical scroll"),
    luaFunction("getScroll", lua_tilemap_getScroll, "() -> sx:int, sy:int",
                "The map's scroll offset in pixels."),
    luaFunction("pixelToTile", lua_tilemap_pixelToTile, "(px:int, py:int) -> tx:int, ty:int",
                "The grid position under a screen pixel, scroll applied; may lie outside the map.",
                "px: screen column\n"
                "py: screen row"),
    luaFunction("tileToPixel", lua_tilemap_tileToPixel, "(tx:int, ty:int) -> px:int, py:int",
                "The screen pixel of a cell's top-left corner, scroll applied.",
                "tx: column\n"
                "ty: row"),
    luaFunction("tileAtPixel", lua_tilemap_tileAtPixel, "(px:int, py:int) -> cell:int",
                "The packed cell under a screen pixel, scroll applied; 0 outside the map.",
                "px: screen column\n"
                "py: screen row"),
    luaFunction("getMapSize", lua_tilemap_getMapSize, "() -> w:int, h:int",
                "The map's size in tiles."),
    luaFunction("setAttrs", lua_tilemap_setAttrs, "(attrs:table) -> nil",
                "Replace the per-tile attributes, keyed by tile id: {flags, kind, flags, kind, ...}.",
                "attrs: a flag byte (1 solid, 2 one-way, bits 2-3 direction) and a kind 0..255 "
                "for tile 0, 1, ...; an empty table makes every tile passable"),
    luaFunction("setPalbank", lua_tilemap_setPalbank, "(index:int, lut:table) -> nil",
                "Set the colour remap a cell's palette bank applies when drawn.",
                "index: palette bank 0..15\n"
                "lut: 16 palette indices, the colour for index 0, 1, ...")
        .note("The bank bits are stored but not yet drawn."),
    luaFunction("attrAt", lua_tilemap_attrAt, "(tx:int, ty:int) -> flags:int, kind:int",
                "A cell's attribute, its direction turned by the cell's flips; 0, 0 when none.",
                "tx: column\n"
                "ty: row"),
    luaFunction("attrAtPixel", lua_tilemap_attrAtPixel, "(px:int, py:int) -> flags:int, kind:int",
                "The attribute under a map pixel (scroll not applied); 0, 0 when none.",
                "px: map column in pixels\n"
                "py: map row in pixels"),
    luaFunction("sweepAabb", lua_tilemap_sweepAabb,
                "(x:number, y:number, w:number, h:number, vx:number, vy:number, dt:number) -> "
                "x:number, y:number, t:number, nx:number, ny:number, hit:boolean",
                "Move a box by its velocity for dt, sliding along solid and one-way tiles.",
                "x: left edge in map pixels\n"
                "y: top edge in map pixels\n"
                "w: width\n"
                "h: height\n"
                "vx: velocity x in pixels per second\n"
                "vy: velocity y in pixels per second\n"
                "dt: seconds")
        .note("Returns the new top-left, the fraction of the move made, the contact normal "
              "(0, 0 when free) and whether it touched. X resolves before Y."),
    luaFunction("sweepCircle", lua_tilemap_sweepCircle,
                "(cx:number, cy:number, r:number, vx:number, vy:number, dt:number) -> "
                "x:number, y:number, t:number, nx:number, ny:number, hit:boolean",
                "Move a circle like sweepAabb, through its bounding box; returns the new centre.",
                "cx: centre column in map pixels\n"
                "cy: centre row in map pixels\n"
                "r: radius\n"
                "vx: velocity x in pixels per second\n"
                "vy: velocity y in pixels per second\n"
                "dt: seconds"),
    luaFunction("forEachCellIn", lua_tilemap_forEachCellIn,
                "(x:number, y:number, w:number, h:number, fn:function) -> nil",
                "Call fn(tx, ty, cell) for every map cell a box overlaps, row by row.",
                "x: left edge in map pixels\n"
                "y: top edge in map pixels\n"
                "w: width\n"
                "h: height\n"
                "fn: the callback; an error in it propagates"),
    luaFunction("buildSolidRects", lua_tilemap_buildSolidRects, "() -> rects:table",
                "The solid tiles merged into rectangles: a flat {x, y, w, h, x, y, w, h, ...} in "
                "map pixels."),
};
static constexpr LuaApiModule kTilemapMethodsModule = luaApiModule(
    LuaApiScope::Methods, "Tilemap",
    "Map methods, called as map:name(...); a scene C_Tilemap has the same.", kTilemapMethods);

// The scene-free map handle's methods (engine.tilemap.load); not switchable.
void LuaBindings::registerTilemapMethods(lua_State* L) {
    static constexpr LuaApiEntry kMeta[] = {
        luaTable("__index", kTilemapMethodsModule, "The map's methods."),
    };
    static constexpr LuaApiModule kMetaModule =
        luaApiModule(LuaApiScope::Metatable, "Tilemap", "Map method lookup.", kMeta);
    lua::registerTilemapMetatable(L);
    luaL_getmetatable(L, lua::tilemapMt());
    luaApiSetFields(L, -1, kMetaModule);
    lua_pop(L, 1);
}

//==============================================================================
// C_Camera_Proxy Metatable Implementation (Phase 44: camera Lua API)
//==============================================================================

// cam:setPosition(x, y)
static int lua_ccamera_proxy_setPosition(lua_State* L) {
    auto* proxy = static_cast<enjin2::ComponentProxy*>(
        luaL_checkudata(L, 1, CCAMERA_PROXY_METATABLE));
    if (!proxy || !proxy->valid || !proxy->component) return luaL_error(L, "camera has been destroyed");
    auto* cam = static_cast<enjin2::C_Camera*>(proxy->component);
    float x = static_cast<float>(luaL_checknumber(L, 2));
    float y = static_cast<float>(luaL_checknumber(L, 3));
    cam->setPosition(x, y);
    return 0;
}

// cam:getPosition() -> x, y
static int lua_ccamera_proxy_getPosition(lua_State* L) {
    auto* proxy = static_cast<enjin2::ComponentProxy*>(
        luaL_checkudata(L, 1, CCAMERA_PROXY_METATABLE));
    if (!proxy || !proxy->valid || !proxy->component) return luaL_error(L, "camera has been destroyed");
    auto* cam = static_cast<enjin2::C_Camera*>(proxy->component);
    enjin2::Vec2 pos = cam->getPosition();
    lua_pushnumber(L, static_cast<lua_Number>(pos.x));
    lua_pushnumber(L, static_cast<lua_Number>(pos.y));
    return 2;
}

// cam:lookAt(x, y [, speed]) — speed defaults to 1.0 (instant snap)
static int lua_ccamera_proxy_lookAt(lua_State* L) {
    auto* proxy = static_cast<enjin2::ComponentProxy*>(
        luaL_checkudata(L, 1, CCAMERA_PROXY_METATABLE));
    if (!proxy || !proxy->valid || !proxy->component) return luaL_error(L, "camera has been destroyed");
    auto* cam = static_cast<enjin2::C_Camera*>(proxy->component);
    float x = static_cast<float>(luaL_checknumber(L, 2));
    float y = static_cast<float>(luaL_checknumber(L, 3));
    float speed = lua_gettop(L) >= 4 ? static_cast<float>(luaL_checknumber(L, 4)) : 1.0f;
    cam->lookAt(x, y, speed);
    return 0;
}

// cam:shake(intensity, duration)
static int lua_ccamera_proxy_shake(lua_State* L) {
    auto* proxy = static_cast<enjin2::ComponentProxy*>(
        luaL_checkudata(L, 1, CCAMERA_PROXY_METATABLE));
    if (!proxy || !proxy->valid || !proxy->component) return luaL_error(L, "camera has been destroyed");
    auto* cam = static_cast<enjin2::C_Camera*>(proxy->component);
    float intensity = static_cast<float>(luaL_checknumber(L, 2));
    float duration = static_cast<float>(luaL_checknumber(L, 3));
    cam->shake(intensity, duration);
    return 0;
}

// cam:setBounds(minX, minY, maxX, maxY)
static int lua_ccamera_proxy_setBounds(lua_State* L) {
    auto* proxy = static_cast<enjin2::ComponentProxy*>(
        luaL_checkudata(L, 1, CCAMERA_PROXY_METATABLE));
    if (!proxy || !proxy->valid || !proxy->component) return luaL_error(L, "camera has been destroyed");
    auto* cam = static_cast<enjin2::C_Camera*>(proxy->component);
    float minX = static_cast<float>(luaL_checknumber(L, 2));
    float minY = static_cast<float>(luaL_checknumber(L, 3));
    float maxX = static_cast<float>(luaL_checknumber(L, 4));
    float maxY = static_cast<float>(luaL_checknumber(L, 5));
    cam->setBounds(minX, minY, maxX, maxY);
    return 0;
}

// cam:clearBounds()
static int lua_ccamera_proxy_clearBounds(lua_State* L) {
    auto* proxy = static_cast<enjin2::ComponentProxy*>(
        luaL_checkudata(L, 1, CCAMERA_PROXY_METATABLE));
    if (!proxy || !proxy->valid || !proxy->component) return luaL_error(L, "camera has been destroyed");
    auto* cam = static_cast<enjin2::C_Camera*>(proxy->component);
    cam->clearBounds();
    return 0;
}

//==============================================================================
// C_Sprite_Proxy Metatable Implementation (ADR-0003 §5, Tomodachi #81)
//
// The retained sprite: clip playback (per-frame durations + loop modes), polled
// events, hflip/vflip through the flip-aware SpriteSheet::draw, and scrub-by-
// angle. Attached with obj:add("C_Sprite") like every other component (§2).
//==============================================================================

#define CSPRITE_PROXY_CHECK(L, varname)                                           \
    auto* proxy = static_cast<enjin2::ComponentProxy*>(                           \
        luaL_checkudata(L, 1, CSPRITE_PROXY_METATABLE));                          \
    if (!proxy || !proxy->valid || !proxy->component) {                           \
        luaL_error(L, "component has been destroyed");                            \
        return 0;                                                                 \
    }                                                                             \
    auto* (varname) = static_cast<enjin2::C_Sprite*>(proxy->component)

// kLoopModeNames (njn2.hpp) is indexed by NjnLoopMode, and AnimMode keeps the
// same order; it is published as the LoopMode enum of the C_Sprite descriptors.

// Parse a loop-mode argument: a kLoopModeNames name (any case) or the integer
// 0/1/2. Defaults to Loop for anything unrecognised.
static enjin2::NjnLoopMode parseLoopMode(lua_State* L, int idx) {
    if (lua_type(L, idx) == LUA_TNUMBER) {
        switch (static_cast<int>(lua_tointeger(L, idx))) {
            case 0: return enjin2::NjnLoopMode::Once;
            case 2: return enjin2::NjnLoopMode::PingPong;
            default: return enjin2::NjnLoopMode::Loop;
        }
    }
    const char* s = lua_tostring(L, idx);
    if (s) {
        // Case-insensitive so "Once"/"ONCE" match, not silently fall to Loop.
        char buf[16] = {0};
        size_t i = 0;
        for (const char* p = s; *p && i < sizeof(buf) - 1; ++p, ++i) {
            buf[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(*p)));
        }
        const int mode = nameIndex(buf, kLoopModeNames);
        if (mode >= 0) return static_cast<enjin2::NjnLoopMode>(mode);
    }
    return enjin2::NjnLoopMode::Loop;
}

// sprite:setSheet(handle) — bind a SpriteSheet from the LuaBindings sprite pool.
static int lua_sprite_setSheet(lua_State* L) {
    CSPRITE_PROXY_CHECK(L, sp);
    int handle = static_cast<int>(luaL_checkinteger(L, 2));
    LuaBindings* b = LuaBindings::getBindings(L);
    if (!b) { luaL_error(L, "C_Sprite.setSheet: LuaBindings not available"); return 0; }
    const enjin2::SpriteSheet* sheet = b->getSpriteSheet(handle);
    if (!sheet) { luaL_error(L, "C_Sprite.setSheet: invalid sprite handle %d", handle); return 0; }
    sp->setSheet(*sheet);
    return 0;
}

// sprite:setClips(handle)  — pull the CLIP table decoded from a v2 .njn (#91), or
// sprite:setClips({ {name=, loop=, frames={ {frame=,dur=,event=}, ... }}, ... })
static int lua_sprite_setClips(lua_State* L) {
    CSPRITE_PROXY_CHECK(L, sp);

    // Handle form: reuse the clips engine.sprite.load() decoded from the .njn.
    if (lua_isnumber(L, 2)) {
        int handle = static_cast<int>(lua_tointeger(L, 2));
        LuaBindings* b = LuaBindings::getBindings(L);
        if (!b) { luaL_error(L, "C_Sprite.setClips: LuaBindings not available"); return 0; }
        const std::vector<enjin2::NjnClip>* loaded = b->getLoadedClips(handle);
        if (!loaded) { luaL_error(L, "C_Sprite.setClips: invalid sprite handle %d", handle); return 0; }
        sp->setClips(*loaded);
        return 0;
    }

    luaL_checktype(L, 2, LUA_TTABLE);

    std::vector<enjin2::NjnClip> clips;
    const int nClips = static_cast<int>(lua_rawlen(L, 2));
    for (int ci = 1; ci <= nClips; ++ci) {
        lua_rawgeti(L, 2, ci);            // clip table at top
        if (!lua_istable(L, -1)) { lua_pop(L, 1); continue; }
        int clipIdx = lua_gettop(L);

        enjin2::NjnClip clip{};
        std::memset(clip.name, 0, sizeof(clip.name));
        lua_getfield(L, clipIdx, "name");
        const char* name = lua_tostring(L, -1);
        if (name) std::strncpy(clip.name, name, sizeof(clip.name) - 1);
        lua_pop(L, 1);

        lua_getfield(L, clipIdx, "loop");
        clip.loopMode = parseLoopMode(L, lua_gettop(L));
        lua_pop(L, 1);

        lua_getfield(L, clipIdx, "frames");
        if (lua_istable(L, -1)) {
            int framesIdx = lua_gettop(L);
            const int nFrames = static_cast<int>(lua_rawlen(L, framesIdx));
            for (int fi = 1; fi <= nFrames; ++fi) {
                lua_rawgeti(L, framesIdx, fi);   // frame table at top
                if (lua_istable(L, -1)) {
                    int frIdx = lua_gettop(L);
                    enjin2::NjnFrameEntry fe{};
                    lua_getfield(L, frIdx, "frame");
                    fe.frameIndex = static_cast<uint16_t>(luaL_optinteger(L, -1, 0) & 0xFFFF);
                    lua_pop(L, 1);
                    lua_getfield(L, frIdx, "dur");
                    fe.durationMs = static_cast<uint16_t>(luaL_optinteger(L, -1, 100) & 0xFFFF);
                    lua_pop(L, 1);
                    lua_getfield(L, frIdx, "event");
                    fe.eventId = static_cast<uint8_t>(luaL_optinteger(L, -1, 0) & 0xFF);
                    lua_pop(L, 1);
                    clip.frames.push_back(fe);
                }
                lua_pop(L, 1);  // frame table
            }
        }
        lua_pop(L, 1);  // frames

        clips.push_back(std::move(clip));
        lua_pop(L, 1);  // clip table
    }
    sp->setClips(clips);
    return 0;
}

// sprite:play(name) -> bool
static int lua_sprite_play(lua_State* L) {
    CSPRITE_PROXY_CHECK(L, sp);
    const char* name = luaL_checkstring(L, 2);
    lua_pushboolean(L, sp->play(name) ? 1 : 0);
    return 1;
}

// sprite:setFlip(h, v)
static int lua_sprite_setFlip(lua_State* L) {
    CSPRITE_PROXY_CHECK(L, sp);
    sp->setFlip(lua_toboolean(L, 2) != 0, lua_toboolean(L, 3) != 0);
    return 0;
}

// sprite:setFrameForAngle(angle, minAngle, maxAngle)
static int lua_sprite_setFrameForAngle(lua_State* L) {
    CSPRITE_PROXY_CHECK(L, sp);
    sp->setFrameForAngle(static_cast<float>(luaL_checknumber(L, 2)),
                         static_cast<float>(luaL_checknumber(L, 3)),
                         static_cast<float>(luaL_checknumber(L, 4)));
    return 0;
}

// sprite:setFrame(index)
static int lua_sprite_setFrame(lua_State* L) {
    CSPRITE_PROXY_CHECK(L, sp);
    sp->setFrame(static_cast<uint16_t>(luaL_checkinteger(L, 2) & 0xFFFF));
    return 0;
}

// sprite:setFPS(fps) — legacy whole-sheet playback (no-clip path)
static int lua_sprite_setFPS(lua_State* L) {
    CSPRITE_PROXY_CHECK(L, sp);
    sp->setFPS(static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}

// sprite:setMode(mode) — legacy loop mode; accepts "once"/"loop"/"pingpong" or 0/1/2
static int lua_sprite_setMode(lua_State* L) {
    CSPRITE_PROXY_CHECK(L, sp);
    // AnimMode and NjnLoopMode share the Once/Loop/PingPong ordering.
    sp->setMode(static_cast<enjin2::AnimMode>(parseLoopMode(L, 2)));
    return 0;
}

#undef CSPRITE_PROXY_CHECK

// __index for C_Sprite_Proxy: methods first, then the readable properties.
static int lua_csprite_proxy_index_impl(lua_State* L) {
    auto* proxy = static_cast<enjin2::ComponentProxy*>(
        luaL_checkudata(L, 1, CSPRITE_PROXY_METATABLE));
    if (!proxy || !proxy->valid || !proxy->component) {
        luaL_error(L, "component has been destroyed");
        return 0;
    }
    const char* key = lua_tostring(L, 2);
    if (!key) { lua_pushnil(L); return 1; }

    if (pushProxyMethod(L, kSpriteMethods, 2)) return 1;

    // Readable properties
    auto* sp = static_cast<enjin2::C_Sprite*>(proxy->component);
    if (strcmp(key, "frame") == 0)         { lua_pushinteger(L, sp->getFrame()); return 1; }
    if (strcmp(key, "hflip") == 0)         { lua_pushboolean(L, sp->getHFlip() ? 1 : 0); return 1; }
    if (strcmp(key, "vflip") == 0)         { lua_pushboolean(L, sp->getVFlip() ? 1 : 0); return 1; }
    if (strcmp(key, "visible") == 0)       { lua_pushboolean(L, sp->isVisible() ? 1 : 0); return 1; }
    if (strcmp(key, "done") == 0)          { lua_pushboolean(L, sp->isDone() ? 1 : 0); return 1; }
    if (strcmp(key, "justAdvanced") == 0)  { lua_pushboolean(L, sp->justAdvanced() ? 1 : 0); return 1; }
    if (strcmp(key, "justCompleted") == 0) { lua_pushboolean(L, sp->justCompleted() ? 1 : 0); return 1; }
    if (strcmp(key, "frameEvent") == 0)    { lua_pushinteger(L, sp->frameEvent()); return 1; }
    if (strcmp(key, "clip") == 0) {
        const char* c = sp->currentClip();
        if (c && c[0]) lua_pushstring(L, c); else lua_pushnil(L);
        return 1;
    }

    lua_pushnil(L);
    return 1;
}

// __newindex for C_Sprite_Proxy: writable hflip/vflip/visible (ADR-0003 §2).
static int lua_csprite_proxy_newindex_impl(lua_State* L) {
    auto* proxy = static_cast<enjin2::ComponentProxy*>(
        luaL_checkudata(L, 1, CSPRITE_PROXY_METATABLE));
    if (!proxy || !proxy->valid || !proxy->component) {
        luaL_error(L, "component has been destroyed");
        return 0;
    }
    const char* key = lua_tostring(L, 2);
    if (!key) return 0;

    auto* sp = static_cast<enjin2::C_Sprite*>(proxy->component);
    if (strcmp(key, "hflip") == 0)        sp->setHFlip(lua_toboolean(L, 3) != 0);
    else if (strcmp(key, "vflip") == 0)   sp->setVFlip(lua_toboolean(L, 3) != 0);
    else if (strcmp(key, "visible") == 0) sp->SetVisibility(lua_toboolean(L, 3) != 0);
    return 0;
}

//==============================================================================
// C_Body_Proxy Metatable Implementation (ADR-0003 §4, Tomodachi #80)
//
// The single dynamic circle body. Writable state (x/y/vx/vy/radius/restitution/
// drag), step configuration (gravity/substeps/swept), a step(dt) that runs N
// substeps against the active ColliderSet, and polled contacts (no callbacks).
//==============================================================================

#define CBODY_PROXY_CHECK(L, varname)                                             \
    auto* proxy = static_cast<enjin2::ComponentProxy*>(                           \
        luaL_checkudata(L, 1, CBODY_PROXY_METATABLE));                            \
    if (!proxy || !proxy->valid || !proxy->component) {                           \
        luaL_error(L, "component has been destroyed");                            \
        return 0;                                                                 \
    }                                                                             \
    auto* (varname) = static_cast<enjin2::C_Body*>(proxy->component)

// body:step(dt)
static int lua_body_step(lua_State* L) {
    CBODY_PROXY_CHECK(L, body);
    body->step(static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}

// body:setGravity(gx, gy)
static int lua_body_setGravity(lua_State* L) {
    CBODY_PROXY_CHECK(L, body);
    body->setGravity(static_cast<float>(luaL_checknumber(L, 2)),
                     static_cast<float>(luaL_checknumber(L, 3)));
    return 0;
}

// body:setSubsteps(n)
static int lua_body_setSubsteps(lua_State* L) {
    CBODY_PROXY_CHECK(L, body);
    body->setSubsteps(static_cast<int>(luaL_checkinteger(L, 2)));
    return 0;
}

// body:setColliders(collidersProxy) — reference the active scene collider set.
static int lua_body_setColliders(lua_State* L) {
    CBODY_PROXY_CHECK(L, body);
    auto** slot = static_cast<enjin2::ColliderSet**>(
        luaL_checkudata(L, 2, COLLIDERSET_PROXY_METATABLE));
    body->setColliders(*slot);
    return 0;
}

// body:numContacts() -> int
static int lua_body_numContacts(lua_State* L) {
    CBODY_PROXY_CHECK(L, body);
    lua_pushinteger(L, static_cast<lua_Integer>(body->numContacts()));
    return 1;
}

// body:contact(i) -> {kind, x, y, nx, ny, relSpeed} | nil
static int lua_body_contact(lua_State* L) {
    CBODY_PROXY_CHECK(L, body);
    const size_t i = static_cast<size_t>(luaL_checkinteger(L, 2));
    const enjin2::Contact* c = body->contact(i - 1);  // 1-based from Lua
    if (!c) { lua_pushnil(L); return 1; }
    lua_createtable(L, 0, 5);
    lua_pushinteger(L, c->kind);          lua_setfield(L, -2, "kind");
    lua_pushnumber(L, c->px);             lua_setfield(L, -2, "x");
    lua_pushnumber(L, c->py);             lua_setfield(L, -2, "y");
    lua_pushnumber(L, c->nx);             lua_setfield(L, -2, "nx");
    lua_pushnumber(L, c->ny);             lua_setfield(L, -2, "ny");
    lua_pushnumber(L, c->relSpeed);       lua_setfield(L, -2, "relSpeed");
    return 1;
}

static int lua_cbody_proxy_index_impl(lua_State* L) {
    CBODY_PROXY_CHECK(L, body);
    const char* key = lua_tostring(L, 2);
    if (!key) { lua_pushnil(L); return 1; }

    if (pushProxyMethod(L, kBodyMethods, 2)) return 1;

    if (strcmp(key, "x") == 0)          { lua_pushnumber(L, body->getX()); return 1; }
    if (strcmp(key, "y") == 0)           { lua_pushnumber(L, body->getY()); return 1; }
    if (strcmp(key, "vx") == 0)          { lua_pushnumber(L, body->getVX()); return 1; }
    if (strcmp(key, "vy") == 0)          { lua_pushnumber(L, body->getVY()); return 1; }
    if (strcmp(key, "radius") == 0)      { lua_pushnumber(L, body->getRadius()); return 1; }
    if (strcmp(key, "restitution") == 0) { lua_pushnumber(L, body->getRestitution()); return 1; }
    if (strcmp(key, "drag") == 0)        { lua_pushnumber(L, body->getDrag()); return 1; }

    lua_pushnil(L);
    return 1;
}

// __newindex for C_Body_Proxy: writable state so obj:add("C_Body", {radius=4,
// restitution=.8}) configures through the C++ setters (ADR-0003 §2).
static int lua_cbody_proxy_newindex_impl(lua_State* L) {
    CBODY_PROXY_CHECK(L, body);
    const char* key = lua_tostring(L, 2);
    if (!key) return 0;

    const float v = static_cast<float>(luaL_checknumber(L, 3));
    if (strcmp(key, "x") == 0)           body->setPosition(v, body->getY());
    else if (strcmp(key, "y") == 0)      body->setPosition(body->getX(), v);
    else if (strcmp(key, "vx") == 0)     body->setVelocity(v, body->getVY());
    else if (strcmp(key, "vy") == 0)     body->setVelocity(body->getVX(), v);
    else if (strcmp(key, "radius") == 0) body->setRadius(v);
    else if (strcmp(key, "restitution") == 0) body->setRestitution(v);
    else if (strcmp(key, "drag") == 0)   body->setDrag(v);
    return 0;
}

#undef CBODY_PROXY_CHECK

//==============================================================================
// ColliderSet_Proxy Metatable Implementation (ADR-0003 §4, Tomodachi #80)
//
// The scene-level collider resource (engine.scene.colliders()). Non-owning: the
// userdata wraps the Scene's ColliderSet reference; the scene owns the storage.
// Every method is a thin call onto the same ColliderSet member a C++ applet
// uses, with the same argument order (parity by construction, ADR-0003 §2).
//==============================================================================

#define COLLIDERSET_PROXY_CHECK(L, varname)                                       \
    auto* (varname) = *static_cast<enjin2::ColliderSet**>(                        \
        luaL_checkudata(L, 1, COLLIDERSET_PROXY_METATABLE))

// Flipper index argument: 1-based from Lua, 0-based into the set.
static size_t luaFlipperIndex(lua_State* L, int arg, const enjin2::ColliderSet& set,
                              const char* fn) {
    const lua_Integer i = luaL_checkinteger(L, arg);
    if (i < 1 || static_cast<size_t>(i) > set.flippers.size())
        luaL_error(L, "%s: bad flipper index %d", fn, static_cast<int>(i));
    return static_cast<size_t>(i - 1);
}

// colliders:addSeg(ax, ay, bx, by, restitution, kind)
static int lua_colliders_addSeg(lua_State* L) {
    COLLIDERSET_PROXY_CHECK(L, set);
    set->addSeg(static_cast<float>(luaL_checknumber(L, 2)),
                static_cast<float>(luaL_checknumber(L, 3)),
                static_cast<float>(luaL_checknumber(L, 4)),
                static_cast<float>(luaL_checknumber(L, 5)),
                static_cast<float>(luaL_optnumber(L, 6, 0.5f)),
                static_cast<uint8_t>(luaL_optinteger(L, 7, enjin2::ColliderKinds::Wall)));
    return 0;
}

// colliders:addCircle(cx, cy, r, restitution, kind)
static int lua_colliders_addCircle(lua_State* L) {
    COLLIDERSET_PROXY_CHECK(L, set);
    set->addCircle(static_cast<float>(luaL_checknumber(L, 2)),
                   static_cast<float>(luaL_checknumber(L, 3)),
                   static_cast<float>(luaL_checknumber(L, 4)),
                   static_cast<float>(luaL_optnumber(L, 5, 0.5f)),
                   static_cast<uint8_t>(luaL_optinteger(L, 6, enjin2::ColliderKinds::Wall)));
    return 0;
}

// colliders:addAabb(minx, miny, maxx, maxy, restitution, kind) — min/max
// corners, the same argument order as ColliderSet::addAabb.
static int lua_colliders_addAabb(lua_State* L) {
    COLLIDERSET_PROXY_CHECK(L, set);
    set->addAabb(static_cast<float>(luaL_checknumber(L, 2)),
                 static_cast<float>(luaL_checknumber(L, 3)),
                 static_cast<float>(luaL_checknumber(L, 4)),
                 static_cast<float>(luaL_checknumber(L, 5)),
                 static_cast<float>(luaL_optnumber(L, 6, 0.1f)),
                 static_cast<uint8_t>(luaL_optinteger(L, 7, enjin2::ColliderKinds::Wall)));
    return 0;
}

// colliders:addSolidRects(map, restitution, kind) -> int
// Derive AABBs from a tilemap's SOLID attrs (ADR-0003 §3 buildSolidRects) and
// append them to the set — the "tile-derived AABBs" half of §4's two collider
// sources. Returns the number of rects added.
static int lua_colliders_addSolidRects(lua_State* L) {
    COLLIDERSET_PROXY_CHECK(L, set);
    auto* tm = lua::checkTilemap(L, 2);
    const size_t n = set->addSolidRects(
        tm->buildSolidRects(),
        static_cast<float>(luaL_optnumber(L, 3, 0.1f)),
        static_cast<uint8_t>(luaL_optinteger(L, 4, enjin2::ColliderKinds::Wall)));
    lua_pushinteger(L, static_cast<lua_Integer>(n));
    return 1;
}

// colliders:addFlipper(pivotX, pivotY, length, restAngle, activeAngle, restitution) -> index (1-based)
static int lua_colliders_addFlipper(lua_State* L) {
    COLLIDERSET_PROXY_CHECK(L, set);
    const size_t i = set->addFlipper(static_cast<float>(luaL_checknumber(L, 2)),
                                     static_cast<float>(luaL_checknumber(L, 3)),
                                     static_cast<float>(luaL_checknumber(L, 4)),
                                     static_cast<float>(luaL_checknumber(L, 5)),
                                     static_cast<float>(luaL_checknumber(L, 6)),
                                     static_cast<float>(luaL_optnumber(L, 7, 0.2f)));
    lua_pushinteger(L, static_cast<lua_Integer>(i + 1));
    return 1;
}

// colliders:setFlipperActive(i, bool) — spring toward activeAngle / restAngle (1-based index).
static int lua_colliders_setFlipperActive(lua_State* L) {
    COLLIDERSET_PROXY_CHECK(L, set);
    const size_t i = luaFlipperIndex(L, 2, *set, "setFlipperActive");
    set->setFlipperActive(i, lua_toboolean(L, 3) != 0);
    return 0;
}

// colliders:setFlipperTarget(i, angle) — raw spring target (1-based index).
static int lua_colliders_setFlipperTarget(lua_State* L) {
    COLLIDERSET_PROXY_CHECK(L, set);
    const size_t i = luaFlipperIndex(L, 2, *set, "setFlipperTarget");
    set->setFlipperTarget(i, static_cast<float>(luaL_checknumber(L, 3)));
    return 0;
}

// colliders:setFlipperAngle(i, angle) — snap a flipper's angle (and spring state).
static int lua_colliders_setFlipperAngle(lua_State* L) {
    COLLIDERSET_PROXY_CHECK(L, set);
    const size_t i = luaFlipperIndex(L, 2, *set, "setFlipperAngle");
    set->setFlipperAngle(i, static_cast<float>(luaL_checknumber(L, 3)));
    return 0;
}

// colliders:numFlippers() -> int
static int lua_colliders_numFlippers(lua_State* L) {
    COLLIDERSET_PROXY_CHECK(L, set);
    lua_pushinteger(L, static_cast<lua_Integer>(set->flippers.size()));
    return 1;
}

// colliders:count() -> total collider count
static int lua_colliders_count(lua_State* L) {
    COLLIDERSET_PROXY_CHECK(L, set);
    lua_pushinteger(L, static_cast<lua_Integer>(set->count()));
    return 1;
}

// colliders:clear()
static int lua_colliders_clear(lua_State* L) {
    COLLIDERSET_PROXY_CHECK(L, set);
    set->clear();
    return 0;
}

#undef COLLIDERSET_PROXY_CHECK

// Allocate a ColliderSet userdata over a scene's collider set. Non-owning.
int pushColliderSetProxy(lua_State* L, enjin2::ColliderSet* set) {
    if (!set) { lua_pushnil(L); return 1; }
    auto** slot = static_cast<enjin2::ColliderSet**>(
        lua_newuserdata(L, sizeof(enjin2::ColliderSet*)));
    *slot = set;
    luaL_getmetatable(L, COLLIDERSET_PROXY_METATABLE);
    lua_setmetatable(L, -2);
    return 1;
}

//==============================================================================
// ObjectProxy Metatable Implementation (Phase 37: engine.scene.find() safety)
//==============================================================================

// proxy:get("TypeName") — fetch a component off a spawn/find'd object.
static int lua_objproxy_get_component_impl(lua_State* L) {
    enjin2::ObjectProxy* proxy = static_cast<enjin2::ObjectProxy*>(
        luaL_checkudata(L, 1, OBJECT_PROXY_METATABLE));
    if (!proxy || !proxy->valid || !proxy->object) {
        luaL_error(L, "object has been destroyed");
        return 0;
    }
    const char* typeName = luaL_checkstring(L, 2);
    return pushComponentGet(L, proxy->object, typeName);
}

// proxy:add("TypeName"[, params]) — attach (or fetch) a component and return its
// writable proxy. Same registry-generated path as ScriptProxy:add.
static int lua_objproxy_add_component_impl(lua_State* L) {
    enjin2::ObjectProxy* proxy = static_cast<enjin2::ObjectProxy*>(
        luaL_checkudata(L, 1, OBJECT_PROXY_METATABLE));
    if (!proxy || !proxy->valid || !proxy->object) {
        luaL_error(L, "object has been destroyed");
        return 0;
    }
    const char* typeName = luaL_checkstring(L, 2);
    return pushComponentAdd(L, proxy->object, typeName, 3);  // optional params table at arg 3
}

// proxy:destroy() — remove this object from the active scene. Mirrors
// engine.scene.destroy(proxy) so an ObjectProxy is a self-sufficient instance
// handle (ADR-0003 §2). Object::~Object() sets proxy->valid = false, so a later
// access on the proxy raises the usual "object has been destroyed" error.
static int lua_objproxy_destroy_impl(lua_State* L) {
    enjin2::ObjectProxy* proxy = static_cast<enjin2::ObjectProxy*>(
        luaL_checkudata(L, 1, OBJECT_PROXY_METATABLE));
    if (!proxy || !proxy->valid || !proxy->object) return 0;

    lua_getfield(L, LUA_REGISTRYINDEX, "enjin_active_scene");
    auto** scenePP = static_cast<enjin2::Scene**>(lua_touserdata(L, -1));
    lua_pop(L, 1);
    if (scenePP == nullptr || *scenePP == nullptr) return 0;

    (*scenePP)->removeObject(proxy->object);
    return 0;
}

// proxy:hasTag(tag) -> boolean
static int lua_objproxy_hasTag_impl(lua_State* L) {
    auto* p = static_cast<enjin2::ObjectProxy*>(luaL_checkudata(L, 1, OBJECT_PROXY_METATABLE));
    if (!p || !p->valid || !p->object) {
        luaL_error(L, "object has been destroyed");
        return 0;
    }
    const char* tag = luaL_checkstring(L, 2);
    lua_pushboolean(L, p->object->hasTag(tag) ? 1 : 0);
    return 1;
}

// __index metamethod for ObjectProxy.
// Reads proxy.name, proxy:hasTag(tag), proxy.position (table snapshot), proxy.enable.
// Locked decisions (Phase 37 CONTEXT.md):
//   - name: read-only string
//   - hasTag(tag): method returning boolean
//   - position: read/write table {x, y}
//   - enable: read/write — controls C_LuaScript enabled state
//
// Stack layout on entry: [1]=ObjectProxy userdata, [2]=key_string
static int lua_objproxy_index_impl(lua_State* L) {
    enjin2::ObjectProxy* proxy = static_cast<enjin2::ObjectProxy*>(
        luaL_checkudata(L, 1, OBJECT_PROXY_METATABLE));
    if (!proxy) { lua_pushnil(L); return 1; }
    if (!proxy->valid || !proxy->object) {
        luaL_error(L, "object has been destroyed");
        return 0;  // unreachable — luaL_error longjmps
    }

    const char* key = lua_tostring(L, 2);
    if (!key) { lua_pushnil(L); return 1; }

    enjin2::Object* obj = proxy->object;

    // Methods (get/add/destroy/hasTag) — the same attach verb spawn()'d and
    // find()'d objects get, so an ObjectProxy is a full instance handle
    // (ADR-0003 §2, closing the spawn-can't-get gap). Checked before the
    // property keys, mirroring ScriptProxy.
    if (pushProxyMethod(L, kObjectProxyMethods, 2)) return 1;

    if (strcmp(key, "name") == 0) {
        const char* n = obj->getName();
        if (n) lua_pushstring(L, n); else lua_pushnil(L);
        return 1;
    } else if (strcmp(key, "position") == 0) {
        // Return a table {x=..., y=...} snapshot of current C_Position
        enjin2::C_Position* pos = obj->getPosition();
        lua_newtable(L);
        lua_pushinteger(L, pos ? static_cast<lua_Integer>(pos->getPosition().x) : 0);
        lua_setfield(L, -2, "x");
        lua_pushinteger(L, pos ? static_cast<lua_Integer>(pos->getPosition().y) : 0);
        lua_setfield(L, -2, "y");
        return 1;
    } else if (strcmp(key, "enable") == 0) {
        // Read current enabled state of C_LuaScript component (per locked user decision)
        enjin2::C_LuaScript* script = obj->getComponent<enjin2::C_LuaScript>();
        if (script) {
            lua_pushboolean(L, script->isEnabled() ? 1 : 0);
        } else {
            lua_pushnil(L);  // No C_LuaScript on this object
        }
        return 1;
    }

    lua_pushnil(L);
    return 1;
}

// __newindex metamethod for ObjectProxy.
// Handles position write (proxy.position = {x=N, y=M}) and enable/disable.
// Locked decisions (Phase 37 CONTEXT.md):
//   - position write: proxy.position = {x=..., y=...} -> C_Position::setPosition()
//   - enable write:   proxy.enable = bool -> C_LuaScript::setEnabled()
//   - name write:     silently ignored (read-only)
//
// Stack layout on entry: [1]=ObjectProxy userdata, [2]=key_string, [3]=value
static int lua_objproxy_newindex_impl(lua_State* L) {
    enjin2::ObjectProxy* proxy = static_cast<enjin2::ObjectProxy*>(
        luaL_checkudata(L, 1, OBJECT_PROXY_METATABLE));
    if (!proxy) return 0;
    if (!proxy->valid || !proxy->object) {
        luaL_error(L, "object has been destroyed");
        return 0;  // unreachable — luaL_error longjmps
    }

    const char* key = lua_tostring(L, 2);
    if (!key) return 0;

    if (strcmp(key, "position") == 0) {
        // Value at stack index 3 must be a table with x and y fields
        if (!lua_istable(L, 3)) {
            luaL_error(L, "ObjectProxy.position: expected table {x=..., y=...}, got %s",
                       lua_typename(L, lua_type(L, 3)));
            return 0;
        }
        lua_getfield(L, 3, "x");
        lua_getfield(L, 3, "y");
        auto x = static_cast<int16_t>(luaL_optinteger(L, -2, 0));
        auto y = static_cast<int16_t>(luaL_optinteger(L, -1, 0));
        lua_pop(L, 2);
        enjin2::C_Position* pos = proxy->object->getPosition();
        if (pos) pos->setPosition(x, y);
        return 0;
    } else if (strcmp(key, "enable") == 0) {
        // Enable/disable the C_LuaScript component on this object (per locked user decision)
        // proxy.enable = true  -> script runs each update tick
        // proxy.enable = false -> script is skipped (component disabled)
        enjin2::C_LuaScript* script = proxy->object->getComponent<enjin2::C_LuaScript>();
        if (script) {
            script->setEnabled(lua_toboolean(L, 3) != 0);
        }
        return 0;
    }

    // All other property writes: silently ignore (name is read-only per design)
    return 0;
}

// __gc metamethod for ObjectProxy.
// Called by Lua GC when the proxy userdata is about to be freed.
// Clears Object::m_luaProxy so the C++ Object destructor does not write
// into freed Lua memory (heap-use-after-free, caught by ASAN).
static int lua_objproxy_gc_impl(lua_State* L) {
    enjin2::ObjectProxy* proxy = static_cast<enjin2::ObjectProxy*>(
        luaL_testudata(L, 1, OBJECT_PROXY_METATABLE));
    if (proxy && proxy->valid && proxy->object) {
        // Clear the back-pointer so Object::~Object() does not try to
        // write to this proxy's memory after it has been freed by the GC.
        proxy->object->setLuaProxy(nullptr);
    }
    return 0;
}

void LuaBindings::registerObjectProxyMetatable() {
    lua_State* L = engine->getState();
    if (!L) return;
    static constexpr LuaApiEntry kMethods[] = {
        luaFunction("get", lua_objproxy_get_component_impl, "(type:string) -> proxy:userdata?",
                    "The object's component of a type, as its proxy; nil when it has none.",
                    "type: a component name such as \"C_Position\"; an unknown name gives nil"),
        luaFunction("add", lua_objproxy_add_component_impl,
                    "(type:string, params:table?) -> proxy:userdata?",
                    "Attach a component, or take the one already there, and set fields from params.",
                    "type: a component name; an unknown name raises\n"
                    "params: field = value pairs written through the proxy; unknown fields are "
                    "ignored"),
        luaFunction("destroy", lua_objproxy_destroy_impl, "() -> nil",
                    "Remove the object from the active scene; its proxies go stale."),
        luaFunction("hasTag", lua_objproxy_hasTag_impl, "(tag:string) -> boolean",
                    "Whether the object has a tag.",
                    "tag: the tag"),
    };
    static constexpr LuaApiModule kMethodsModule = luaApiModule(
        LuaApiScope::Methods, "ObjectProxy", "A scene object's methods, called as obj:name(...).",
        kMethods);
    static constexpr LuaApiEntry kMeta[] = {
        luaFunction("__index", lua_objproxy_index_impl, "(obj:ObjectProxy, key:string) -> any",
                    "obj.name, obj.position (an {x, y} copy) and obj.enable (its script runs; nil "
                    "without a script) read the object; other keys look up a method.",
                    "obj: the object\n"
                    "key: a field or method name; unknown keys give nil")
            .note("Raises once the object is destroyed."),
        luaFunction("__newindex", lua_objproxy_newindex_impl,
                    "(obj:ObjectProxy, key:string, value:any) -> nil",
                    "obj.position = {x = n, y = n} moves the object and obj.enable = b runs or "
                    "pauses its script; other keys are ignored.",
                    "obj: the object\n"
                    "key: the field\n"
                    "value: the new value; a position that is not a table raises"),
        luaFunction("__gc", lua_objproxy_gc_impl, "(obj:ObjectProxy) -> nil",
                    "Detach the object from a collected proxy.",
                    "obj: the proxy"),
    };
    static constexpr LuaApiModule kMetaModule = luaApiModule(
        LuaApiScope::Metatable, "ObjectProxy",
        "A scene object, from engine.scene.find or spawn.", kMeta);
    // luaL_newmetatable returns 1 if new (creates it), 0 if it already exists
    if (luaL_newmetatable(L, OBJECT_PROXY_METATABLE)) {
        luaApiSetFields(L, -1, kMetaModule);
    }
    lua_pop(L, 1);  // always pop — both new and existing cases leave table on stack
    setProxyMethods(L, kObjectProxyMethods, kMethodsModule);
}

// Create a component proxy's metatable from its descriptors (if new).
static void newProxyMetatable(lua_State* L, const char* name, const LuaApiModule& meta) {
    if (luaL_newmetatable(L, name)) {
        luaApiSetFields(L, -1, meta);
    }
    lua_pop(L, 1);
}

// C_Position_Proxy (Phase 39): x/y fields plus getX/getY. The __newindex makes
// it the writable proxy ADR-0003 §2 requires (p.x = N).
static void registerPositionProxy(lua_State* L) {
    static constexpr LuaApiEntry kMethods[] = {
        luaFunction("getX", lua_cposition_getX, "() -> int", "The column; the same as p.x."),
        luaFunction("getY", lua_cposition_getY, "() -> int", "The row; the same as p.y."),
    };
    static constexpr LuaApiModule kMethodsModule = luaApiModule(
        LuaApiScope::Methods, "C_Position", "Position methods, called as p:name().", kMethods);
    static constexpr LuaApiEntry kMeta[] = {
        luaFunction("__index", lua_cposition_proxy_index_impl,
                    "(p:C_Position, key:string) -> any",
                    "p.x and p.y (int) read the position; other keys look up a method.",
                    "p: the component\n"
                    "key: a field or method name; unknown keys give nil")
            .note("Raises once the component is destroyed."),
        luaFunction("__newindex", lua_cposition_proxy_newindex_impl,
                    "(p:C_Position, key:string, value:int) -> nil",
                    "p.x = n and p.y = n move the object; other keys are ignored.",
                    "p: the component\n"
                    "key: \"x\" or \"y\"\n"
                    "value: the new coordinate"),
    };
    static constexpr LuaApiModule kMetaModule = luaApiModule(
        LuaApiScope::Metatable, "C_Position", "An object's position.", kMeta);
    newProxyMetatable(L, CPOSITION_PROXY_METATABLE, kMetaModule);
    setProxyMethods(L, kPositionMethods, kMethodsModule);
}

// C_Timer_Proxy (Phase 40): timer:after/every/cancel.
static void registerTimerProxy(lua_State* L) {
    static constexpr LuaApiEntry kMethods[] = {
        luaFunction("after", lua_timer_after, "(seconds:number, fn:function) -> id:int",
                    "Call fn(self) once after a delay; 0 when all 8 timers are busy.",
                    "seconds: the delay\n"
                    "fn: the callback; self is the owning object's script"),
        luaFunction("every", lua_timer_every, "(seconds:number, fn:function) -> id:int",
                    "Call fn(self) at a fixed interval; 0 when all 8 timers are busy.",
                    "seconds: the interval\n"
                    "fn: the callback; self is the owning object's script"),
        luaFunction("cancel", lua_timer_cancel, "(id:int) -> nil",
                    "Stop a timer; unknown ids are ignored.",
                    "id: the id after or every returned"),
    };
    static constexpr LuaApiModule kMethodsModule = luaApiModule(
        LuaApiScope::Methods, "C_Timer", "Timer methods, called as timer:name(...).", kMethods);
    static constexpr LuaApiEntry kMeta[] = {
        luaTable("__index", kMethodsModule, "The methods; each raises once the component is destroyed."),
    };
    static constexpr LuaApiModule kMetaModule = luaApiModule(
        LuaApiScope::Metatable, "C_Timer", "An object's callback timers.", kMeta);
    newProxyMetatable(L, CTIMER_PROXY_METATABLE, kMetaModule);
}

// C_StateMachine_Proxy (Phase 41): fsm:addState/setState/getState.
static void registerStateMachineProxy(lua_State* L) {
    static constexpr LuaApiEntry kMethods[] = {
        luaFunction("addState", lua_fsm_addState, "(name:string, callbacks:table) -> nil",
                    "Add a state with optional enter, exit and update callbacks.",
                    "name: the state; up to 31 bytes\n"
                    "callbacks: {enter = fn(self), exit = fn(self), update = fn(self, dt)}, each "
                    "optional")
            .note("Raises when all 8 states are used or the name is too long."),
        luaFunction("setState", lua_fsm_setState, "(name:string) -> nil",
                    "Switch state at the end of the next update.",
                    "name: the state"),
        luaFunction("getState", lua_fsm_getState, "() -> string",
                    "The current state's name; \"\" before the first switch."),
    };
    static constexpr LuaApiModule kMethodsModule = luaApiModule(
        LuaApiScope::Methods, "C_StateMachine", "State machine methods, called as fsm:name(...).",
        kMethods);
    static constexpr LuaApiEntry kMeta[] = {
        luaTable("__index", kMethodsModule, "The methods; each raises once the component is destroyed."),
    };
    static constexpr LuaApiModule kMetaModule = luaApiModule(
        LuaApiScope::Metatable, "C_StateMachine", "An object's state machine.", kMeta);
    newProxyMetatable(L, CFSM_PROXY_METATABLE, kMetaModule);
}

// C_Camera_Proxy (Phase 44): the scene camera component's methods.
static void registerCameraProxy(lua_State* L) {
    static constexpr LuaApiEntry kMethods[] = {
        luaFunction("setPosition", lua_ccamera_proxy_setPosition, "(x:number, y:number) -> nil",
                    "Move the camera.",
                    "x: world column\n"
                    "y: world row"),
        luaFunction("getPosition", lua_ccamera_proxy_getPosition, "() -> x:number, y:number",
                    "The camera's position."),
        luaFunction("lookAt", lua_ccamera_proxy_lookAt,
                    "(x:number, y:number, speed:number?=1) -> nil",
                    "Glide the camera toward a point; a speed of 1 or more jumps there.",
                    "x: world column\n"
                    "y: world row\n"
                    "speed: how fast it glides; 0.1 covers the distance in about a second"),
        luaFunction("shake", lua_ccamera_proxy_shake, "(intensity:number, duration:number) -> nil",
                    "Shake the camera.",
                    "intensity: how far it shakes, in pixels\n"
                    "duration: seconds"),
        luaFunction("setBounds", lua_ccamera_proxy_setBounds,
                    "(minX:number, minY:number, maxX:number, maxY:number) -> nil",
                    "Keep the camera's position inside a rectangle.",
                    "minX: left limit\n"
                    "minY: top limit\n"
                    "maxX: right limit\n"
                    "maxY: bottom limit"),
        luaFunction("clearBounds", lua_ccamera_proxy_clearBounds, "() -> nil",
                    "Let the camera move anywhere again."),
    };
    static constexpr LuaApiModule kMethodsModule = luaApiModule(
        LuaApiScope::Methods, "C_Camera", "Camera methods, called as cam:name(...).", kMethods);
    static constexpr LuaApiEntry kMeta[] = {
        luaTable("__index", kMethodsModule, "The methods; each raises once the camera is destroyed."),
    };
    static constexpr LuaApiModule kMetaModule = luaApiModule(
        LuaApiScope::Metatable, "C_Camera", "A scene camera component.", kMeta);
    newProxyMetatable(L, CCAMERA_PROXY_METATABLE, kMetaModule);
}

// C_Sprite_Proxy (ADR-0003 §5, #81): clips, events, flips, scrub. The
// __newindex makes hflip/vflip/visible the writable side (§2).
static void registerSpriteProxy(lua_State* L) {
    static constexpr LuaApiEnum kLoopMode = luaApiEnum("LoopMode", kLoopModeNames);
    static constexpr LuaApiEntry kMethods[] = {
        luaFunction("setSheet", lua_sprite_setSheet, "(handle:int) -> nil",
                    "Draw frames from a loaded sprite sheet; an invalid handle raises.",
                    "handle: a sprite handle from engine.sprite.load"),
        luaFunction("setClips", lua_sprite_setClips,
                    "(handle:int) -> nil\n"
                    "(clips:table) -> nil",
                    "Replace the clips: the ones a .njn sprite carried, or a list of "
                    "{name, loop, frames = {{frame, dur, event}, ...}}.",
                    "handle: a sprite handle from engine.sprite.load; an invalid one raises\n"
                    "clips: clip tables; loop is a LoopMode name or 0..2, dur is milliseconds "
                    "(100 by default), event is a number polled through frameEvent")
            .withEnum(kLoopMode),
        luaFunction("play", lua_sprite_play, "(name:string) -> boolean",
                    "Start a clip from its first frame; false when there is no such clip or it is "
                    "empty.",
                    "name: the clip's name"),
        luaFunction("setFlip", lua_sprite_setFlip, "(h:boolean, v:boolean) -> nil",
                    "Mirror the sprite; nil counts as false.",
                    "h: mirror left to right\n"
                    "v: mirror top to bottom"),
        luaFunction("setFrameForAngle", lua_sprite_setFrameForAngle,
                    "(angle:number, minAngle:number, maxAngle:number) -> nil",
                    "Show the frame an angle maps to and stop timed playback.",
                    "angle: the current angle\n"
                    "minAngle: the angle of the first frame\n"
                    "maxAngle: the angle of the last frame"),
        luaFunction("setFrame", lua_sprite_setFrame, "(index:int) -> nil",
                    "Show one frame of the sheet.",
                    "index: the frame, from 0"),
        luaFunction("setFPS", lua_sprite_setFPS, "(fps:number) -> nil",
                    "Frame rate of the whole-sheet animation used while no clip plays; also stops "
                    "angle scrubbing.",
                    "fps: frames per second"),
        luaFunction("setMode", lua_sprite_setMode, "(mode:LoopMode) -> nil",
                    "How the whole-sheet animation repeats.",
                    "mode: a LoopMode name in any case, or 0..2; anything else loops")
            .withEnum(kLoopMode),
    };
    static constexpr LuaApiModule kMethodsModule = luaApiModule(
        LuaApiScope::Methods, "C_Sprite", "Sprite methods, called as sp:name(...).", kMethods);
    static constexpr LuaApiEntry kMeta[] = {
        luaFunction("__index", lua_csprite_proxy_index_impl, "(sp:C_Sprite, key:string) -> any",
                    "sp.frame, sp.clip (nil when none), sp.hflip, sp.vflip, sp.visible, sp.done, "
                    "sp.justAdvanced, sp.justCompleted and sp.frameEvent read the sprite; other "
                    "keys look up a method.",
                    "sp: the component\n"
                    "key: a field or method name; unknown keys give nil")
            .note("Raises once the component is destroyed."),
        luaFunction("__newindex", lua_csprite_proxy_newindex_impl,
                    "(sp:C_Sprite, key:string, value:boolean) -> nil",
                    "Writes hflip, vflip and visible; other keys are ignored.",
                    "sp: the component\n"
                    "key: the field\n"
                    "value: the new flag"),
    };
    static constexpr LuaApiModule kMetaModule = luaApiModule(
        LuaApiScope::Metatable, "C_Sprite", "A retained, clip-animated sprite.", kMeta);
    newProxyMetatable(L, CSPRITE_PROXY_METATABLE, kMetaModule);
    setProxyMethods(L, kSpriteMethods, kMethodsModule);
}

// C_Body_Proxy (ADR-0003 §4, #80): the circle body. Writable
// x/y/vx/vy/radius/restitution/drag via __newindex.
static void registerBodyProxy(lua_State* L) {
    static constexpr LuaApiEntry kMethods[] = {
        luaFunction("step", lua_body_step, "(dt:number) -> nil",
                    "Advance the body by dt seconds, in substeps, against its collider set.",
                    "dt: seconds"),
        luaFunction("setGravity", lua_body_setGravity, "(gx:number, gy:number) -> nil",
                    "Set the body's gravity.",
                    "gx: acceleration along x\n"
                    "gy: acceleration along y (positive is down)"),
        luaFunction("setSubsteps", lua_body_setSubsteps, "(n:int) -> nil",
                    "How many substeps each step runs.",
                    "n: the substep count"),
        luaFunction("setColliders", lua_body_setColliders, "(set:ColliderSet) -> nil",
                    "Collide against a scene's collider set.",
                    "set: from engine.scene.colliders()"),
        luaFunction("numContacts", lua_body_numContacts, "() -> int",
                    "How many contacts the last step made."),
        luaFunction("contact", lua_body_contact, "(i:int) -> contact:table?",
                    "A contact of the last step: {kind, x, y, nx, ny, relSpeed}; nil past the end.",
                    "i: the contact, from 1"),
    };
    static constexpr LuaApiModule kMethodsModule = luaApiModule(
        LuaApiScope::Methods, "C_Body", "Body methods, called as body:name(...).", kMethods);
    static constexpr LuaApiEntry kMeta[] = {
        luaFunction("__index", lua_cbody_proxy_index_impl, "(body:C_Body, key:string) -> any",
                    "body.x, y, vx, vy, radius, restitution and drag (numbers) read the body; "
                    "other keys look up a method.",
                    "body: the component\n"
                    "key: a field or method name; unknown keys give nil")
            .note("Raises once the component is destroyed."),
        luaFunction("__newindex", lua_cbody_proxy_newindex_impl,
                    "(body:C_Body, key:string, value:number) -> nil",
                    "Writes x, y, vx, vy, radius, restitution and drag; other keys are ignored.",
                    "body: the component\n"
                    "key: the field\n"
                    "value: the new value; a non-number raises, whatever the key"),
    };
    static constexpr LuaApiModule kMetaModule = luaApiModule(
        LuaApiScope::Metatable, "C_Body", "The single dynamic circle body.", kMeta);
    newProxyMetatable(L, CBODY_PROXY_METATABLE, kMetaModule);
    setProxyMethods(L, kBodyMethods, kMethodsModule);
}

// ColliderSet_Proxy (ADR-0003 §4, #80): the scene-level collider resource.
static void registerColliderSetProxy(lua_State* L) {
    static constexpr LuaApiEntry kMethods[] = {
        luaFunction("addSeg", lua_colliders_addSeg,
                    "(ax:number, ay:number, bx:number, by:number, restitution:number?=0.5, "
                    "kind:int?=0) -> nil",
                    "Add a line segment collider.",
                    "ax: start column\n"
                    "ay: start row\n"
                    "bx: end column\n"
                    "by: end row\n"
                    "restitution: bounciness, 0..1\n"
                    "kind: 0 wall, 1 bouncy, 2 hazard, 3 goal, 4 flipper; reported in contacts"),
        luaFunction("addCircle", lua_colliders_addCircle,
                    "(cx:number, cy:number, r:number, restitution:number?=0.5, kind:int?=0) -> nil",
                    "Add a circle collider.",
                    "cx: centre column\n"
                    "cy: centre row\n"
                    "r: radius\n"
                    "restitution: bounciness, 0..1\n"
                    "kind: 0 wall, 1 bouncy, 2 hazard, 3 goal, 4 flipper; reported in contacts"),
        luaFunction("addAabb", lua_colliders_addAabb,
                    "(minX:number, minY:number, maxX:number, maxY:number, "
                    "restitution:number?=0.1, kind:int?=0) -> nil",
                    "Add a rectangle collider by its corners.",
                    "minX: left edge\n"
                    "minY: top edge\n"
                    "maxX: right edge\n"
                    "maxY: bottom edge\n"
                    "restitution: bounciness, 0..1\n"
                    "kind: 0 wall, 1 bouncy, 2 hazard, 3 goal, 4 flipper; reported in contacts"),
        luaFunction("addSolidRects", lua_colliders_addSolidRects,
                    "(map:Tilemap, restitution:number?=0.1, kind:int?=0) -> int",
                    "Add rectangles covering a map's solid tiles; returns how many.",
                    "map: a map handle or a scene C_Tilemap\n"
                    "restitution: bounciness, 0..1\n"
                    "kind: 0 wall, 1 bouncy, 2 hazard, 3 goal, 4 flipper; reported in contacts"),
        luaFunction("addFlipper", lua_colliders_addFlipper,
                    "(pivotX:number, pivotY:number, length:number, restAngle:number, "
                    "activeAngle:number, restitution:number?=0.2) -> int",
                    "Add a flipper at rest; returns its index, from 1.",
                    "pivotX: pivot column\n"
                    "pivotY: pivot row\n"
                    "length: arm length\n"
                    "restAngle: angle at rest, in radians\n"
                    "activeAngle: angle when active, in radians\n"
                    "restitution: bounciness, 0..1"),
        luaFunction("setFlipperActive", lua_colliders_setFlipperActive,
                    "(i:int, active:boolean) -> nil",
                    "Spring a flipper toward its active or rest angle.",
                    "i: the flipper, from 1; a bad index raises\n"
                    "active: true for the active angle"),
        luaFunction("setFlipperTarget", lua_colliders_setFlipperTarget,
                    "(i:int, angle:number) -> nil",
                    "Spring a flipper toward any angle.",
                    "i: the flipper, from 1; a bad index raises\n"
                    "angle: the target, in radians"),
        luaFunction("setFlipperAngle", lua_colliders_setFlipperAngle,
                    "(i:int, angle:number) -> nil",
                    "Put a flipper at an angle at once.",
                    "i: the flipper, from 1; a bad index raises\n"
                    "angle: the angle, in radians"),
        luaFunction("numFlippers", lua_colliders_numFlippers, "() -> int", "How many flippers."),
        luaFunction("count", lua_colliders_count, "() -> int", "How many colliders in all."),
        luaFunction("clear", lua_colliders_clear, "() -> nil", "Remove every collider."),
    };
    static constexpr LuaApiModule kMethodsModule = luaApiModule(
        LuaApiScope::Methods, "ColliderSet", "Collider set methods, called as set:name(...).",
        kMethods);
    static constexpr LuaApiEntry kMeta[] = {
        luaTable("__index", kMethodsModule, "The methods."),
    };
    static constexpr LuaApiModule kMetaModule = luaApiModule(
        LuaApiScope::Metatable, "ColliderSet", "A scene's static colliders.", kMeta);
    newProxyMetatable(L, COLLIDERSET_PROXY_METATABLE, kMetaModule);
}

void LuaBindings::registerComponentProxyMetatable() {
    lua_State* L = engine->getState();
    if (!L) return;

    registerPositionProxy(L);
    registerTimerProxy(L);
    registerStateMachineProxy(L);

    // Register C_Tilemap_Proxy metatable (Phase 43: tilemap component): the
    // same methods as the scene-free map handle (engine.tilemap.load, #256).
    static constexpr LuaApiEntry kTilemapProxyMeta[] = {
        luaTable("__index", kTilemapMethodsModule, "The map's methods."),
    };
    static constexpr LuaApiModule kTilemapProxyMetaModule = luaApiModule(
        LuaApiScope::Metatable, "C_Tilemap", "Map method lookup.", kTilemapProxyMeta);
    newProxyMetatable(L, CTILEMAP_PROXY_METATABLE, kTilemapProxyMetaModule);

    registerCameraProxy(L);
    registerSpriteProxy(L);
    registerBodyProxy(L);
    registerColliderSetProxy(L);
}

} // namespace enjin2
