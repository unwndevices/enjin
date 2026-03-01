#include "bindings_internal.hpp"
#include "../../include/enjin2/components/position.hpp"
#include "../../include/enjin2/components/timer.hpp"
#include "../../include/enjin2/components/state_machine.hpp"
#include "../../include/enjin2/components/tilemap.hpp"
#include "../../include/enjin2/components/camera.hpp"
#include "../../include/enjin2/components/lua_script.hpp"
#include "../../include/enjin2/core/object.hpp"

namespace enjin2 {

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

// tilemap:setTile(tx, ty, tileId) — TMAP-05
static int lua_tilemap_setTile(lua_State* L) {
    CTILEMAP_PROXY_CHECK(L, tm);
    uint8_t tx     = static_cast<uint8_t>(luaL_checkinteger(L, 2));
    uint8_t ty     = static_cast<uint8_t>(luaL_checkinteger(L, 3));
    uint8_t tileId = static_cast<uint8_t>(luaL_checkinteger(L, 4));
    tm->setTile(tx, ty, tileId);
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
// ObjectProxy Metatable Implementation (Phase 37: engine.scene.find() safety)
//==============================================================================

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

void LuaBindings::registerObjectProxyMetatable() {
    lua_State* L = engine->getState();
    if (!L) return;
    // luaL_newmetatable returns 1 if new (creates it), 0 if it already exists
    if (luaL_newmetatable(L, OBJECT_PROXY_METATABLE)) {
        lua_pushcfunction(L, lua_objproxy_index_impl);
        lua_setfield(L, -2, "__index");
        lua_pushcfunction(L, lua_objproxy_newindex_impl);
        lua_setfield(L, -2, "__newindex");
    }
    lua_pop(L, 1);  // always pop — both new and existing cases leave table on stack
}

void LuaBindings::registerComponentProxyMetatable() {
    lua_State* L = engine->getState();
    if (!L) return;

    // Register C_Position_Proxy metatable (proof-of-concept for Phase 39)
    if (luaL_newmetatable(L, CPOSITION_PROXY_METATABLE)) {
        lua_pushcfunction(L, lua_cposition_proxy_index_impl);
        lua_setfield(L, -2, "__index");
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
}

} // namespace enjin2
