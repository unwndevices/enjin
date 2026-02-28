#include "../../include/enjin2/scripting/bindings.hpp"
#include "../../include/enjin2/scripting/component_proxy.hpp"
#include "../../include/enjin2/graphics/defaultfont.hpp"
#include "../../include/enjin2/graphics/text_renderer.hpp"
#include "../../include/enjin2/components/lua_script.hpp"
#include "../../include/enjin2/components/position.hpp"
#include "../../include/enjin2/components/timer.hpp"
#include "../../include/enjin2/components/state_machine.hpp"
#include "../../include/enjin2/components/tilemap.hpp"
#include "../../include/enjin2/core/object.hpp"

namespace enjin2 {

// Global pointer to current bindings instance for static callbacks
static LuaBindings* g_currentBindings = nullptr;

//==============================================================================
// ScriptProxy Metatable Implementation
//==============================================================================

static constexpr const char* PROXY_METATABLE = "ScriptProxy";

// Forward declarations for tag method implementations (defined after __newindex)
static int lua_proxy_addTag_impl(lua_State* L);
static int lua_proxy_hasTag_impl(lua_State* L);
static int lua_proxy_clearTags_impl(lua_State* L);

// Forward declaration for self:get("TypeName") — ComponentProxy dispatch (Phase 39)
static int lua_proxy_get_component_impl(lua_State* L);

// __index metamethod: called when Lua reads self.property
// Stack layout on entry: [1]=userdata(self), [2]=key_string
static int lua_proxy_index_impl(lua_State* L) {
    if (!lua_isuserdata(L, 1)) {
        lua_pushnil(L);
        return 1;
    }
    enjin2::ScriptProxy* proxy = static_cast<enjin2::ScriptProxy*>(lua_touserdata(L, 1));
    if (!proxy) {
        lua_pushnil(L);
        return 1;
    }
    if (!proxy->valid || !proxy->component) {
        luaL_error(L, "object has been destroyed");
        return 0;  // unreachable — luaL_error longjmps
    }

    const char* key = lua_tostring(L, 2);
    if (!key) {
        lua_pushnil(L);
        return 1;
    }

    // PROXY-01: self:get("TypeName") — checked FIRST before any property (PROXY-04b collision prevention)
    if (strcmp(key, "get") == 0) {
        lua_pushcfunction(L, lua_proxy_get_component_impl);
        return 1;
    }

    enjin2::C_LuaScript* comp = proxy->component;
    enjin2::Object* owner = comp->getOwner();

    if (strcmp(key, "x") == 0) {
        enjin2::C_Position* pos = owner ? owner->getPosition() : nullptr;
        lua_pushinteger(L, pos ? static_cast<lua_Integer>(pos->getPosition().x) : 0);
        return 1;
    } else if (strcmp(key, "y") == 0) {
        enjin2::C_Position* pos = owner ? owner->getPosition() : nullptr;
        lua_pushinteger(L, pos ? static_cast<lua_Integer>(pos->getPosition().y) : 0);
        return 1;
    } else if (strcmp(key, "visible") == 0) {
        lua_pushboolean(L, comp->isVisible() ? 1 : 0);
        return 1;
    } else if (strcmp(key, "layer") == 0) {
        // 1-indexed in Lua: buffer_index 0 == layer 1
        lua_pushinteger(L, static_cast<lua_Integer>(comp->GetBufferIndex() + 1));
        return 1;
    } else if (strcmp(key, "active") == 0) {
        lua_pushboolean(L, (owner && owner->isActive()) ? 1 : 0);
        return 1;
    } else if (strcmp(key, "name") == 0) {
        // Phase 29 complete: Object::getName() available
        const char* n = owner ? owner->getName() : nullptr;
        if (n) {
            lua_pushstring(L, n);
        } else {
            lua_pushnil(L);
        }
        return 1;
    } else if (strcmp(key, "addTag") == 0) {
        lua_pushcfunction(L, lua_proxy_addTag_impl);
        return 1;
    } else if (strcmp(key, "hasTag") == 0) {
        lua_pushcfunction(L, lua_proxy_hasTag_impl);
        return 1;
    } else if (strcmp(key, "clearTags") == 0) {
        lua_pushcfunction(L, lua_proxy_clearTags_impl);
        return 1;
    }

    lua_pushnil(L);
    return 1;
}

// __newindex metamethod: called when Lua writes self.property = value
// Stack layout on entry: [1]=userdata(self), [2]=key_string, [3]=value
static int lua_proxy_newindex_impl(lua_State* L) {
    if (!lua_isuserdata(L, 1)) return 0;
    enjin2::ScriptProxy* proxy = static_cast<enjin2::ScriptProxy*>(lua_touserdata(L, 1));
    if (!proxy) return 0;
    if (!proxy->valid || !proxy->component) {
        luaL_error(L, "object has been destroyed");
        return 0;  // unreachable — luaL_error longjmps
    }

    const char* key = lua_tostring(L, 2);
    if (!key) return 0;

    enjin2::C_LuaScript* comp = proxy->component;
    enjin2::Object* owner = comp->getOwner();

    if (strcmp(key, "x") == 0) {
        enjin2::C_Position* pos = owner ? owner->getPosition() : nullptr;
        if (pos) {
            int16_t newX = static_cast<int16_t>(luaL_checkinteger(L, 3));
            pos->setPosition(newX, pos->getPosition().y);
        }
    } else if (strcmp(key, "y") == 0) {
        enjin2::C_Position* pos = owner ? owner->getPosition() : nullptr;
        if (pos) {
            int16_t newY = static_cast<int16_t>(luaL_checkinteger(L, 3));
            pos->setPosition(pos->getPosition().x, newY);
        }
    } else if (strcmp(key, "visible") == 0) {
        comp->SetVisibility(lua_toboolean(L, 3) != 0);
    } else if (strcmp(key, "layer") == 0) {
        // 1-indexed in Lua; convert to 0-indexed C++
        int luaLayer = static_cast<int>(luaL_checkinteger(L, 3));
        if (luaLayer >= 1) {
            comp->SetBufferIndex(static_cast<uint8_t>(luaLayer - 1));
        }
    } else if (strcmp(key, "active") == 0) {
        if (owner) owner->setActive(lua_toboolean(L, 3) != 0);
    }
    // "name" is intentionally read-only — silently ignore writes

    return 0;
}

// lua_proxy_addTag_impl: stack [1]=proxy userdata (self), [2]=tag_string
// Called as self:addTag("enemy") from Lua
static int lua_proxy_addTag_impl(lua_State* L) {
    enjin2::ScriptProxy* proxy = static_cast<enjin2::ScriptProxy*>(
        luaL_checkudata(L, 1, PROXY_METATABLE));
    if (!proxy || !proxy->valid || !proxy->component) {
        luaL_error(L, "object has been destroyed");
        return 0;
    }
    const char* tag = luaL_checkstring(L, 2);
    enjin2::Object* owner = proxy->component->getOwner();
    if (owner) owner->addTag(tag);
    return 0;
}

// lua_proxy_hasTag_impl: stack [1]=proxy, [2]=tag_string -> returns boolean
static int lua_proxy_hasTag_impl(lua_State* L) {
    enjin2::ScriptProxy* proxy = static_cast<enjin2::ScriptProxy*>(
        luaL_checkudata(L, 1, PROXY_METATABLE));
    if (!proxy || !proxy->valid || !proxy->component) {
        luaL_error(L, "object has been destroyed");
        return 0;
    }
    const char* tag = luaL_checkstring(L, 2);
    enjin2::Object* owner = proxy->component->getOwner();
    bool result = owner ? owner->hasTag(tag) : false;
    lua_pushboolean(L, result ? 1 : 0);
    return 1;
}

// lua_proxy_clearTags_impl: stack [1]=proxy -- clears all tags on the owner Object
static int lua_proxy_clearTags_impl(lua_State* L) {
    enjin2::ScriptProxy* proxy = static_cast<enjin2::ScriptProxy*>(
        luaL_checkudata(L, 1, PROXY_METATABLE));
    if (!proxy || !proxy->valid || !proxy->component) {
        luaL_error(L, "object has been destroyed");
        return 0;
    }
    enjin2::Object* owner = proxy->component->getOwner();
    if (owner) owner->clearTags();
    return 0;
}

// lua_proxy_get_component_impl: stack [1]=ScriptProxy userdata (self), [2]=type_name_string
// Called as self:get("C_Position") from Lua — returns ComponentProxy userdata or nil
static int lua_proxy_get_component_impl(lua_State* L) {
    enjin2::ScriptProxy* proxy = static_cast<enjin2::ScriptProxy*>(
        luaL_checkudata(L, 1, PROXY_METATABLE));
    if (!proxy || !proxy->valid || !proxy->component) {
        luaL_error(L, "object has been destroyed");
        return 0;
    }

    const char* typeName = luaL_checkstring(L, 2);
    enjin2::Object* owner = proxy->component->getOwner();
    if (!owner) { lua_pushnil(L); return 1; }

    // Type dispatch — each proxied component type has an entry here.
    // Phase 40 adds: C_Timer. Phase 41 adds: C_StateMachine.
    enjin2::Component* comp = nullptr;
    const char* metaName = nullptr;

    if (strcmp(typeName, "C_Position") == 0) {
        comp = owner->getComponent<enjin2::C_Position>();
        metaName = "C_Position_Proxy";
    } else if (strcmp(typeName, "C_Timer") == 0) {
        comp = owner->getComponent<enjin2::C_Timer>();
        metaName = "C_Timer_Proxy";
    } else if (strcmp(typeName, "C_StateMachine") == 0) {
        comp = owner->getComponent<enjin2::C_StateMachine>();
        metaName = "C_StateMachine_Proxy";
    } else if (strcmp(typeName, "C_Tilemap") == 0) {
        comp = owner->getComponent<enjin2::C_Tilemap>();
        metaName = "C_Tilemap_Proxy";
    }

    if (!comp) { lua_pushnil(L); return 1; }

    // Allocate ComponentProxy userdata with per-type metatable
    auto* cproxy = static_cast<enjin2::ComponentProxy*>(
        lua_newuserdata(L, sizeof(enjin2::ComponentProxy)));
    cproxy->component = comp;
    cproxy->valid = true;
    luaL_getmetatable(L, metaName);
    lua_setmetatable(L, -2);

    // Register proxy with component for destructor invalidation
    // Note: overwrites any previous proxy (single-proxy-per-component constraint — accepted v1.6 limitation)
    comp->setLuaProxy(cproxy);

    return 1;
}

//==============================================================================
// C_Position_Proxy Metatable Implementation (Phase 39: ComponentProxy proof-of-concept)
//==============================================================================

static constexpr const char* CPOSITION_PROXY_METATABLE = "C_Position_Proxy";

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

static constexpr const char* CTIMER_PROXY_METATABLE = "C_Timer_Proxy";

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

static constexpr const char* CFSM_PROXY_METATABLE = "C_StateMachine_Proxy";

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

static constexpr const char* CTILEMAP_PROXY_METATABLE = "C_Tilemap_Proxy";

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
// LuaCanvas Implementation
//==============================================================================

void LuaCanvas::clear(uint8_t color) {
    if (is4Bit) {
        auto* canvas = static_cast<ICanvas<Pixel4>*>(canvasPtr);
        canvas->clear(Pixel4(color));
    } else {
        auto* canvas = static_cast<ICanvas<uint8_t>*>(canvasPtr);
        canvas->clear(color);
    }
}

void LuaCanvas::setPixel(int16_t x, int16_t y, uint8_t color) {
    if (is4Bit) {
        auto* canvas = static_cast<ICanvas<Pixel4>*>(canvasPtr);
        canvas->setPixel(x, y, Pixel4(color));
    } else {
        auto* canvas = static_cast<ICanvas<uint8_t>*>(canvasPtr);
        canvas->setPixel(x, y, color);
    }
}

uint8_t LuaCanvas::getPixel(int16_t x, int16_t y) const {
    if (is4Bit) {
        auto* canvas = static_cast<ICanvas<Pixel4>*>(canvasPtr);
        return canvas->getPixel(x, y).value;
    } else {
        auto* canvas = static_cast<ICanvas<uint8_t>*>(canvasPtr);
        return canvas->getPixel(x, y);
    }
}

void LuaCanvas::drawLine(int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint8_t color) {
    if (is4Bit) {
        auto* canvas = static_cast<ICanvas<Pixel4>*>(canvasPtr);
        Primitives4::drawLine(*canvas, x1, y1, x2, y2, Pixel4(color));
    } else {
        auto* canvas = static_cast<ICanvas<uint8_t>*>(canvasPtr);
        Primitives8::drawLine(*canvas, x1, y1, x2, y2, color);
    }
}

void LuaCanvas::drawRect(int16_t x, int16_t y, uint16_t width, uint16_t height, uint8_t color) {
    Rect rect(x, y, width, height);
    if (is4Bit) {
        auto* canvas = static_cast<ICanvas<Pixel4>*>(canvasPtr);
        Primitives4::drawRect(*canvas, rect, Pixel4(color));
    } else {
        auto* canvas = static_cast<ICanvas<uint8_t>*>(canvasPtr);
        Primitives8::drawRect(*canvas, rect, color);
    }
}

void LuaCanvas::fillRect(int16_t x, int16_t y, uint16_t width, uint16_t height, uint8_t color) {
    Rect rect(x, y, width, height);
    if (is4Bit) {
        auto* canvas = static_cast<ICanvas<Pixel4>*>(canvasPtr);
        Primitives4::fillRect(*canvas, rect, Pixel4(color));
    } else {
        auto* canvas = static_cast<ICanvas<uint8_t>*>(canvasPtr);
        Primitives8::fillRect(*canvas, rect, color);
    }
}

void LuaCanvas::drawCircle(int16_t x, int16_t y, uint16_t radius, uint8_t color) {
    if (is4Bit) {
        auto* canvas = static_cast<ICanvas<Pixel4>*>(canvasPtr);
        Primitives4::drawCircle(*canvas, x, y, radius, Pixel4(color));
    } else {
        auto* canvas = static_cast<ICanvas<uint8_t>*>(canvasPtr);
        Primitives8::drawCircle(*canvas, x, y, radius, color);
    }
}

void LuaCanvas::fillCircle(int16_t x, int16_t y, uint16_t radius, uint8_t color) {
    if (is4Bit) {
        auto* canvas = static_cast<ICanvas<Pixel4>*>(canvasPtr);
        Primitives4::fillCircle(*canvas, x, y, radius, Pixel4(color));
    } else {
        auto* canvas = static_cast<ICanvas<uint8_t>*>(canvasPtr);
        Primitives8::fillCircle(*canvas, x, y, radius, color);
    }
}

void LuaCanvas::drawTriangle(int16_t x1, int16_t y1, int16_t x2, int16_t y2,
                           int16_t x3, int16_t y3, uint8_t color) {
    if (is4Bit) {
        auto* canvas = static_cast<ICanvas<Pixel4>*>(canvasPtr);
        Primitives4::drawTriangle(*canvas, x1, y1, x2, y2, x3, y3, Pixel4(color));
    } else {
        auto* canvas = static_cast<ICanvas<uint8_t>*>(canvasPtr);
        Primitives8::drawTriangle(*canvas, x1, y1, x2, y2, x3, y3, color);
    }
}

void LuaCanvas::fillTriangle(int16_t x1, int16_t y1, int16_t x2, int16_t y2,
                           int16_t x3, int16_t y3, uint8_t color) {
    if (is4Bit) {
        auto* canvas = static_cast<ICanvas<Pixel4>*>(canvasPtr);
        Primitives4::fillTriangle(*canvas, x1, y1, x2, y2, x3, y3, Pixel4(color));
    } else {
        auto* canvas = static_cast<ICanvas<uint8_t>*>(canvasPtr);
        Primitives8::fillTriangle(*canvas, x1, y1, x2, y2, x3, y3, color);
    }
}

void LuaCanvas::drawText(const char* str, int16_t x, int16_t y,
                         uint8_t color, uint8_t size, const ::GFXfont* font) {
    if (!str) return;
    const auto* gfxFont = reinterpret_cast<const enjin2::GFXfont*>(font);
    if (is4Bit) {
        auto* canvas = static_cast<ICanvas<Pixel4>*>(canvasPtr);
        TextRenderer<Pixel4> tr;
        tr.setFont(gfxFont);
        tr.setTextColor(Pixel4(color));
        tr.setTextSize(size);
        tr.drawString(*canvas, x, y, str);
    } else {
        auto* canvas = static_cast<ICanvas<uint8_t>*>(canvasPtr);
        TextRenderer<uint8_t> tr;
        tr.setFont(gfxFont);
        tr.setTextColor(color);
        tr.setTextSize(size);
        tr.drawString(*canvas, x, y, str);
    }
}

void LuaCanvas::drawTextWrapped(const char* str, int16_t x, int16_t y,
                               uint16_t maxWidth, uint8_t color, uint8_t size,
                               const ::GFXfont* font) {
    if (!str) return;
    const auto* gfxFont = reinterpret_cast<const enjin2::GFXfont*>(font);
    if (is4Bit) {
        auto* canvas = static_cast<ICanvas<Pixel4>*>(canvasPtr);
        TextRenderer<Pixel4> tr;
        tr.setFont(gfxFont);
        tr.setTextColor(Pixel4(color));
        tr.setTextSize(size);
        tr.drawStringWrapped(*canvas, x, y, maxWidth, str);
    } else {
        auto* canvas = static_cast<ICanvas<uint8_t>*>(canvasPtr);
        TextRenderer<uint8_t> tr;
        tr.setFont(gfxFont);
        tr.setTextColor(color);
        tr.setTextSize(size);
        tr.drawStringWrapped(*canvas, x, y, maxWidth, str);
    }
}

uint16_t LuaCanvas::measureTextWidth(const char* str, uint8_t size, const ::GFXfont* font) {
    if (!str) return 0;
    const auto* gfxFont = reinterpret_cast<const enjin2::GFXfont*>(font);
    if (is4Bit) {
        TextRenderer<Pixel4> tr;
        tr.setFont(gfxFont);
        tr.setTextSize(size);
        return tr.getTextWidth(str);
    } else {
        TextRenderer<uint8_t> tr;
        tr.setFont(gfxFont);
        tr.setTextSize(size);
        return tr.getTextWidth(str);
    }
}

uint8_t LuaCanvas::measureTextHeight(uint8_t size, const ::GFXfont* font) {
    const auto* gfxFont = reinterpret_cast<const enjin2::GFXfont*>(font);
    if (is4Bit) {
        TextRenderer<Pixel4> tr;
        tr.setFont(gfxFont);
        tr.setTextSize(size);
        return tr.getCharHeight() * size;
    } else {
        TextRenderer<uint8_t> tr;
        tr.setFont(gfxFont);
        tr.setTextSize(size);
        return tr.getCharHeight() * size;
    }
}

//==============================================================================
// LuaBindings Implementation
//==============================================================================

LuaBindings::LuaBindings(LuaEngine* luaEngine)
    : engine(luaEngine), currentCanvas(nullptr), currentInput(nullptr), currentColor(15), lineWidth(1) {
    g_currentBindings = this;
}

void LuaBindings::registerAll() {
    if (!engine || !engine->isInitialized()) {
        return;
    }

    // Reset all per-reload state so every load starts clean
    resetSpritePool();
    currentColor = 15;
    lineWidth = 1;

    // Store bindings instance in Lua registry
    lua_State* L = engine->getState();
    lua_pushlightuserdata(L, this);
    lua_setfield(L, LUA_REGISTRYINDEX, "enjin_bindings");

    // Store injected engine.* pointers in registry for closure retrieval
    lua_pushlightuserdata(L, &m_ssm);          // store address-of-member (pointer-to-pointer)
    lua_setfield(L, LUA_REGISTRYINDEX, "enjin_ssm");
    lua_pushlightuserdata(L, &m_activeScene);  // store address-of-member (pointer-to-pointer)
    lua_setfield(L, LUA_REGISTRYINDEX, "enjin_active_scene");
    lua_pushlightuserdata(L, &m_timeState);   // always valid (member of LuaBindings)
    lua_setfield(L, LUA_REGISTRYINDEX, "enjin_time");

    // EVENT-05: clear event bus handlers from previous load (hot-reload cleanup)
    m_eventBus.clearHandlers();
    m_eventBus.setLuaState(L);

    // Store event bus pointer in registry for engine.event.* closures
    lua_pushlightuserdata(L, &m_eventBus);
    lua_setfield(L, LUA_REGISTRYINDEX, "enjin_event_bus");

    // Canvas functions
    engine->registerFunction("getWidth", lua_getWidth);
    engine->registerFunction("getHeight", lua_getHeight);
    engine->registerFunction("clear", lua_clear);

    // Drawing state functions (love2d.graphics style)
    engine->registerFunction("setColor", lua_setColor);
    engine->registerFunction("getColor", lua_getColor);
    engine->registerFunction("setLineWidth", lua_setLineWidth);
    engine->registerFunction("getLineWidth", lua_getLineWidth);

    // Drawing primitives
    engine->registerFunction("point", lua_point);
    engine->registerFunction("line", lua_line);
    engine->registerFunction("rectangle", lua_rectangle);
    engine->registerFunction("circle", lua_circle);
    engine->registerFunction("triangle", lua_triangle);

    // Pixel access
    engine->registerFunction("setPixel", lua_setPixel);
    engine->registerFunction("getPixel", lua_getPixel);

    // Utility functions
    engine->registerFunction("print", lua_print);

    // High-performance optimized drawing functions
    engine->registerFunction("fastFillRect", lua_fastFillRect);
    engine->registerFunction("fastDrawLine", lua_fastDrawLine);

    // Palette
    engine->registerFunction("setPaletteColor", lua_setPaletteColor);
    engine->registerFunction("getPaletteColor", lua_getPaletteColor);
    engine->registerFunction("loadPalette", lua_loadPalette);
    engine->registerFunction("getPaletteSize", lua_getPaletteSize);

    // Input polling (INP-05)
    engine->registerFunction("isButtonHeld",        lua_isButtonHeld);
    engine->registerFunction("isButtonJustPressed",  lua_isButtonJustPressed);
    engine->registerFunction("isButtonJustReleased", lua_isButtonJustReleased);
    engine->registerFunction("getAxis",              lua_getAxis);

    // Sprite pool (SPR-06)
    engine->registerFunction("newSprite",    lua_newSprite);
    engine->registerFunction("freeSprite",   lua_freeSprite);
    engine->registerFunction("drawSprite",   lua_drawSprite);
    engine->registerFunction("updateSprite", lua_updateSprite);
    engine->registerFunction("setFrame",     lua_setFrame);

    // Layer system (LAYER-06)
    engine->registerFunction("setLayer",        lua_setLayer);
    engine->registerFunction("getLayer",        lua_getLayer);
    engine->registerFunction("clearLayer",      lua_clearLayer);
    engine->registerFunction("getLayerCount",   lua_getLayerCount);
    engine->registerFunction("setLayerVisible", lua_setLayerVisible);
    engine->registerFunction("isLayerVisible",  lua_isLayerVisible);

    // Text
    engine->registerFunction("text",            lua_text);
    engine->registerFunction("textWrapped",     lua_textWrapped);
    engine->registerFunction("setTextSize",    lua_setTextSize);
    engine->registerFunction("getTextSize",     lua_getTextSize);
    engine->registerFunction("setFont",        lua_setFont);
    engine->registerFunction("getFont",        lua_getFont);
    engine->registerFunction("getTextWidth",    lua_getTextWidth);
    engine->registerFunction("getTextHeight",  lua_getTextHeight);

    // Pre-register built-in 8pt font so setFont("default8") works
    registerFont("default8", &defaultFont8pt7b);

    // Layer global constants (Lua 1-indexed)
    lua_pushinteger(L, 1); lua_setglobal(L, "LAYER_BG");
    lua_pushinteger(L, 2); lua_setglobal(L, "LAYER_MID");
    lua_pushinteger(L, 3); lua_setglobal(L, "LAYER_FG");
    lua_pushinteger(L, 4); lua_setglobal(L, "LAYER_UI");

    // Create love2d.graphics-style table for familiarity
    lua_newtable(L);
    lua_pushcfunction(L, lua_rectangle);
    lua_setfield(L, -2, "draw");
    lua_setglobal(L, "love");

    // Create graphics subtable properly
    lua_getglobal(L, "love");
    if (lua_istable(L, -1)) {
        lua_newtable(L);  // Create graphics table

        // Add functions to graphics table
        lua_pushcfunction(L, lua_setColor);
        lua_setfield(L, -2, "setColor");
        lua_pushcfunction(L, lua_getColor);
        lua_setfield(L, -2, "getColor");
        lua_pushcfunction(L, lua_setLineWidth);
        lua_setfield(L, -2, "setLineWidth");
        lua_pushcfunction(L, lua_getLineWidth);
        lua_setfield(L, -2, "getLineWidth");
        lua_pushcfunction(L, lua_point);
        lua_setfield(L, -2, "point");
        lua_pushcfunction(L, lua_line);
        lua_setfield(L, -2, "line");
        lua_pushcfunction(L, lua_rectangle);
        lua_setfield(L, -2, "rectangle");
        lua_pushcfunction(L, lua_circle);
        lua_setfield(L, -2, "circle");
        lua_pushcfunction(L, lua_clear);
        lua_setfield(L, -2, "clear");

        // Set graphics table as love.graphics
        lua_setfield(L, -2, "graphics");
    }
    lua_pop(L, 1);

    // Register engine.* global table (ENG-06: must be before any script loads)
    registerEngineTable();

    // Register ScriptProxy metatable for C_LuaScript component path
    registerProxyMetatable();

    // Register ObjectProxy metatable for engine.scene.find() return value (Phase 37)
    registerObjectProxyMetatable();

    // Register ComponentProxy metatables for self:get() return values (Phase 39)
    registerComponentProxyMetatable();

    // Register Vec2/Point/Rect userdata metatables and math utility globals
    registerMathBindings();
}

void LuaBindings::setCanvas(LuaCanvas* canvas) {
    currentCanvas = canvas;
}

void LuaBindings::setInput(InputState* input) {
    currentInput = input;
}

void LuaBindings::setLayers(LuaCanvas** canvases, uint8_t count, bool* visibleArr) {
    layerCount = (count > MAX_LUA_LAYERS) ? static_cast<uint8_t>(MAX_LUA_LAYERS) : count;
    for (uint8_t i = 0; i < layerCount; ++i) {
        layerCanvases[i] = canvases[i];
    }
    layerVisible = visibleArr;
    activeLayer = 0;
    currentCanvas = layerCanvases[0];
}

void LuaBindings::resetSpritePool() {
    for (int i = 0; i < LUA_SPRITE_POOL_SIZE; ++i) {
        spritePool[i] = SpriteState{};
        loadedAssets_[i] = SpriteAsset{};
    }
    assetBufferUsed_ = 0;
    currentTextSize = 1;
    currentFont = nullptr;
    strncpy(currentFontName, "default", 31);
    currentFontName[31] = '\0';
    m_rngState = 0x12345678;  // reset RNG on reload
}

LuaBindings* LuaBindings::getBindings(lua_State* L) {
    lua_getfield(L, LUA_REGISTRYINDEX, "enjin_bindings");
    LuaBindings* bindings = static_cast<LuaBindings*>(lua_touserdata(L, -1));
    lua_pop(L, 1);
    return bindings;
}

const enjin2::SpriteSheet* LuaBindings::getSpriteSheet(int handle) const {
    if (handle < 0 || handle >= LUA_SPRITE_POOL_SIZE || !spritePool[handle].active)
        return nullptr;
    return &spritePool[handle].sheet;
}

void LuaBindings::registerProxyMetatable() {
    lua_State* L = engine->getState();
    if (!L) return;
    // luaL_newmetatable returns 1 if new (creates it), 0 if it already exists
    if (luaL_newmetatable(L, PROXY_METATABLE)) {
        lua_pushcfunction(L, lua_proxy_index_impl);
        lua_setfield(L, -2, "__index");
        lua_pushcfunction(L, lua_proxy_newindex_impl);
        lua_setfield(L, -2, "__newindex");
    }
    lua_pop(L, 1);  // always pop — both new and existing cases leave table on stack
}

//==============================================================================
// ObjectProxy Metatable Implementation (Phase 37: engine.scene.find() safety)
//==============================================================================

static constexpr const char* OBJECT_PROXY_METATABLE = "ObjectProxy";

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
}

void LuaBindings::setActiveScene(Scene* scene) {
    if (scene != m_activeScene) {
        // EVENT-04: Scene is changing -- clear event bus for the outgoing scene.
        // This ensures no stale handlers carry over to the new scene.
        m_eventBus.clearHandlers();
    }
    m_activeScene = scene;
}

} // namespace enjin2
