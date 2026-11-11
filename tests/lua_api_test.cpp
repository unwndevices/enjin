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

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <new>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

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
static int fnC(lua_State* L) { lua_pushinteger(L, 3); return 1; }

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
    luaFunction("demoGlobal", fnC, "() -> int", "A bare global."),
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
    ASSERT(evalInt(L, "demoGlobal()") == 3, "Globals scope registers bare globals");

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
// The merged view and the _G census (Tomodachi #260)
//==============================================================================

// A second module at "demo" that re-registers `a` and adds `c`, the way
// libtomo's stroke module augments gfx.
static constexpr LuaApiEntry kDemoOverride[] = {
    luaFunction("a", fnB, "() -> int", "Overrides a."),
    luaFunction("c", fnA, "() -> int", "Adds c."),
};
static constexpr LuaApiModule kDemoOverrideModule =
    luaApiModule(LuaApiScope::Table, "demo", "Demo, augmented.", kDemoOverride);

static std::vector<std::string> entryNames(const LuaApiTableView& t) {
    std::vector<std::string> names;
    for (const auto* e : t.entries) names.push_back(e->name);
    return names;
}

static void testViewMergesByPlace() {
    lua_State* L = luaL_newstate();
    luaL_openlibs(L);
    luaApiSetGlobalTable(L, kDemoModule);
    lua_getglobal(L, "demo");
    luaApiSetFields(L, -1, kDemoOverrideModule);
    lua_pop(L, 1);

    const auto view = luaApiView(L);
    ASSERT(view.size() == 2, "demo and demo.INNER, one table each");
    if (view.size() == 2) {
        ASSERT(view[0].path == "demo" && view[1].path == "demo.INNER", "nested follows its parent");
        ASSERT(std::string(view[0].summary) == "A demo module.", "the first module's summary");
        ASSERT((entryNames(view[0]) ==
                std::vector<std::string>{"a", "b", "K", "INNER", "update", "c"}),
               "a keeps its place, c is appended");
        ASSERT(view[0].entries[0]->func == fnB, "a is the later registration");
    }
    ASSERT(evalInt(L, "demo.a()") == 2, "Lua holds the later a too");
    lua_close(L);
}

static int undescribed(lua_State*) { return 0; }

