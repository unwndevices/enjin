/**
 * @file lua_api_test.cpp
 * @brief Lua API descriptors (Tomodachi #257, ADR-0013)
 *
 * The signature grammar round-trips and rejects malformed strings; a module
 * registered from a descriptor array exposes exactly its entries; the API
 * registry lists what a VM registered; and every enjin core module that
 * registerAll() installs passes validation.
 */
#include <enjin2/scripting/bindings.hpp>
#include <enjin2/scripting/lua_api.hpp>
#include <enjin2/scripting/lua_engine.hpp>
#include <enjin2/scripting/tilemap_lua.hpp>

#include <cstdio>
#include <cstring>
#include <set>
#include <string>

using namespace enjin2;

static int passes = 0;
static int failures = 0;

#define ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            fprintf(stderr, "FAIL [line %d]: %s\n", __LINE__, std::string(msg).c_str()); \
            failures++; \
        } else { \
            passes++; \
        } \
    } while (0)

//==============================================================================
// Signature grammar
//==============================================================================

static void roundTrips(const char* text) {
    LuaApiSignature sig;
    std::string err;
    const bool ok = parseLuaApiSignature(text, sig, &err);
    ASSERT(ok, std::string("parses: ") + text + " (" + err + ")");
    if (!ok) return;
    const std::string back = formatLuaApiSignature(sig);
    ASSERT(back == text, std::string("round-trips: ") + text + " -> " + back);
}

static void rejects(const char* text) {
    LuaApiSignature sig;
    std::string err;
    const bool ok = parseLuaApiSignature(text, sig, &err);
    ASSERT(!ok, std::string("rejects: ") + text);
    ASSERT(ok || !err.empty(), std::string("explains rejecting: ") + text);
}

static void testRoundTrip() {
    roundTrips("() -> nil");
    roundTrips("() -> int");
    roundTrips("(layer:int, x:int, y:int, color:int?=current) -> nil");
    roundTrips("(color:int?) -> nil");
    roundTrips("(mode:string?=\"fill\") -> nil");
    roundTrips("(x:number?=-1.5, on:boolean?=false) -> nil");
    roundTrips("(v:int|string) -> nil");
    roundTrips("(...) -> nil");
    roundTrips("(fmt:string, ...:any) -> string");
    roundTrips("(index:int) -> r:int, g:int, b:int");
    roundTrips("(target:table, key:string, goal:number, params:table?) -> id:int?");
    roundTrips("(fn:function) -> int?");
    roundTrips("() -> ...:any");
    roundTrips("(a:Vec2, b:Vec2) -> Vec2");
    roundTrips("(index:int, hex:string) -> nil\n(index:int, r:int, g:int, b:int) -> nil");
}

static void testStructure() {
    LuaApiSignature sig;
    ASSERT(parseLuaApiSignature("(x:int, c:int?=current, ...:any) -> r:int?, string", sig),
           "structure parses");
    ASSERT(sig.overloads.size() == 1, "one overload");
    const auto& o = sig.overloads[0];
    ASSERT(o.params.size() == 3, "three params");
    ASSERT(o.params[0].name == "x" && o.params[0].type == "int" && !o.params[0].optional,
           "required param");
    ASSERT(o.params[1].optional && o.params[1].defaultValue == "current", "defaulted param");
    ASSERT(o.params[2].name == "..." && o.params[2].type == "any", "variadic param");
    ASSERT(o.results.size() == 2, "two results");
    ASSERT(o.results[0].name == "r" && o.results[0].optional, "named optional result");
    ASSERT(o.results[1].name.empty() && o.results[1].type == "string", "unnamed result");

    ASSERT(parseLuaApiSignature("() -> nil", sig) && sig.overloads[0].results.empty(),
           "nil means no results");
}

static void testWhitespaceNormalises() {
    LuaApiSignature sig;
    ASSERT(parseLuaApiSignature("( x : int ,y:int ? = 0 )->nil", sig), "loose spacing parses");
    ASSERT(formatLuaApiSignature(sig) == "(x:int, y:int?=0) -> nil", "formats canonically");
}

