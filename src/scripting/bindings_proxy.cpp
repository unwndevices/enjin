#include "bindings_internal.hpp"
#include "../../include/enjin2/scripting/component_registry.hpp"
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

// __index metamethod for C_Position_Proxy.
// Methods: getX(), getY()
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
    // C_Position setter. The getX()/getY() methods stay for back-compat.
    if (strcmp(key, "x") == 0) {
        auto* pos = static_cast<enjin2::C_Position*>(proxy->component);
        lua_pushinteger(L, static_cast<lua_Integer>(pos->getPosition().x));
        return 1;
    } else if (strcmp(key, "y") == 0) {
        auto* pos = static_cast<enjin2::C_Position*>(proxy->component);
        lua_pushinteger(L, static_cast<lua_Integer>(pos->getPosition().y));
        return 1;
    }

    if (strcmp(key, "getX") == 0) {
        lua_pushcfunction(L, [](lua_State* L2) -> int {
            enjin2::ComponentProxy* p = static_cast<enjin2::ComponentProxy*>(
                luaL_checkudata(L2, 1, "C_Position_Proxy"));
            if (!p || !p->valid || !p->component) {
                luaL_error(L2, "component has been destroyed");
                return 0;
            }
            enjin2::C_Position* pos2 = static_cast<enjin2::C_Position*>(p->component);
            lua_pushinteger(L2, static_cast<lua_Integer>(pos2->getPosition().x));
            return 1;
        });
        return 1;
    } else if (strcmp(key, "getY") == 0) {
        lua_pushcfunction(L, [](lua_State* L2) -> int {
            enjin2::ComponentProxy* p = static_cast<enjin2::ComponentProxy*>(
                luaL_checkudata(L2, 1, "C_Position_Proxy"));
            if (!p || !p->valid || !p->component) {
                luaL_error(L2, "component has been destroyed");
                return 0;
            }
            enjin2::C_Position* pos2 = static_cast<enjin2::C_Position*>(p->component);
            lua_pushinteger(L2, static_cast<lua_Integer>(pos2->getPosition().y));
            return 1;
        });
        return 1;
    }

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

