// Lua API descriptors: registration and the per-VM registry (Tomodachi #257).
#include "../../include/enjin2/scripting/lua_api.hpp"
#include "../../include/enjin2/scripting/lua_api_stdlib.hpp"

#include <cstring>
#include <set>

namespace enjin2 {

namespace {

// Registry key of the table recording registered modules: an array of
// lightuserdata (registration order) plus a lightuserdata -> true set.
constexpr const char* kApiRegistryKey = "enjin.api";

void recordModule(lua_State* L, const LuaApiModule& module) {
    lua_getfield(L, LUA_REGISTRYINDEX, kApiRegistryKey);
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_pushvalue(L, -1);
        lua_setfield(L, LUA_REGISTRYINDEX, kApiRegistryKey);
    }
    const void* key = &module;
    lua_pushlightuserdata(L, const_cast<void*>(key));
    lua_rawget(L, -2);
    const bool seen = lua_toboolean(L, -1) != 0;
    lua_pop(L, 1);
    if (!seen) {
        lua_pushlightuserdata(L, const_cast<void*>(key));
        lua_pushboolean(L, 1);
        lua_rawset(L, -3);
        lua_pushlightuserdata(L, const_cast<void*>(key));
        lua_rawseti(L, -2, static_cast<lua_Integer>(lua_rawlen(L, -2)) + 1);
    }
    lua_pop(L, 1);
}

void pushTable(lua_State* L, const LuaApiModule& module);

// Push one entry's value; false for entries that register nothing.
bool pushEntry(lua_State* L, const LuaApiEntry& e) {
    switch (e.kind) {
        case LuaApiKind::Function:
            lua_pushcfunction(L, e.func);
            return true;
        case LuaApiKind::Constant:
            lua_pushinteger(L, e.value);
            return true;
        case LuaApiKind::Table:
            pushTable(L, *e.table);
            return true;
        case LuaApiKind::Lifecycle:
            break;
    }
    return false;
}

void setEntries(lua_State* L, int tableIdx, const LuaApiModule& module) {
    for (size_t i = 0; i < module.count; ++i) {
        const LuaApiEntry& e = module.entries[i];
        if (pushEntry(L, e)) lua_setfield(L, tableIdx, e.name);
    }
}

void pushTable(lua_State* L, const LuaApiModule& module) {
    lua_createtable(L, 0, static_cast<int>(module.count));
    setEntries(L, lua_gettop(L), module);
}

const char* leafName(const char* path) {
    const char* dot = std::strrchr(path, '.');
    return dot ? dot + 1 : path;
}

} // namespace

void luaApiSetFields(lua_State* L, int tableIdx, const LuaApiModule& module) {
    setEntries(L, lua_absindex(L, tableIdx), module);
    recordModule(L, module);
}

void luaApiSetSubtable(lua_State* L, int parentIdx, const LuaApiModule& module) {
    parentIdx = lua_absindex(L, parentIdx);
    pushTable(L, module);
    lua_setfield(L, parentIdx, leafName(module.path));
    recordModule(L, module);
}

void luaApiSetGlobalTable(lua_State* L, const LuaApiModule& module) {
    pushTable(L, module);
    lua_setglobal(L, module.path);
    recordModule(L, module);
}

void luaApiSetGlobals(lua_State* L, const LuaApiModule& module) {
    for (size_t i = 0; i < module.count; ++i) {
        const LuaApiEntry& e = module.entries[i];
        if (pushEntry(L, e)) lua_setglobal(L, e.name);
    }
    recordModule(L, module);
}

const char* luaApiKindName(LuaApiKind kind) {
    switch (kind) {
        case LuaApiKind::Function:  return "function";
        case LuaApiKind::Constant:  return "constant";
        case LuaApiKind::Table:     return "table";
        case LuaApiKind::Lifecycle: return "lifecycle";
    }
    return "function";
}

const char* luaApiScopeName(LuaApiScope scope) {
    switch (scope) {
        case LuaApiScope::Table:     return "table";
        case LuaApiScope::Globals:   return "globals";
        case LuaApiScope::Methods:   return "methods";
        case LuaApiScope::Metatable: return "metatable";
    }
    return "table";
}

std::vector<const LuaApiModule*> luaApiModules(lua_State* L) {
    std::vector<const LuaApiModule*> modules;
    lua_getfield(L, LUA_REGISTRYINDEX, kApiRegistryKey);
    if (lua_istable(L, -1)) {
        const lua_Integer n = static_cast<lua_Integer>(lua_rawlen(L, -1));
        modules.reserve(static_cast<size_t>(n));
        for (lua_Integer i = 1; i <= n; ++i) {
            lua_rawgeti(L, -1, i);
            modules.push_back(static_cast<const LuaApiModule*>(lua_touserdata(L, -1)));
            lua_pop(L, 1);
        }
    }
    lua_pop(L, 1);
    return modules;
}

namespace {

// The table at (scope, path) in a view, or nullptr; const or not with the view.
template <typename View>
auto findView(View& view, LuaApiScope scope, const std::string& path) -> decltype(&view[0]) {
    for (auto& t : view)
        if (t.scope == scope && t.path == path) return &t;
    return nullptr;
}

// "path.name", or "name" at the top level.
std::string joinPath(const std::string& path, const std::string& name) {
    return path.empty() ? name : path + "." + name;
}

void addToView(std::vector<LuaApiTableView>& view, const LuaApiModule& module) {
    LuaApiTableView* table = findView(view, module.scope, module.path);
    if (!table) {
        view.push_back(LuaApiTableView{module.scope, module.path, module.summary, {}});
        table = &view.back();
    } else if (!table->summary) {
        table->summary = module.summary;
    }
    for (size_t i = 0; i < module.count; ++i) {
        const LuaApiEntry* e = &module.entries[i];
        bool replaced = false;
        for (auto& existing : table->entries) {
            if (std::strcmp(existing->name, e->name) == 0) {
                existing = e;
                replaced = true;
                break;
            }
        }
        if (!replaced) table->entries.push_back(e);
    }
    // `table` may dangle once the recursion grows `view`.
    for (size_t i = 0; i < module.count; ++i) {
        if (module.entries[i].kind == LuaApiKind::Table) addToView(view, *module.entries[i].table);
    }
}

} // namespace