static void testMalformed() {
    rejects("");
    rejects("x:int -> nil");                  // no parens
    rejects("(x:int)");                       // no returns
    rejects("(x:int) ->");                    // empty returns
    rejects("(x:int) nil");                   // no arrow
    rejects("(x) -> nil");                    // untyped param
    rejects("(x:) -> nil");                   // empty type
    rejects("(x:int=3) -> nil");              // default without ?
    rejects("(x:int?=) -> nil");              // empty default
    rejects("(x:int?, y:int) -> nil");        // required after optional
    rejects("(x:int, x:int) -> nil");         // duplicate name
    rejects("(..., x:int) -> nil");           // variadic not last
    rejects("(x:int,) -> nil");               // trailing comma
    rejects("(x:int y:int) -> nil");          // missing comma
    rejects("(1x:int) -> nil");               // bad identifier
    rejects("(x:int|) -> nil");               // dangling union
    rejects("(x:int) -> nil trailing");       // trailing garbage
    rejects("(x:int) -> nil, int");           // nil inside a list
    rejects("(s:string?=\"open) -> nil");     // unterminated string
    rejects("() -> ...:any, int");            // variadic result not last
    rejects("() -> nil\n");                   // empty overload
    rejects("(x:int -> nil");                 // unclosed paren
}

//==============================================================================
// Registration
//==============================================================================

static int fnA(lua_State* L) { lua_pushinteger(L, 1); return 1; }
static int fnB(lua_State* L) { lua_pushinteger(L, 2); return 1; }

static constexpr const char* kFruitNames[] = {"apple", "pear"};
static constexpr LuaApiEnum kFruit = luaApiEnum("Fruit", kFruitNames);

static constexpr LuaApiEntry kInner[] = {
    luaConstant("DEEP", 42, "A nested constant."),
};
static constexpr LuaApiModule kInnerModule =
    luaApiModule(LuaApiScope::Table, "demo.INNER", "Nested.", kInner);

static constexpr LuaApiEntry kDemo[] = {
    luaFunction("a", fnA, "() -> int", "Returns 1."),
    luaFunction("b", fnB, "(fruit:Fruit?=apple) -> int", "Returns 2.",
                "fruit: which fruit").withEnum(kFruit).note("Host note."),
    luaConstant("K", 7, "Seven."),
    luaTable("INNER", kInnerModule, "Nested table."),
    luaLifecycle("update", "(dt:number) -> nil", "Called every frame.", "dt: seconds"),
};
static constexpr LuaApiModule kDemoModule =
    luaApiModule(LuaApiScope::Table, "demo", "A demo module.", kDemo);

static constexpr LuaApiEntry kGlobalsEntries[] = {
    luaFunction("demoGlobal", fnB, "() -> int", "A bare global."),
};
static constexpr LuaApiModule kGlobalsModule =
    luaApiModule(LuaApiScope::Globals, "", "Demo globals.", kGlobalsEntries);

static std::set<std::string> tableKeys(lua_State* L, int idx) {
    std::set<std::string> keys;
    if (idx < 0) idx = lua_gettop(L) + idx + 1;
    lua_pushnil(L);
    while (lua_next(L, idx) != 0) {
        if (lua_type(L, -2) == LUA_TSTRING) keys.insert(lua_tostring(L, -2));
        lua_pop(L, 1);
    }
    return keys;
}

static lua_Integer evalInt(lua_State* L, const char* expr) {
    const std::string code = std::string("return ") + expr;
    if (luaL_dostring(L, code.c_str()) != LUA_OK) {
        fprintf(stderr, "  lua error: %s\n", lua_tostring(L, -1));
        lua_pop(L, 1);
        return -999;
    }
    const lua_Integer v = lua_tointeger(L, -1);
    lua_pop(L, 1);
    return v;
}

static void testModuleExposesExactlyItsEntries() {
    lua_State* L = luaL_newstate();
    luaL_openlibs(L);

    luaApiSetGlobalTable(L, kDemoModule);
    lua_getglobal(L, "demo");
    ASSERT(lua_istable(L, -1), "demo is a global table");
    const std::set<std::string> keys = tableKeys(L, -1);
    ASSERT((keys == std::set<std::string>{"a", "b", "K", "INNER"}),
           "demo has exactly a, b, K, INNER (the lifecycle entry is not registered)");
    lua_getfield(L, -1, "INNER");
    ASSERT((tableKeys(L, -1) == std::set<std::string>{"DEEP"}), "INNER has exactly DEEP");
    lua_pop(L, 2);

    ASSERT(evalInt(L, "demo.a()") == 1, "demo.a is fnA");
    ASSERT(evalInt(L, "demo.b()") == 2, "demo.b is fnB");
    ASSERT(evalInt(L, "demo.K") == 7, "demo.K is 7");
    ASSERT(evalInt(L, "demo.INNER.DEEP") == 42, "demo.INNER.DEEP is 42");
    ASSERT(evalInt(L, "update == nil and 1 or 0") == 1, "lifecycle name left undefined");

    // Subtable: parent[leaf of path].
    lua_newtable(L);
    static constexpr LuaApiModule kSub =
        luaApiModule(LuaApiScope::Table, "engine.demo", "Sub.", kInner);
    luaApiSetSubtable(L, -1, kSub);
    ASSERT((tableKeys(L, -1) == std::set<std::string>{"demo"}), "subtable stored under its leaf");
    lua_pop(L, 1);

    luaApiSetGlobals(L, kGlobalsModule);
    ASSERT(evalInt(L, "demoGlobal()") == 2, "Globals scope registers bare globals");

    // Registry lists what was registered, in order, once each.
    luaApiSetGlobals(L, kGlobalsModule);
    const auto modules = luaApiModules(L);
    ASSERT(modules.size() == 3, "registry lists three modules");
    ASSERT(modules.size() == 3 && modules[0] == &kDemoModule && modules[1] == &kSub &&
               modules[2] == &kGlobalsModule,
           "registry keeps registration order and ignores repeats");

    lua_close(L);
}