static void testCensusFindsUndescribedNames() {
    lua_State* L = luaL_newstate();
    luaL_openlibs(L);
    LuaPlatform::configureSecurityRestrictions(L);
    luaApiSetGlobalTable(L, kDemoModule);
    luaApiSetGlobals(L, kGlobalsModule);
    // Workaround for the host build only: enjin's desktop tests link the system
    // Lua, which may carry LUA_COMPAT_5_3 names (math.pow, ...) the stdlib list
    // rightly leaves out, so compare with this VM's own starting problems. The
    // Tomodachi census (vendored Lua, as on the device) expects none at all.
    const std::vector<std::string> before = luaApiCensus(L);
    for (const auto& p : before)
        ASSERT(p.rfind("demo", 0) != 0, "the demo module is fully described: " + p);

    lua_register(L, "sneaky", undescribed);
    lua_getglobal(L, "demo");
    lua_pushcfunction(L, undescribed);
    lua_setfield(L, -2, "a");          // described name, other function
    lua_pushnil(L);
    lua_setfield(L, -2, "K");          // described name, gone
    lua_pushcfunction(L, undescribed);
    lua_setfield(L, -2, "extra");      // no descriptor
    lua_pop(L, 1);
    lua_pushnil(L);
    lua_setglobal(L, "pairs");         // listed stdlib name, gone

    std::vector<std::string> added;
    for (const auto& p : luaApiCensus(L))
        if (std::find(before.begin(), before.end(), p) == before.end()) added.push_back(p);
    std::sort(added.begin(), added.end());
    const std::vector<std::string> expected = {
        "demo.K is described but not registered",
        "demo.a is not the function its descriptor registers",
        "demo.extra has no descriptor",
        "sneaky has no descriptor",
        "stdlib pairs is listed but not reachable",
    };
    std::string got;
    for (const auto& p : added) got += "\n  " + p;
    ASSERT(added == expected, "census reports exactly the drift:" + got);
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
// ADR-0030: camelCase, one home per job, nil, err or raise
//==============================================================================

static void testValidateHoldsTheConvention() {
    static constexpr LuaApiEntry kInner[] = {
        luaConstant("DEEP_ONE", 1, "A constant may be UPPER_SNAKE."),
    };
    static constexpr LuaApiModule kInnerModule =
        luaApiModule(LuaApiScope::Table, "good.INNER", "Constants.", kInner);
    static constexpr LuaApiEntry kGood[] = {
        luaFunction("loadThing", fnA, "(path:string) -> thing:int?, err:string?",
                    "Load a thing; nil and a reason when the card says no.", "path: the file")
            .returnsErr(),
        luaFunction("Vec", fnA, "(_:nil, x:number) -> nil", "A constructor-style name.",
                    "_: always nil\nx: a value"),
        luaFunction("isBusy", fnB, "() -> boolean", "A query; nothing fails."),
        luaConstant("LAYER_BG", 1, "A constant."),
        luaTable("INNER", kInnerModule, "A table of constants."),
    };
    static constexpr LuaApiModule kGoodModule =
        luaApiModule(LuaApiScope::Table, "good", "Good.", kGood);
    std::string errors;
    ASSERT(validateLuaApiModule(kGoodModule, &errors), "the convention validates: " + errors);

    static constexpr LuaApiEntry kBad[] = {
        luaFunction("play_note", fnA, "() -> nil", "A snake_case function."),
        luaFunction("snakeArg", fnA, "(lo_end:number) -> nil", "A snake_case parameter.",
                    "lo_end: low end"),
        luaFunction("snakeResult", fnA, "() -> peak_db:number", "A snake_case result."),
        luaFunction("errNoShape", fnA, "(path:string) -> int", "Says nil, err but returns one.",
                    "path: the file")
            .returnsErr(),
        luaFunction("errNotLast", fnA, "() -> err:string?, x:int?", "err must come last.")
            .returnsErr(),
        luaFunction("errUndeclared", fnA, "() -> x:int?, err:string?",
                    "Returns an err but is documented as raising."),
        luaFunction("minusOne", fnA, "(path:string) -> handle:int",
                    "Load a sample; -1 on failure.", "path: the file"),
        luaFunction("nilWhenBusy", fnA, "() -> id:int?", "Start one; nil when all 8 slots are busy."),
        luaFunction("printsIt", fnA, "() -> nil", "Do it.")
            .note("The failure reason is printed to the log."),
    };
    static constexpr LuaApiModule kBadModule =
        luaApiModule(LuaApiScope::Table, "bad_table", "Bad.", kBad);
    errors.clear();
    ASSERT(!validateLuaApiModule(kBadModule, &errors), "the convention's breaches fail validation");
    for (const char* name : {"bad_table: ", "play_note", "snakeArg", "snakeResult", "errNoShape",
                             "errNotLast", "errUndeclared", "minusOne", "nilWhenBusy", "printsIt"})
        ASSERT(errors.find(name) != std::string::npos, std::string("reports ") + name + "\n" + errors);
}

static void testCensusFindsOneJobInTwoHomes() {
    static constexpr LuaApiEntry kSensorEntries[] = {
        luaFunction("tilt", fnA, "() -> number", "Tilt."),
    };
    static constexpr LuaApiModule kSensorGlobal =
        luaApiModule(LuaApiScope::Table, "radar", "Radar.", kSensorEntries);
    static constexpr LuaApiEntry kTwinEntries[] = {
        luaFunction("lean", fnB, "() -> number", "Tilt again."),
    };
    static constexpr LuaApiModule kSensorEngine =
        luaApiModule(LuaApiScope::Table, "engine.radar", "Radar again.", kTwinEntries);
    static constexpr LuaApiEntry kAliasEntries[] = {
        luaFunction("tilt", fnA, "() -> number", "Tilt under a second name."),
    };
    static constexpr LuaApiModule kAlias =
        luaApiModule(LuaApiScope::Table, "motion", "An alias table.", kAliasEntries);

    lua_State* L = luaL_newstate();
    luaL_openlibs(L);
    LuaPlatform::configureSecurityRestrictions(L);
    const std::vector<std::string> before = luaApiCensus(L);
    luaApiSetGlobalTable(L, kSensorGlobal);
    lua_newtable(L);
    lua_setglobal(L, "engine");
    lua_getglobal(L, "engine");
    luaApiSetSubtable(L, -1, kSensorEngine);
    lua_pop(L, 1);
    luaApiSetGlobalTable(L, kAlias);
    std::vector<std::string> added;
    for (const auto& p : luaApiCensus(L))
        if (std::find(before.begin(), before.end(), p) == before.end()) added.push_back(p);
    std::string got;
    for (const auto& p : added) got += "\n  " + p;
    ASSERT(std::find(added.begin(), added.end(),
                     "engine.radar and radar hold one job: engine.* is enjin's, a product table "
                     "is a global") != added.end(), "reports engine.X beside X:" + got);
    ASSERT(std::find(added.begin(), added.end(),
                     "motion.tilt and radar.tilt are one function in two homes") != added.end(),
           "reports a function registered twice:" + got);
    lua_close(L);
}

// One world failure per enjin subsystem without a test of its own: a full
// pool or slot table is nil, err (sprite, store, event bus and tilemap have
// theirs in their own tests).
static void testWorldFailuresReturnNilErr() {
    LuaEngine engine;
    LuaBindings bindings(&engine);
    engine.initialize();
    bindings.registerAll();
    lua_State* L = engine.getState();
    auto check = [&](const char* code, const char* what) {
        const LuaResult r = engine.executeString(code);
        ASSERT(r.success, std::string(what) + ": " + r.error);
    };
    check("for i = 1, 8 do assert(engine.async.start(function() coroutine.yield() end)) end "
          "local id, err = engine.async.start(function() end) "
          "assert(id == nil and err == 'coroutine slots full', err)",
          "engine.async.start full is nil, err");
    check("for i = 1, 8 do assert(engine.tween.to({x = 0}, {x = 1}, 1)) end "
          "local id, err = engine.tween.to({x = 0}, {x = 1}, 1) "
          "assert(id == nil and err == 'tween slots full', err)",
          "engine.tween.to full is nil, err");
    check("for i = 1, 16 do assert(engine.state.onEnter('s' .. i, function() end) == true) end "
          "local ok, err = engine.state.onExit('one more', function() end) "
          "assert(ok == nil and err == 'states full', err)",
          "engine.state.onExit full is nil, err");
    check("local ok, err = pcall(engine.async.start, 42) assert(not ok)",
          "a mistake still raises");
}

//==============================================================================
// enjin core: every module registerAll() installs is described and valid
//==============================================================================

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
               std::string(luaApiScopeName(m->scope)) + " '" + m->path + "' validates:\n" + errors);
    }

    // The converted core surfaces are registered through descriptors, and
    // everything they put in Lua is described.
    lua_getglobal(L, "gfx");
    expectDescribed(L, modulesAt(modules, LuaApiScope::Table, "gfx"), "gfx");
    lua_pop(L, 1);
    lua_getglobal(L, "ui");   // a global, not engine.ui (ADR-0030)
    expectDescribed(L, modulesAt(modules, LuaApiScope::Table, "ui"), "ui");
    lua_pop(L, 1);
    for (const char* sub : {"async", "tween"}) {
        lua_getglobal(L, "engine");
        lua_getfield(L, -1, sub);
        const std::string path = std::string("engine.") + sub;
        expectDescribed(L, modulesAt(modules, LuaApiScope::Table, path.c_str()), path);
        lua_pop(L, 2);
    }

    const auto globals = modulesAt(modules, LuaApiScope::Globals, "");
    for (const char* name : {"print", "Vec2", "Point", "Rect", "lerp", "remap", "sign",
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
        ASSERT(validateLuaApiModule(*m, &errors), where + ": " + luaApiScopeName(m->scope) + " '" +
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

    // One table per job (ADR-0030): gfx has no second name, and ui is a global.
    ASSERT(modulesAt(modules, LuaApiScope::Table, "engine.graphics").empty(), "no engine.graphics");
    ASSERT(modulesAt(modules, LuaApiScope::Table, (std::string("engine") + ".ui").c_str()).empty(),
           "no ui under engine");
    ASSERT(!modulesAt(modules, LuaApiScope::Table, "ui").empty(), "ui is a global table");

    // The scene-free map handle (#256) and the HUD value objects (#83).
    expectMetatableDescribed(L, modules, "enjin2.Tilemap", "Tilemap");
    expectMetatableDescribed(L, modules, "enjin2.RollingCounter", "RollingCounter");
    expectMetatableDescribed(L, modules, "enjin2.Timer", "Timer");
    // The seeded generator engine.random.new returns (Tomodachi #406).
    expectMetatableDescribed(L, modules, "enjin2.Random", "Random");
    ASSERT(describes(modulesAt(modules, LuaApiScope::Table, "engine.random"), "new"),
           "engine.random.new has a descriptor");
    ASSERT(describes(modulesAt(modules, LuaApiScope::Methods, "Random"), "integer") &&
               describes(modulesAt(modules, LuaApiScope::Methods, "Random"), "float"),
           "a generator's integer and float have descriptors");

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


//==============================================================================
// A C++ exception in a binding is a Lua error (Tomodachi #279, #465)
//==============================================================================

// An allocation that fails inside a binding: what make_unique<OscWave> did
// in audio.new when internal DRAM ran out.
static int fnOutOfMemory(lua_State*) { throw std::bad_alloc(); }
static int fnThrowsRuntime(lua_State*) { throw std::runtime_error("disk on fire"); }
static int fnThrowsInt(lua_State*) { throw 7; }
static int fnArgError(lua_State* L) { return luaL_checkinteger(L, 1) > 0 ? 0 : 0; }

static constexpr LuaApiEntry kThrowing[] = {
    luaFunction("oom", fnOutOfMemory, "() -> nil", "Runs out of memory."),
    luaFunction("runtime", fnThrowsRuntime, "() -> nil", "Throws std::runtime_error."),
    luaFunction("other", fnThrowsInt, "() -> nil", "Throws a non-std exception."),
    luaFunction("arg", fnArgError, "(n:int) -> nil", "Checks its argument."),
    luaFunction("ok", fnA, "() -> int", "Returns 1."),
};
static constexpr LuaApiModule kThrowingModule =
    luaApiModule(LuaApiScope::Table, "throwing", "Bindings that throw.", kThrowing);

static std::string pcallMessage(lua_State* L, const char* code) {
    if (luaL_dostring(L, code) == LUA_OK) {
        lua_settop(L, 0);
        return "<no error>";
    }
    std::string msg = lua_tostring(L, -1) ? lua_tostring(L, -1) : "<non-string error>";
    lua_settop(L, 0);
    return msg;
}

static void testExceptionsBecomeLuaErrors() {
    lua_State* L = luaL_newstate();
    luaL_openlibs(L);
    luaApiSetGlobalTable(L, kThrowingModule);

    const std::string oom = pcallMessage(L, "throwing.oom()");
    ASSERT(oom.find("not enough memory") != std::string::npos,
           "bad_alloc in a binding is a Lua 'not enough memory' error: " + oom);
    ASSERT(oom.find("throwing.oom: not enough memory") != std::string::npos,
           "the error names the binding: " + oom);
    ASSERT(evalInt(L, "select(2, pcall(throwing.oom)):find('throwing.oom', 1, true) and 1 or 0") == 1,
           "the name holds when the binding is called through pcall");

    ASSERT(evalInt(L, "pcall(throwing.oom) and 0 or 1") == 1,
           "a script can pcall a binding that runs out of memory");

    const std::string rt = pcallMessage(L, "throwing.runtime()");
    ASSERT(rt.find("disk on fire") != std::string::npos,
           "std::exception's what() reaches the Lua error: " + rt);

    const std::string other = pcallMessage(L, "throwing.other()");
    ASSERT(other.find("C++ exception") != std::string::npos,
           "any other exception is a Lua error too: " + other);

    const std::string arg = pcallMessage(L, "throwing.arg('x')");
    ASSERT(arg.find("bad argument #1 to 'arg'") != std::string::npos,
           "luaL_argerror still names the function through the guard: " + arg);

    ASSERT(evalInt(L, "throwing.ok()") == 1, "the VM keeps running after each");
    ASSERT(lua_gettop(L) == 0, "the stack is balanced");

    // The census still sees the descriptor's own function behind the guard.
    lua_getglobal(L, "throwing");
    lua_getfield(L, -1, "ok");
    ASSERT(luaApiFunction(L, -1) == fnA, "luaApiFunction unwraps the guard");
    lua_pop(L, 2);
    for (const auto& p : luaApiCensus(L))
        ASSERT(p.rfind("throwing", 0) != 0, "a guarded module is still fully described: " + p);
    lua_close(L);
}

int main() {
    testRoundTrip();
    testStructure();
    testWhitespaceNormalises();
    testMalformed();
    testModuleExposesExactlyItsEntries();
    testRegistryIsPerVm();
    testViewMergesByPlace();
    testCensusFindsUndescribedNames();
    testValidateCatchesDrift();
    testValidateHoldsTheConvention();
    testCensusFindsOneJobInTwoHomes();
    testWorldFailuresReturnNilErr();
    testEnjinCoreModules();
    testEngineTableDescribed();
    testEachFeatureSwitch();
    testExceptionsBecomeLuaErrors();

    printf("lua_api_test: %d passed, %d failed\n", passes, failures);
    return failures == 0 ? 0 : 1;
}