// __index metamethod for C_Timer_Proxy
static int lua_ctimer_proxy_index_impl(lua_State* L) {
    auto* proxy = static_cast<enjin2::ComponentProxy*>(
        luaL_checkudata(L, 1, CTIMER_PROXY_METATABLE));
    if (!proxy || !proxy->valid || !proxy->component) {
        luaL_error(L, "component has been destroyed");
        return 0;
    }
    const char* key = lua_tostring(L, 2);
    if (!key) { lua_pushnil(L); return 1; }

    if (strcmp(key, "after") == 0) {
        lua_pushcfunction(L, lua_timer_after);
        return 1;
    } else if (strcmp(key, "every") == 0) {
        lua_pushcfunction(L, lua_timer_every);
        return 1;
    } else if (strcmp(key, "cancel") == 0) {
        lua_pushcfunction(L, lua_timer_cancel);
        return 1;
    }
    lua_pushnil(L);
    return 1;
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

// __index metamethod for C_StateMachine_Proxy
static int lua_cfsm_proxy_index_impl(lua_State* L) {
    auto* proxy = static_cast<enjin2::ComponentProxy*>(
        luaL_checkudata(L, 1, CFSM_PROXY_METATABLE));
    if (!proxy || !proxy->valid || !proxy->component) {
        luaL_error(L, "component has been destroyed");
        return 0;
    }
    const char* key = lua_tostring(L, 2);
    if (!key) { lua_pushnil(L); return 1; }

    if (strcmp(key, "addState") == 0) {
        lua_pushcfunction(L, lua_fsm_addState);
        return 1;
    } else if (strcmp(key, "setState") == 0) {
        lua_pushcfunction(L, lua_fsm_setState);
        return 1;
    } else if (strcmp(key, "getState") == 0) {
        lua_pushcfunction(L, lua_fsm_getState);
        return 1;
    }
    lua_pushnil(L);
    return 1;
}

//==============================================================================
// C_Tilemap_Proxy Metatable Implementation (Phase 43: tilemap Lua API)
//==============================================================================

// Helper macro: validate C_Tilemap_Proxy userdata and cast to C_Tilemap*.
// Must be at the top of every proxy method.
#define CTILEMAP_PROXY_CHECK(L, varname)                                          \
    auto* proxy = static_cast<enjin2::ComponentProxy*>(                           \
        luaL_checkudata(L, 1, CTILEMAP_PROXY_METATABLE));                         \
    if (!proxy || !proxy->valid || !proxy->component) {                           \
        luaL_error(L, "component has been destroyed");                            \
        return 0;                                                                 \
    }                                                                             \
    auto* (varname) = static_cast<enjin2::C_Tilemap*>(proxy->component)

// tilemap:setTile(tx, ty, cell) — TMAP-05
// `cell` is the full 16-bit packed cell (tile id in the low 9 bits); a plain
// tile id 0-511 works as-is (band/flip/palbank = 0).
static int lua_tilemap_setTile(lua_State* L) {
    CTILEMAP_PROXY_CHECK(L, tm);
    uint8_t  tx   = static_cast<uint8_t>(luaL_checkinteger(L, 2));
    uint8_t  ty   = static_cast<uint8_t>(luaL_checkinteger(L, 3));
    uint16_t cell = static_cast<uint16_t>(luaL_checkinteger(L, 4) & 0xFFFF);
    tm->setTile(tx, ty, cell);
    return 0;
}

// tilemap:getTile(tx, ty) -> tileId — TMAP-05
static int lua_tilemap_getTile(lua_State* L) {
    CTILEMAP_PROXY_CHECK(L, tm);
    uint8_t tx = static_cast<uint8_t>(luaL_checkinteger(L, 2));
    uint8_t ty = static_cast<uint8_t>(luaL_checkinteger(L, 3));
    lua_pushinteger(L, static_cast<lua_Integer>(tm->getTile(tx, ty)));
    return 1;
}

// tilemap:setTiles(flat_table, w, h) — TMAP-07
static int lua_tilemap_setTiles(lua_State* L) {
    CTILEMAP_PROXY_CHECK(L, tm);
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
    CTILEMAP_PROXY_CHECK(L, tm);
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
    CTILEMAP_PROXY_CHECK(L, tm);
    int16_t sx = static_cast<int16_t>(luaL_checkinteger(L, 2));
    int16_t sy = static_cast<int16_t>(luaL_checkinteger(L, 3));
    tm->setScroll(sx, sy);
    return 0;
}

// tilemap:getScroll() -> sx, sy — TMAP-04/05
static int lua_tilemap_getScroll(lua_State* L) {
    CTILEMAP_PROXY_CHECK(L, tm);
    lua_pushinteger(L, static_cast<lua_Integer>(tm->getScrollX()));
    lua_pushinteger(L, static_cast<lua_Integer>(tm->getScrollY()));
    return 2;
}

// tilemap:pixelToTile(px, py) -> tx, ty — TMAP-06
static int lua_tilemap_pixelToTile(lua_State* L) {
    CTILEMAP_PROXY_CHECK(L, tm);
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
    CTILEMAP_PROXY_CHECK(L, tm);
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
    CTILEMAP_PROXY_CHECK(L, tm);
    int16_t px = static_cast<int16_t>(luaL_checkinteger(L, 2));
    int16_t py = static_cast<int16_t>(luaL_checkinteger(L, 3));
    lua_pushinteger(L, static_cast<lua_Integer>(tm->tileAtPixel(px, py)));
    return 1;
}

// tilemap:getMapSize() -> w, h — TMAP-05
static int lua_tilemap_getMapSize(lua_State* L) {
    CTILEMAP_PROXY_CHECK(L, tm);
    lua_pushinteger(L, static_cast<lua_Integer>(tm->getMapWidth()));
    lua_pushinteger(L, static_cast<lua_Integer>(tm->getMapHeight()));
    return 2;
}

// tilemap:setAttrs(flatTable) — flat {flags,kind, flags,kind, …} (ADR-0003 §3)
static int lua_tilemap_setAttrs(lua_State* L) {
    CTILEMAP_PROXY_CHECK(L, tm);
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
    CTILEMAP_PROXY_CHECK(L, tm);
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
    CTILEMAP_PROXY_CHECK(L, tm);
    const uint8_t tx = static_cast<uint8_t>(luaL_checkinteger(L, 2));
    const uint8_t ty = static_cast<uint8_t>(luaL_checkinteger(L, 3));
    const TileAttr a = tm->attrAt(tx, ty);
    lua_pushinteger(L, a.flags);
    lua_pushinteger(L, a.kind);
    return 2;
}

// tilemap:attrAtPixel(px, py) -> flags, kind
static int lua_tilemap_attrAtPixel(lua_State* L) {
    CTILEMAP_PROXY_CHECK(L, tm);
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
    CTILEMAP_PROXY_CHECK(L, tm);
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
    CTILEMAP_PROXY_CHECK(L, tm);
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
    CTILEMAP_PROXY_CHECK(L, tm);
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
    CTILEMAP_PROXY_CHECK(L, tm);
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

#undef CTILEMAP_PROXY_CHECK

// __index metamethod for C_Tilemap_Proxy — dispatches all method names
static int lua_ctilemap_proxy_index_impl(lua_State* L) {
    auto* proxy = static_cast<enjin2::ComponentProxy*>(
        luaL_checkudata(L, 1, CTILEMAP_PROXY_METATABLE));
    if (!proxy || !proxy->valid || !proxy->component) {
        luaL_error(L, "component has been destroyed");
        return 0;
    }
    const char* key = lua_tostring(L, 2);
    if (!key) { lua_pushnil(L); return 1; }

    if (strcmp(key, "setTile") == 0) {
        lua_pushcfunction(L, lua_tilemap_setTile);
    } else if (strcmp(key, "getTile") == 0) {
        lua_pushcfunction(L, lua_tilemap_getTile);
    } else if (strcmp(key, "setTiles") == 0) {
        lua_pushcfunction(L, lua_tilemap_setTiles);
    } else if (strcmp(key, "setSheet") == 0) {
        lua_pushcfunction(L, lua_tilemap_setSheet);
    } else if (strcmp(key, "setScroll") == 0) {
        lua_pushcfunction(L, lua_tilemap_setScroll);
    } else if (strcmp(key, "getScroll") == 0) {
        lua_pushcfunction(L, lua_tilemap_getScroll);
    } else if (strcmp(key, "pixelToTile") == 0) {
        lua_pushcfunction(L, lua_tilemap_pixelToTile);
    } else if (strcmp(key, "tileToPixel") == 0) {
        lua_pushcfunction(L, lua_tilemap_tileToPixel);
    } else if (strcmp(key, "tileAtPixel") == 0) {
        lua_pushcfunction(L, lua_tilemap_tileAtPixel);
    } else if (strcmp(key, "getMapSize") == 0) {
        lua_pushcfunction(L, lua_tilemap_getMapSize);
    } else if (strcmp(key, "setAttrs") == 0) {
        lua_pushcfunction(L, lua_tilemap_setAttrs);
    } else if (strcmp(key, "setPalbank") == 0) {
        lua_pushcfunction(L, lua_tilemap_setPalbank);
    } else if (strcmp(key, "attrAt") == 0) {
        lua_pushcfunction(L, lua_tilemap_attrAt);
    } else if (strcmp(key, "attrAtPixel") == 0) {
        lua_pushcfunction(L, lua_tilemap_attrAtPixel);
    } else if (strcmp(key, "sweepAabb") == 0) {
        lua_pushcfunction(L, lua_tilemap_sweepAabb);
    } else if (strcmp(key, "sweepCircle") == 0) {
        lua_pushcfunction(L, lua_tilemap_sweepCircle);
    } else if (strcmp(key, "forEachCellIn") == 0) {
        lua_pushcfunction(L, lua_tilemap_forEachCellIn);
    } else if (strcmp(key, "buildSolidRects") == 0) {
        lua_pushcfunction(L, lua_tilemap_buildSolidRects);
    } else {
        lua_pushnil(L);
    }
    return 1;
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

// __index metamethod for C_Camera_Proxy
static int lua_ccamera_proxy_index_impl(lua_State* L) {
    const char* key = luaL_checkstring(L, 2);
    if (strcmp(key, "setPosition") == 0) { lua_pushcfunction(L, lua_ccamera_proxy_setPosition); return 1; }
    if (strcmp(key, "getPosition") == 0) { lua_pushcfunction(L, lua_ccamera_proxy_getPosition); return 1; }
    if (strcmp(key, "lookAt") == 0)      { lua_pushcfunction(L, lua_ccamera_proxy_lookAt);      return 1; }
    if (strcmp(key, "shake") == 0)       { lua_pushcfunction(L, lua_ccamera_proxy_shake);       return 1; }
    if (strcmp(key, "setBounds") == 0)   { lua_pushcfunction(L, lua_ccamera_proxy_setBounds);   return 1; }
    if (strcmp(key, "clearBounds") == 0) { lua_pushcfunction(L, lua_ccamera_proxy_clearBounds); return 1; }
    lua_pushnil(L);
    return 1;
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

// Parse a loop-mode argument: accepts "once"/"loop"/"pingpong" (case-sensitive)
// or the integer 0/1/2. Defaults to Loop for anything unrecognised.
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
        if (strcmp(buf, "once") == 0)     return enjin2::NjnLoopMode::Once;
        if (strcmp(buf, "pingpong") == 0) return enjin2::NjnLoopMode::PingPong;
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

// sprite:setClips({ {name=, loop=, frames={ {frame=,dur=,event=}, ... }}, ... })
static int lua_sprite_setClips(lua_State* L) {
    CSPRITE_PROXY_CHECK(L, sp);
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

// __index for C_Sprite_Proxy: methods first, then read-only/readable properties.
static int lua_csprite_proxy_index_impl(lua_State* L) {
    auto* proxy = static_cast<enjin2::ComponentProxy*>(
        luaL_checkudata(L, 1, CSPRITE_PROXY_METATABLE));
    if (!proxy || !proxy->valid || !proxy->component) {
        luaL_error(L, "component has been destroyed");
        return 0;
    }
    const char* key = lua_tostring(L, 2);
    if (!key) { lua_pushnil(L); return 1; }

    // Methods
    if (strcmp(key, "setSheet") == 0)         { lua_pushcfunction(L, lua_sprite_setSheet); return 1; }
    if (strcmp(key, "setClips") == 0)         { lua_pushcfunction(L, lua_sprite_setClips); return 1; }
    if (strcmp(key, "play") == 0)             { lua_pushcfunction(L, lua_sprite_play); return 1; }
    if (strcmp(key, "setFlip") == 0)          { lua_pushcfunction(L, lua_sprite_setFlip); return 1; }
    if (strcmp(key, "setFrameForAngle") == 0) { lua_pushcfunction(L, lua_sprite_setFrameForAngle); return 1; }
    if (strcmp(key, "setFrame") == 0)         { lua_pushcfunction(L, lua_sprite_setFrame); return 1; }
    if (strcmp(key, "setFPS") == 0)           { lua_pushcfunction(L, lua_sprite_setFPS); return 1; }
    if (strcmp(key, "setMode") == 0)          { lua_pushcfunction(L, lua_sprite_setMode); return 1; }

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

// body:setSwept(bool)
static int lua_body_setSwept(lua_State* L) {
    CBODY_PROXY_CHECK(L, body);
    body->setSwept(lua_toboolean(L, 2) != 0);
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

#undef CBODY_PROXY_CHECK

static int lua_cbody_proxy_index_impl(lua_State* L) {
    auto* proxy = static_cast<enjin2::ComponentProxy*>(
        luaL_checkudata(L, 1, CBODY_PROXY_METATABLE));
    if (!proxy || !proxy->valid || !proxy->component) {
        luaL_error(L, "component has been destroyed");
        return 0;
    }
    const char* key = lua_tostring(L, 2);
    if (!key) { lua_pushnil(L); return 1; }

    if (strcmp(key, "step") == 0)         { lua_pushcfunction(L, lua_body_step); return 1; }
    if (strcmp(key, "setGravity") == 0)   { lua_pushcfunction(L, lua_body_setGravity); return 1; }
    if (strcmp(key, "setSubsteps") == 0)  { lua_pushcfunction(L, lua_body_setSubsteps); return 1; }
    if (strcmp(key, "setSwept") == 0)     { lua_pushcfunction(L, lua_body_setSwept); return 1; }
    if (strcmp(key, "setColliders") == 0) { lua_pushcfunction(L, lua_body_setColliders); return 1; }
    if (strcmp(key, "numContacts") == 0)  { lua_pushcfunction(L, lua_body_numContacts); return 1; }
    if (strcmp(key, "contact") == 0)      { lua_pushcfunction(L, lua_body_contact); return 1; }

    auto* body = static_cast<enjin2::C_Body*>(proxy->component);
    if (strcmp(key, "x") == 0)           { lua_pushnumber(L, body->getX()); return 1; }
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
    auto* proxy = static_cast<enjin2::ComponentProxy*>(
        luaL_checkudata(L, 1, CBODY_PROXY_METATABLE));
    if (!proxy || !proxy->valid || !proxy->component) {
        luaL_error(L, "component has been destroyed");
        return 0;
    }
    const char* key = lua_tostring(L, 2);
    if (!key) return 0;

    auto* body = static_cast<enjin2::C_Body*>(proxy->component);
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

//==============================================================================
// ColliderSet_Proxy Metatable Implementation (ADR-0003 §4, Tomodachi #80)
//
// The scene-level collider resource (engine.scene.colliders()). Non-owning: the
// userdata wraps the Scene's ColliderSet reference; the scene owns the storage.
//==============================================================================

// colliders:addSeg(ax, ay, bx, by, restitution, kind)
static int lua_colliders_addSeg(lua_State* L) {
    auto** slot = static_cast<enjin2::ColliderSet**>(
        luaL_checkudata(L, 1, COLLIDERSET_PROXY_METATABLE));
    enjin2::ColliderSet* set = *slot;
    enjin2::SegCollider s;
    s.ax = static_cast<float>(luaL_checknumber(L, 2));
    s.ay = static_cast<float>(luaL_checknumber(L, 3));
    s.bx = static_cast<float>(luaL_checknumber(L, 4));
    s.by = static_cast<float>(luaL_checknumber(L, 5));
    s.restitution = static_cast<float>(luaL_optnumber(L, 6, 0.5f));
    s.kind = static_cast<uint8_t>(luaL_optinteger(L, 7, enjin2::ColliderKinds::Wall));
    set->segments.push_back(s);
    return 0;
}

// colliders:addCircle(cx, cy, r, restitution, kind)
static int lua_colliders_addCircle(lua_State* L) {
    auto** slot = static_cast<enjin2::ColliderSet**>(
        luaL_checkudata(L, 1, COLLIDERSET_PROXY_METATABLE));
    enjin2::ColliderSet* set = *slot;
    enjin2::CircleCollider c;
    c.cx = static_cast<float>(luaL_checknumber(L, 2));
    c.cy = static_cast<float>(luaL_checknumber(L, 3));
    c.r  = static_cast<float>(luaL_checknumber(L, 4));
    c.restitution = static_cast<float>(luaL_optnumber(L, 5, 0.5f));
    c.kind = static_cast<uint8_t>(luaL_optinteger(L, 6, enjin2::ColliderKinds::Wall));
    set->circles.push_back(c);
    return 0;
}

// colliders:addAabb(x, y, w, h, restitution, kind)
static int lua_colliders_addAabb(lua_State* L) {
    auto** slot = static_cast<enjin2::ColliderSet**>(
        luaL_checkudata(L, 1, COLLIDERSET_PROXY_METATABLE));
    enjin2::ColliderSet* set = *slot;
    const float x = static_cast<float>(luaL_checknumber(L, 2));
    const float y = static_cast<float>(luaL_checknumber(L, 3));
    const float w = static_cast<float>(luaL_checknumber(L, 4));
    const float h = static_cast<float>(luaL_checknumber(L, 5));
    const float restitution = static_cast<float>(luaL_optnumber(L, 6, 0.1f));
    const uint8_t kind = static_cast<uint8_t>(luaL_optinteger(L, 7, enjin2::ColliderKinds::Wall));
    set->addAabb(x, y, x + w, y + h, restitution, kind);
    return 0;
}

// colliders:addSolidRects(tilemapProxy) -> int
// Derive AABBs from a tilemap's SOLID attrs (ADR-0003 §3 buildSolidRects) and
// append them to the set — the "tile-derived AABBs" half of §4's two collider
// sources. Returns the number of rects added.
static int lua_colliders_addSolidRects(lua_State* L) {
    auto** slot = static_cast<enjin2::ColliderSet**>(
        luaL_checkudata(L, 1, COLLIDERSET_PROXY_METATABLE));
    enjin2::ColliderSet* set = *slot;
    auto* tmProxy = static_cast<enjin2::ComponentProxy*>(
        luaL_checkudata(L, 2, CTILEMAP_PROXY_METATABLE));
    if (!tmProxy || !tmProxy->valid || !tmProxy->component) {
        luaL_error(L, "component has been destroyed");
        return 0;
    }
    auto* tm = static_cast<enjin2::C_Tilemap*>(tmProxy->component);
    const float restitution = static_cast<float>(luaL_optnumber(L, 3, 0.1f));
    const uint8_t kind = static_cast<uint8_t>(luaL_optinteger(L, 4, enjin2::ColliderKinds::Wall));

    const std::vector<enjin2::Rect> rects = tm->buildSolidRects();
    for (const auto& r : rects) {
        set->addAabb(static_cast<float>(r.x), static_cast<float>(r.y),
                     static_cast<float>(r.x + r.width),
                     static_cast<float>(r.y + r.height), restitution, kind);
    }
    lua_pushinteger(L, static_cast<lua_Integer>(rects.size()));
    return 1;
}

// colliders:addFlipper(pivotX, pivotY, length, restAngle, activeAngle, restitution)
static int lua_colliders_addFlipper(lua_State* L) {
    auto** slot = static_cast<enjin2::ColliderSet**>(
        luaL_checkudata(L, 1, COLLIDERSET_PROXY_METATABLE));
    enjin2::ColliderSet* set = *slot;
    enjin2::Flipper f;
    f.pivotX = static_cast<float>(luaL_checknumber(L, 2));
    f.pivotY = static_cast<float>(luaL_checknumber(L, 3));
    f.length = static_cast<float>(luaL_checknumber(L, 4));
    f.restAngle   = static_cast<float>(luaL_checknumber(L, 5));
    f.activeAngle = static_cast<float>(luaL_checknumber(L, 6));
    f.angle = f.restAngle; f.target = f.restAngle; f.springVel = 0.0f;
    f.restitution = static_cast<float>(luaL_optnumber(L, 7, 0.2f));
    f.kind = enjin2::ColliderKinds::Flipper;
    set->flippers.push_back(f);
    return 0;
}

// colliders:setFlipperTarget(i, angle) — drive a flipper (1-based index).
static int lua_colliders_setFlipperTarget(lua_State* L) {
    auto** slot = static_cast<enjin2::ColliderSet**>(
        luaL_checkudata(L, 1, COLLIDERSET_PROXY_METATABLE));
    enjin2::ColliderSet* set = *slot;
    const size_t i = static_cast<size_t>(luaL_checkinteger(L, 2));
    if (i < 1 || i > set->flippers.size()) { luaL_error(L, "setFlipperTarget: bad flipper index"); return 0; }
    set->flippers[i - 1].target = static_cast<float>(luaL_checknumber(L, 3));
    return 0;
}

// colliders:setFlipperAngle(i, angle) — snap a flipper's angle (and spring state).
static int lua_colliders_setFlipperAngle(lua_State* L) {
    auto** slot = static_cast<enjin2::ColliderSet**>(
        luaL_checkudata(L, 1, COLLIDERSET_PROXY_METATABLE));
    enjin2::ColliderSet* set = *slot;
    const size_t i = static_cast<size_t>(luaL_checkinteger(L, 2));
    if (i < 1 || i > set->flippers.size()) { luaL_error(L, "setFlipperAngle: bad flipper index"); return 0; }
    set->flippers[i - 1].angle = static_cast<float>(luaL_checknumber(L, 3));
    set->flippers[i - 1].target = set->flippers[i - 1].angle;
    set->flippers[i - 1].springVel = 0.0f;
    return 0;
}

// colliders:numFlippers() -> int
static int lua_colliders_numFlippers(lua_State* L) {
    auto** slot = static_cast<enjin2::ColliderSet**>(
        luaL_checkudata(L, 1, COLLIDERSET_PROXY_METATABLE));
    enjin2::ColliderSet* set = *slot;
    lua_pushinteger(L, static_cast<lua_Integer>(set->flippers.size()));
    return 1;
}

// colliders:count() -> total collider count
static int lua_colliders_count(lua_State* L) {
    auto** slot = static_cast<enjin2::ColliderSet**>(
        luaL_checkudata(L, 1, COLLIDERSET_PROXY_METATABLE));
    enjin2::ColliderSet* set = *slot;
    lua_pushinteger(L, static_cast<lua_Integer>(
        set->segments.size() + set->circles.size() + set->aabbs.size() + set->flippers.size()));
    return 1;
}

// colliders:clear()
static int lua_colliders_clear(lua_State* L) {
    auto** slot = static_cast<enjin2::ColliderSet**>(
        luaL_checkudata(L, 1, COLLIDERSET_PROXY_METATABLE));
    enjin2::ColliderSet* set = *slot;
    set->clear();
    return 0;
}

static int lua_colliderset_proxy_index_impl(lua_State* L) {
    luaL_checkudata(L, 1, COLLIDERSET_PROXY_METATABLE);
    const char* key = lua_tostring(L, 2);
    if (!key) { lua_pushnil(L); return 1; }

    if (strcmp(key, "addSeg") == 0)          { lua_pushcfunction(L, lua_colliders_addSeg); return 1; }
    if (strcmp(key, "addCircle") == 0)       { lua_pushcfunction(L, lua_colliders_addCircle); return 1; }
    if (strcmp(key, "addAabb") == 0)         { lua_pushcfunction(L, lua_colliders_addAabb); return 1; }
    if (strcmp(key, "addSolidRects") == 0)   { lua_pushcfunction(L, lua_colliders_addSolidRects); return 1; }
    if (strcmp(key, "addFlipper") == 0)      { lua_pushcfunction(L, lua_colliders_addFlipper); return 1; }
    if (strcmp(key, "setFlipperTarget") == 0){ lua_pushcfunction(L, lua_colliders_setFlipperTarget); return 1; }
    if (strcmp(key, "setFlipperAngle") == 0) { lua_pushcfunction(L, lua_colliders_setFlipperAngle); return 1; }
    if (strcmp(key, "numFlippers") == 0)     { lua_pushcfunction(L, lua_colliders_numFlippers); return 1; }
    if (strcmp(key, "count") == 0)           { lua_pushcfunction(L, lua_colliders_count); return 1; }
    if (strcmp(key, "clear") == 0)           { lua_pushcfunction(L, lua_colliders_clear); return 1; }

    lua_pushnil(L);
    return 1;
}

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

    // self:get("TypeName") / self:add("TypeName"[, params]) — the same attach
    // verb spawn()'d and find()'d objects get, so an ObjectProxy is a full
    // instance handle (ADR-0003 §2, closing the spawn-can't-get gap). Checked
    // before the property keys, mirroring ScriptProxy.
    if (strcmp(key, "get") == 0) {
        lua_pushcfunction(L, lua_objproxy_get_component_impl);
        return 1;
    }
    if (strcmp(key, "add") == 0) {
        lua_pushcfunction(L, lua_objproxy_add_component_impl);
        return 1;
    }
    if (strcmp(key, "destroy") == 0) {
        lua_pushcfunction(L, lua_objproxy_destroy_impl);
        return 1;
    }

    if (strcmp(key, "name") == 0) {
        const char* n = obj->getName();
        if (n) lua_pushstring(L, n); else lua_pushnil(L);
        return 1;
    } else if (strcmp(key, "hasTag") == 0) {
        // Return a function: proxy:hasTag(tag) -> boolean
        // Non-capturing lambda converts to plain function pointer (C++11, safe for lua_pushcfunction)
        lua_pushcfunction(L, [](lua_State* L2) -> int {
            enjin2::ObjectProxy* p = static_cast<enjin2::ObjectProxy*>(
                luaL_checkudata(L2, 1, OBJECT_PROXY_METATABLE));
            if (!p || !p->valid || !p->object) {
                luaL_error(L2, "object has been destroyed");
                return 0;
            }
            const char* tag = luaL_checkstring(L2, 2);
            lua_pushboolean(L2, p->object->hasTag(tag) ? 1 : 0);
            return 1;
        });
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
    // luaL_newmetatable returns 1 if new (creates it), 0 if it already exists
    if (luaL_newmetatable(L, OBJECT_PROXY_METATABLE)) {
        lua_pushcfunction(L, lua_objproxy_index_impl);
        lua_setfield(L, -2, "__index");
        lua_pushcfunction(L, lua_objproxy_newindex_impl);
        lua_setfield(L, -2, "__newindex");
        // __gc: clear Object::m_luaProxy back-pointer when Lua frees this proxy.
        // Prevents heap-use-after-free in Object::~Object() (PROXY-GC-01).
        lua_pushcfunction(L, lua_objproxy_gc_impl);
        lua_setfield(L, -2, "__gc");
    }
    lua_pop(L, 1);  // always pop — both new and existing cases leave table on stack
}

void LuaBindings::registerComponentProxyMetatable() {
    lua_State* L = engine->getState();
    if (!L) return;

    // Register C_Position_Proxy metatable (proof-of-concept for Phase 39).
    // __newindex makes it the writable proxy ADR-0003 §2 requires (p.x = N).
    if (luaL_newmetatable(L, CPOSITION_PROXY_METATABLE)) {
        lua_pushcfunction(L, lua_cposition_proxy_index_impl);
        lua_setfield(L, -2, "__index");
        lua_pushcfunction(L, lua_cposition_proxy_newindex_impl);
        lua_setfield(L, -2, "__newindex");
    }
    lua_pop(L, 1);

    // Register C_Timer_Proxy metatable (Phase 40: timer:after/every/cancel)
    if (luaL_newmetatable(L, CTIMER_PROXY_METATABLE)) {
        lua_pushcfunction(L, lua_ctimer_proxy_index_impl);
        lua_setfield(L, -2, "__index");
    }
    lua_pop(L, 1);

    // Register C_StateMachine_Proxy metatable (Phase 41: fsm:addState/setState/getState)
    if (luaL_newmetatable(L, CFSM_PROXY_METATABLE)) {
        lua_pushcfunction(L, lua_cfsm_proxy_index_impl);
        lua_setfield(L, -2, "__index");
    }
    lua_pop(L, 1);

    // Register C_Tilemap_Proxy metatable (Phase 43: tilemap component)
    if (luaL_newmetatable(L, CTILEMAP_PROXY_METATABLE)) {
        lua_pushcfunction(L, lua_ctilemap_proxy_index_impl);
        lua_setfield(L, -2, "__index");
    }
    lua_pop(L, 1);

    // Register C_Camera_Proxy metatable (Phase 44: cam:setPosition/getPosition/lookAt/shake/setBounds/clearBounds)
    if (luaL_newmetatable(L, CCAMERA_PROXY_METATABLE)) {
        lua_pushcfunction(L, lua_ccamera_proxy_index_impl);
        lua_setfield(L, -2, "__index");
    }
    lua_pop(L, 1);

    // Register C_Sprite_Proxy metatable (ADR-0003 §5, #81: clips, events, flips,
    // scrub). __newindex makes hflip/vflip/visible the writable side (§2).
    if (luaL_newmetatable(L, CSPRITE_PROXY_METATABLE)) {
        lua_pushcfunction(L, lua_csprite_proxy_index_impl);
        lua_setfield(L, -2, "__index");
        lua_pushcfunction(L, lua_csprite_proxy_newindex_impl);
        lua_setfield(L, -2, "__newindex");
    }
    lua_pop(L, 1);

    // Register C_Body_Proxy metatable (ADR-0003 §4, #80: circle body). Writable
    // x/y/vx/vy/radius/restitution/drag via __newindex.
    if (luaL_newmetatable(L, CBODY_PROXY_METATABLE)) {
        lua_pushcfunction(L, lua_cbody_proxy_index_impl);
        lua_setfield(L, -2, "__index");
        lua_pushcfunction(L, lua_cbody_proxy_newindex_impl);
        lua_setfield(L, -2, "__newindex");
    }
    lua_pop(L, 1);

    // Register ColliderSet_Proxy metatable (ADR-0003 §4, #80: scene-level
    // collider resource — addSeg/addCircle/addAabb/addFlipper/setFlipperTarget).
    if (luaL_newmetatable(L, COLLIDERSET_PROXY_METATABLE)) {
        lua_pushcfunction(L, lua_colliderset_proxy_index_impl);
        lua_setfield(L, -2, "__index");
    }
    lua_pop(L, 1);
}

} // namespace enjin2