static void testRegistryIsPerVm() {
    lua_State* L = luaL_newstate();
    ASSERT(luaApiModules(L).empty(), "a fresh VM has registered nothing");
    lua_close(L);
}

//==============================================================================
// Validation
//==============================================================================

static void testValidateCatchesDrift() {
    std::string errors;
    ASSERT(validateLuaApiModule(kDemoModule, &errors), "demo module validates: " + errors);

    static constexpr LuaApiEntry kBad[] = {
        luaFunction("wrongArgs", fnA, "(x:int, y:int) -> nil", "Arg lines drift.",
                    "x: first\nz: not a parameter"),
        luaFunction("badSig", fnA, "(x:int", "Unparseable.", "x: first"),
        luaFunction("badType", fnA, "(x:integer) -> nil", "Lowercase non-builtin.",
                    "x: first"),
        luaFunction("noArgs", fnA, "(x:int) -> nil", "Missing arg lines."),
        luaFunction("", fnA, "() -> nil", "Empty name."),
        luaFunction("noSummary", fnA, "() -> nil", ""),
        luaFunction("unusedEnum", fnA, "(f:Fruity) -> nil", "Enum only matches a longer word.",
                    "f: a Fruity").withEnum(kFruit),
    };
    static constexpr LuaApiModule kBadModule =
        luaApiModule(LuaApiScope::Table, "bad", "Bad.", kBad);
    errors.clear();
    ASSERT(!validateLuaApiModule(kBadModule, &errors), "bad module fails validation");
    ASSERT(errors.find("wrongArgs") != std::string::npos, "reports drifted arg lines");
    ASSERT(errors.find("badSig") != std::string::npos, "reports an unparseable signature");
    ASSERT(errors.find("badType") != std::string::npos, "reports an unknown lowercase type");
    ASSERT(errors.find("noArgs") != std::string::npos, "reports missing arg lines");
    ASSERT(errors.find("noSummary") != std::string::npos, "reports a missing summary");
    ASSERT(errors.find("unusedEnum") != std::string::npos,
           "an enum counts as referenced only as a whole word");
}

//==============================================================================
// enjin core: every module registerAll() installs is described and valid
//==============================================================================

static const char* scopeName(LuaApiScope s) {
    switch (s) {
        case LuaApiScope::Table:     return "table";
        case LuaApiScope::Globals:   return "globals";
        case LuaApiScope::Methods:   return "methods";
        case LuaApiScope::Metatable: return "metatable";
    }
    return "?";
}

static std::vector<const LuaApiModule*> modulesAt(const std::vector<const LuaApiModule*>& mods,
                                                  LuaApiScope scope, const char* path) {
    std::vector<const LuaApiModule*> found;
    for (const auto* m : mods)
        if (m->scope == scope && std::strcmp(m->path, path) == 0) found.push_back(m);
    return found;
}

static bool describes(const std::vector<const LuaApiModule*>& mods, const std::string& name) {
    for (const auto* m : mods)
        for (size_t i = 0; i < m->count; ++i)
            if (m->entries[i].kind != LuaApiKind::Lifecycle && name == m->entries[i].name)
                return true;
    return false;
}

// Every string key of the table at the top of the stack has a descriptor entry.
static void expectDescribed(lua_State* L, const std::vector<const LuaApiModule*>& mods,
                            const std::string& where) {
    ASSERT(!mods.empty(), where + " is a descriptor module");
    for (const std::string& key : tableKeys(L, -1))
        ASSERT(describes(mods, key), where + "." + key + " has a descriptor");
}

