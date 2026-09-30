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

int main() {
    testRoundTrip();
    testStructure();
    testWhitespaceNormalises();
    testMalformed();
    testModuleExposesExactlyItsEntries();
    testRegistryIsPerVm();
    testValidateCatchesDrift();
    testEnjinCoreModules();

    printf("lua_api_test: %d passed, %d failed\n", passes, failures);
    return failures == 0 ? 0 : 1;
}