std::vector<LuaApiTableView> luaApiView(lua_State* L) {
    std::vector<LuaApiTableView> view;
    for (const LuaApiModule* module : luaApiModules(L)) addToView(view, *module);
    return view;
}

namespace {

struct Census {
    lua_State*                          L;
    const std::vector<LuaApiTableView>& view;
    std::vector<std::string>&           problems;
    std::set<const void*>               walked;
};

const LuaApiEntry* findEntry(const LuaApiTableView* table, const char* name) {
    if (!table) return nullptr;
    for (const LuaApiEntry* e : table->entries)
        if (std::strcmp(e->name, name) == 0) return e;
    return nullptr;
}

const LuaStdlibEntry* findStdlib(const std::string& name) {
    for (const auto& e : kLuaStdlib)
        if (name == e.name) return &e;
    return nullptr;
}

// Push the value at dotted `path` from _G ("" = _G itself), or nil.
void pushPath(lua_State* L, const std::string& path) {
    lua_pushglobaltable(L);
    size_t start = 0;
    while (start < path.size() && !lua_isnil(L, -1)) {
        size_t dot = path.find('.', start);
        if (dot == std::string::npos) dot = path.size();
        if (!lua_istable(L, -1)) {
            lua_pop(L, 1);
            lua_pushnil(L);
            break;
        }
        lua_getfield(L, -1, path.substr(start, dot - start).c_str());
        lua_remove(L, -2);
        start = dot + 1;
    }
}

void walkTable(Census& c, const std::string& path);

// The value at the top of the stack is `full`, described by `e`.
void checkDescribed(Census& c, const LuaApiEntry& e, const std::string& full) {
    lua_State* L = c.L;
    switch (e.kind) {
        case LuaApiKind::Function:
            if (lua_tocfunction(L, -1) != e.func)
                c.problems.push_back(full + " is not the function its descriptor registers");
            break;
        case LuaApiKind::Constant:
            if (!lua_isinteger(L, -1) || lua_tointeger(L, -1) != e.value)
                c.problems.push_back(full + " is not the constant its descriptor registers");
            break;
        case LuaApiKind::Table:
            if (lua_istable(L, -1)) walkTable(c, e.table->path);
            else c.problems.push_back(full + " is not the table its descriptor registers");
            break;
        case LuaApiKind::Lifecycle:
            break;  // the applet's own callback
    }
}

// Every key of the table at the top of the stack, reached as `path`.
void walkTable(Census& c, const std::string& path) {
    lua_State* L = c.L;
    if (!c.walked.insert(lua_topointer(L, -1)).second) return;
    const LuaApiTableView* here = path.empty() ? findView(c.view, LuaApiScope::Globals, "")
                                               : findView(c.view, LuaApiScope::Table, path);
    const int t = lua_gettop(L);
    lua_pushnil(L);
    while (lua_next(L, t) != 0) {
        if (lua_type(L, -2) != LUA_TSTRING) {
            c.problems.push_back((path.empty() ? "_G" : path) + " has a " +
                                 luaL_typename(L, -2) + " key with no descriptor");
            lua_pop(L, 1);
            continue;
        }
        const char* key = lua_tostring(L, -2);
        const std::string full = joinPath(path, key);
        if (const LuaApiEntry* e = findEntry(here, key)) {
            checkDescribed(c, *e, full);
        } else if (lua_istable(L, -1) &&
                   (findView(c.view, LuaApiScope::Table, full) || findStdlib(full))) {
            walkTable(c, full);  // a module registered at this path, or a stdlib library
        } else if (!findStdlib(full)) {
            c.problems.push_back(full + " has no descriptor");
        }
        lua_pop(L, 1);
    }
}

} // namespace

std::vector<std::string> luaApiCensus(lua_State* L) {
    std::vector<std::string> problems;
    const std::vector<LuaApiTableView> view = luaApiView(L);
    const int top = lua_gettop(L);

    Census census{L, view, problems, {}};
    lua_pushglobaltable(L);
    walkTable(census, "");
    lua_settop(L, top);

    // The other way: every described or listed name is there to call.
    for (const auto& t : view) {
        if (t.scope != LuaApiScope::Table && t.scope != LuaApiScope::Globals) continue;
        pushPath(L, t.path);
        if (!lua_istable(L, -1)) {
            problems.push_back(t.path + " is described but not reachable from _G");
        } else {
            for (const LuaApiEntry* e : t.entries) {
                if (e->kind == LuaApiKind::Lifecycle) continue;
                lua_getfield(L, -1, e->name);
                if (lua_isnil(L, -1))
                    problems.push_back(joinPath(t.path, e->name) +
                                       " is described but not registered");
                lua_pop(L, 1);
            }
        }
        lua_settop(L, top);
    }
    for (const auto& e : kLuaStdlib) {
        pushPath(L, e.name);
        if (lua_isnil(L, -1))
            problems.push_back(std::string("stdlib ") + e.name + " is listed but not reachable");
        lua_settop(L, top);
    }

    for (const LuaApiModule* m : luaApiModules(L)) {
        std::string errors;
        if (!validateLuaApiModule(*m, &errors))
            problems.push_back(std::string("module '") + m->path + "' fails validation:\n" + errors);
    }
    return problems;
}

} // namespace enjin2