static void testEnjinCoreModules() {
    LuaEngine engine;
    LuaBindings bindings(&engine);
    engine.initialize();
    bindings.registerAll();
    lua_State* L = engine.getState();

    const auto modules = luaApiModules(L);
    for (const auto* m : modules) {
        std::string errors;
        ASSERT(validateLuaApiModule(*m, &errors),
               std::string(scopeName(m->scope)) + " '" + m->path + "' validates:\n" + errors);
    }

    // The converted core surfaces are registered through descriptors, and
    // everything they put in Lua is described.
    lua_getglobal(L, "gfx");
    expectDescribed(L, modulesAt(modules, LuaApiScope::Table, "gfx"), "gfx");
    lua_pop(L, 1);
    for (const char* sub : {"async", "tween", "ui"}) {
        lua_getglobal(L, "engine");
        lua_getfield(L, -1, sub);
        const std::string path = std::string("engine.") + sub;
        expectDescribed(L, modulesAt(modules, LuaApiScope::Table, path.c_str()), path);
        lua_pop(L, 2);
    }

    const auto globals = modulesAt(modules, LuaApiScope::Globals, "");
    for (const char* name : {"print", "Vec2", "Point", "Rect", "clamp", "lerp", "remap", "sign",
                             "smoothstep", "distance"})
        ASSERT(describes(globals, name), std::string("global ") + name + " has a descriptor");
    for (const char* type : {"Vec2", "Point", "Rect"})
        ASSERT(!modulesAt(modules, LuaApiScope::Metatable, type).empty(),
               std::string(type) + " metatable module");
    for (const char* type : {"Vec2", "Rect"}) {
        const auto methods = modulesAt(modules, LuaApiScope::Methods, type);
        lua_getfield(L, LUA_REGISTRYINDEX, (std::string(type) + "_methods").c_str());
        expectDescribed(L, methods, std::string(type) + " methods");
        lua_pop(L, 1);
    }
}

//==============================================================================
// engine.* (Tomodachi #258): fully described; the switchable features
//==============================================================================

static bool evalBool(lua_State* L, const char* expr) {
    const std::string code = std::string("return ") + expr;
    if (luaL_dostring(L, code.c_str()) != LUA_OK) {
        fprintf(stderr, "  lua error: %s\n", lua_tostring(L, -1));
        lua_pop(L, 1);
        return false;
    }
    const bool v = lua_toboolean(L, -1) != 0;
    lua_pop(L, 1);
    return v;
}

// The registry lists top-level modules; nested ones (a metatable's __index
// methods, gfx.COLOR) are reached through Table entries. Both, flattened.
static void addWithNested(std::vector<const LuaApiModule*>& out, const LuaApiModule* m) {
    out.push_back(m);
    for (size_t i = 0; i < m->count; ++i)
        if (m->entries[i].kind == LuaApiKind::Table) addWithNested(out, m->entries[i].table);
}

static std::vector<const LuaApiModule*> allModules(lua_State* L) {
    std::vector<const LuaApiModule*> out;
    for (const auto* m : luaApiModules(L)) addWithNested(out, m);
    return out;
}

static void expectAllValidate(const std::vector<const LuaApiModule*>& modules,
                              const std::string& where) {
    for (const auto* m : modules) {
        std::string errors;
        ASSERT(validateLuaApiModule(*m, &errors), where + ": " + scopeName(m->scope) + " '" +
                                                      m->path + "' validates:\n" + errors);
    }
}

// Every key of `engine` is described: a value by an "engine" module entry, a
// subtable by the modules registered at "engine.<key>" (which must describe
// every key of that subtable).
static void expectEngineDescribed(lua_State* L, const std::vector<const LuaApiModule*>& mods,
                                  const std::string& where) {
    lua_getglobal(L, "engine");
    ASSERT(lua_istable(L, -1), where + ": engine is a table");
    const auto top = modulesAt(mods, LuaApiScope::Table, "engine");
    for (const std::string& key : tableKeys(L, -1)) {
        lua_getfield(L, -1, key.c_str());
        const std::string path = "engine." + key;
        if (lua_istable(L, -1)) {
            expectDescribed(L, modulesAt(mods, LuaApiScope::Table, path.c_str()),
                            where + ": " + path);
        } else {
            ASSERT(describes(top, key), where + ": " + path + " has a descriptor");
        }
        lua_pop(L, 1);
    }
    lua_pop(L, 1);
}

// The metatable `mtName` is described by the Metatable modules at `type`
// (every key but Lua's own __name), and when its __index is a table, that
// table is described by the Methods module its __index entry names.
static void expectMetatableDescribed(lua_State* L, const std::vector<const LuaApiModule*>& mods,
                                     const char* mtName, const char* type) {
    luaL_getmetatable(L, mtName);
    ASSERT(lua_istable(L, -1), std::string(mtName) + " metatable is registered");
    if (!lua_istable(L, -1)) { lua_pop(L, 1); return; }
    const auto meta = modulesAt(mods, LuaApiScope::Metatable, type);
    ASSERT(!meta.empty(), std::string(type) + " has a metatable module");
    for (const std::string& key : tableKeys(L, -1)) {
        if (key == "__name") continue;
        ASSERT(describes(meta, key), std::string(type) + " metatable " + key + " has a descriptor");
    }
    lua_getfield(L, -1, "__index");
    if (lua_istable(L, -1)) {
        // The methods module is the one the metatable's __index entry builds.
        const char* methodsPath = type;
        for (const auto* m : meta)
            for (size_t i = 0; i < m->count; ++i)
                if (m->entries[i].kind == LuaApiKind::Table &&
                    std::strcmp(m->entries[i].name, "__index") == 0)
                    methodsPath = m->entries[i].table->path;
        expectDescribed(L, modulesAt(mods, LuaApiScope::Methods, methodsPath),
                        std::string(type) + " methods");
    }
    lua_pop(L, 2);
}

static const char* const kProxyMetatables[] = {
    "ScriptProxy",      "ObjectProxy",       "C_Position_Proxy",     "C_Timer_Proxy",
    "C_StateMachine_Proxy", "C_Tilemap_Proxy", "C_Camera_Proxy",     "C_Sprite_Proxy",
    "C_Body_Proxy",     "ColliderSet_Proxy",
};

static bool anyProxyMetatable(lua_State* L) {
    bool any = false;
    for (const char* mt : kProxyMetatables) {
        luaL_getmetatable(L, mt);
        any = any || !lua_isnil(L, -1);
        lua_pop(L, 1);
    }
    return any;
}

static void testEngineTableDescribed() {
    LuaEngine engine;
    LuaBindings bindings(&engine);
    engine.initialize();
    bindings.registerAll();
    lua_State* L = engine.getState();
    const auto modules = allModules(L);

    expectAllValidate(modules, "defaults");
    expectEngineDescribed(L, modules, "defaults");

    // engine.graphics re-exports gfx functions with gfx's own descriptors.
    const auto gfx = modulesAt(modules, LuaApiScope::Table, "gfx");
    const auto graphics = modulesAt(modules, LuaApiScope::Table, "engine.graphics");
    ASSERT(!graphics.empty(), "engine.graphics is a descriptor module");
    for (const auto* m : graphics) {
        for (size_t i = 0; i < m->count; ++i) {
            const LuaApiEntry& alias = m->entries[i];
            bool same = false;
            for (const auto* g : gfx)
                for (size_t j = 0; j < g->count; ++j)
                    same = same || (std::strcmp(g->entries[j].name, alias.name) == 0 &&
                                    g->entries[j].func == alias.func &&
                                    std::strcmp(g->entries[j].signature, alias.signature) == 0 &&
                                    std::strcmp(g->entries[j].summary, alias.summary) == 0);
            ASSERT(same, std::string("engine.graphics.") + alias.name + " is gfx's entry");
        }
    }

    // The scene-free map handle (#256) and the HUD value objects (#83).
    expectMetatableDescribed(L, modules, "enjin2.Tilemap", "Tilemap");
    expectMetatableDescribed(L, modules, "enjin2.RollingCounter", "RollingCounter");
    expectMetatableDescribed(L, modules, "enjin2.Timer", "Timer");

    // Every switchable feature is off by default: not registered, not listed.
    ASSERT(evalBool(L, "engine.scene == nil"), "engine.scene is off by default");
    ASSERT(evalBool(L, "engine.camera == nil"), "engine.camera is off by default");
    ASSERT(evalBool(L, "engine.debug == nil"), "engine.debug is off by default");
    ASSERT(evalBool(L, "engine.physics.raycast == nil"), "physics.raycast is off by default");
    ASSERT(evalBool(L, "engine.physics.bounce ~= nil"), "the rest of engine.physics stays");
    ASSERT(!anyProxyMetatable(L), "no proxy metatable is registered by default");
    for (const char* path : {"engine.scene", "engine.camera", "engine.debug"})
        ASSERT(modulesAt(modules, LuaApiScope::Table, path).empty(),
               std::string(path) + " is not in the registry when off");
    ASSERT(!describes(modulesAt(modules, LuaApiScope::Table, "engine.physics"), "raycast"),
           "raycast is not in the registry when off");

    // A map handle still works with the proxies off.
    lua::pushTilemap(L);
    lua_setglobal(L, "handle");
    ASSERT(luaL_dostring(L, "handle:setTiles({1, 2, 3, 4}, 2, 2)") == LUA_OK,
           "a map handle's setTiles runs with the proxies off");
    ASSERT(evalBool(L, "handle:getTile(1, 1) == 4"), "and its getTile reads the map back");
}

// Each switch registers exactly its own feature, from its descriptors.
static void testEachFeatureSwitch() {
    struct Case {
        const char* name;
        bool LuaFeatures::*flag;
        const char* probe;  // nullptr: the proxies (probed by metatable)
    };
    static const Case kCases[] = {
        {"scene", &LuaFeatures::scene, "engine.scene"},
        {"camera", &LuaFeatures::camera, "engine.camera"},
        {"debug", &LuaFeatures::debug, "engine.debug"},
        {"raycast", &LuaFeatures::raycast, "engine.physics.raycast"},
        {"proxies", &LuaFeatures::proxies, nullptr},
    };
    for (const Case& on : kCases) {
        LuaEngine engine;
        LuaBindings bindings(&engine);
        LuaFeatures features;
        features.*on.flag = true;
        bindings.setFeatures(features);
        engine.initialize();
        bindings.registerAll();
        lua_State* L = engine.getState();
        const auto modules = allModules(L);
        const std::string where = std::string(on.name) + " on";

        expectAllValidate(modules, where);
        expectEngineDescribed(L, modules, where);
        for (const Case& other : kCases) {
            const bool present = other.probe
                ? evalBool(L, (std::string(other.probe) + " ~= nil").c_str())
                : anyProxyMetatable(L);
            ASSERT(present == (&other == &on),
                   where + ": " + other.name + (present ? " registered" : " not registered"));
        }
        if (on.flag == &LuaFeatures::proxies) {
            // Every proxy metatable, and the methods its __index consults, is described.
            static const struct { const char* mt; const char* type; const char* methods; } kProxies[] = {
                {"ScriptProxy", "ScriptProxy", "ScriptProxy.methods"},
                {"ObjectProxy", "ObjectProxy", "ObjectProxy.methods"},
                {"C_Position_Proxy", "C_Position", "C_Position.methods"},
                {"C_Timer_Proxy", "C_Timer", nullptr},
                {"C_StateMachine_Proxy", "C_StateMachine", nullptr},
                {"C_Tilemap_Proxy", "C_Tilemap", nullptr},
                {"C_Camera_Proxy", "C_Camera", nullptr},
                {"C_Sprite_Proxy", "C_Sprite", "C_Sprite.methods"},
                {"C_Body_Proxy", "C_Body", "C_Body.methods"},
                {"ColliderSet_Proxy", "ColliderSet", nullptr},
            };
            static_assert(sizeof(kProxies) / sizeof(kProxies[0]) ==
                              sizeof(kProxyMetatables) / sizeof(kProxyMetatables[0]),
                          "every proxy metatable is checked");
            for (const auto& p : kProxies) {
                expectMetatableDescribed(L, modules, p.mt, p.type);
                if (p.methods) {
                    lua_getfield(L, LUA_REGISTRYINDEX, p.methods);
                    expectDescribed(L, modulesAt(modules, LuaApiScope::Methods, p.type),
                                    std::string(p.type) + " methods");
                    lua_pop(L, 1);
                }
            }
        }
        if (on.flag == &LuaFeatures::raycast) {
            ASSERT(describes(modulesAt(modules, LuaApiScope::Table, "engine.physics"), "raycast"),
                   "raycast has a descriptor when on");
        }
    }
}

int main() {
    testRoundTrip();
    testStructure();
    testWhitespaceNormalises();
    testMalformed();
    testModuleExposesExactlyItsEntries();
    testRegistryIsPerVm();
    testValidateCatchesDrift();
    testEnjinCoreModules();
    testEngineTableDescribed();
    testEachFeatureSwitch();

    printf("lua_api_test: %d passed, %d failed\n", passes, failures);
    return failures == 0 ? 0 : 1;
}
