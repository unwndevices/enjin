/**
 * @file lua_api.hpp
 * @brief Lua API descriptors: the registration input that also documents the API
 *
 * ADR-0013 (Tomodachi): each binding's descriptor array IS its registration
 * input, the way the audio catalog's NodeDescriptor carries its factory. The
 * entry that registers a function is the entry that documents it, so a
 * function cannot reach Lua without a description, and the Studio's API
 * reference reads exactly what the VM runs.
 *
 * Descriptor arrays are `static constexpr` so they live in flash on the
 * device: every text field is a string literal and nothing allocates.
 *
 * ## Signature grammar
 *
 * A function's `signature` is one or more overloads, one per line:
 *
 *     signature := overload { "\n" overload }
 *     overload  := "(" [ param { ", " param } ] ") -> " returns
 *     param     := name ":" type [ "?" [ "=" default ] ]
 *                | "..." [ ":" type ]                  -- variadic, last only
 *     returns   := "nil"                               -- no values
 *                | result { ", " result }
 *     result    := [ name ":" ] type [ "?" ]           -- "?" = may be nil
 *                | "..." [ ":" type ]                  -- variadic, last only
 *     type      := ident { "|" ident }
 *     name      := ident
 *     ident     := [A-Za-z_][A-Za-z0-9_]*
 *     default   := '"' { any char but '"' } '"'
 *                | [A-Za-z0-9_.+-]+                    -- 0, -1, 1.5, false, current
 *
 * Builtin types are int, number, string, boolean, table, function, userdata,
 * thread, any and nil. A capitalised type names a userdata type (Vec2,
 * Effect) or one of the entry's named enums (Easing, StyleSlot). A required
 * parameter may not follow an optional one; polymorphic calls are written as
 * separate overloads instead. Whitespace around tokens is tolerated;
 * formatLuaApiSignature() writes the canonical spacing shown above, e.g.
 *
 *     (layer:int, x:int, y:int, color:int?=current) -> nil
 *     (target:table, key:string, goal:number, params:table?) -> id:int?
 *
 * A Methods-scope signature omits the receiver: v:rotate(rad) is written
 * "(rad:number) -> Vec2". A Metatable-scope signature lists every operand.
 *
 * ## Argument lines
 *
 * `args` holds one line per distinct parameter name across all overloads,
 * in first-appearance order, each written "name: description" and separated
 * by "\n". A variadic parameter's line starts with "...:".
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

extern "C" {
#include "lua.h"
#include "lauxlib.h"
}

namespace enjin2 {

enum class LuaApiKind : uint8_t {
    Function,   ///< A C function, registered under `name`
    Constant,   ///< An integer constant, registered under `name`
    Table,      ///< A nested table built from the `table` module
    Lifecycle,  ///< A callback the script defines and the host calls (documented, never registered)
};

/**
 * @brief A named set of string values an argument accepts.
 *
 * `values` is the same C array the binding matches its argument against (with
 * nameIndex(), core/name_index.hpp), so the documented set and the accepted
 * set are one array.
 */
struct LuaApiEnum {
    const char*        name;    ///< Type name used in signatures ("Easing")
    const char* const* values;  ///< The binding's own name array
    size_t             count;
};

template <size_t N>
constexpr LuaApiEnum luaApiEnum(const char* name, const char* const (&values)[N]) {
    return LuaApiEnum{name, values, N};
}

struct LuaApiModule;

/** @brief One registered (or, for Lifecycle, documented) name. */
struct LuaApiEntry {
    LuaApiKind          kind = LuaApiKind::Function;
    const char*         name = nullptr;
    lua_CFunction       func = nullptr;       ///< Function
    lua_Integer         value = 0;            ///< Constant
    const LuaApiModule* table = nullptr;      ///< Table
    const char*         signature = nullptr;  ///< Function / Lifecycle; see grammar above
    const char*         summary = nullptr;    ///< One line
    const char*         args = nullptr;       ///< One "name: text" line per parameter
    const char*         hostNote = nullptr;   ///< Optional free text (host differences, caveats)
    const LuaApiEnum*   enums = nullptr;      ///< Named enums the signature refers to
    size_t              enumCount = 0;

    /// Attach a host note.
    constexpr LuaApiEntry note(const char* text) const {
        LuaApiEntry e = *this;
        e.hostNote = text;
        return e;
    }
    /// Attach the named enum this entry's signature refers to.
    constexpr LuaApiEntry withEnum(const LuaApiEnum& en) const {
        LuaApiEntry e = *this;
        e.enums = &en;
        e.enumCount = 1;
        return e;
    }
    /// Attach several named enums.
    template <size_t N>
    constexpr LuaApiEntry withEnums(const LuaApiEnum (&list)[N]) const {
        LuaApiEntry e = *this;
        e.enums = list;
        e.enumCount = N;
        return e;
    }
};

/// A C function. `args` may be nullptr only when the signature has no parameters.
constexpr LuaApiEntry luaFunction(const char* name, lua_CFunction func,
                                  const char* signature, const char* summary,
                                  const char* args = nullptr) {
    LuaApiEntry e{};
    e.kind = LuaApiKind::Function;
    e.name = name;
    e.func = func;
    e.signature = signature;
    e.summary = summary;
    e.args = args;
    return e;
}

/// An integer constant.
constexpr LuaApiEntry luaConstant(const char* name, lua_Integer value, const char* summary) {
    LuaApiEntry e{};
    e.kind = LuaApiKind::Constant;
    e.name = name;
    e.value = value;
    e.summary = summary;
    return e;
}

/// A nested table built from `module` (its path should extend the parent's).
constexpr LuaApiEntry luaTable(const char* name, const LuaApiModule& module,
                               const char* summary) {
    LuaApiEntry e{};
    e.kind = LuaApiKind::Table;
    e.name = name;
    e.table = &module;
    e.summary = summary;
    return e;
}

/// A callback the script defines (e.g. update(dt)); documented, never registered.
constexpr LuaApiEntry luaLifecycle(const char* name, const char* signature,
                                   const char* summary, const char* args = nullptr) {
    LuaApiEntry e{};
    e.kind = LuaApiKind::Lifecycle;
    e.name = name;
    e.signature = signature;
    e.summary = summary;
    e.args = args;
    return e;
}

/** @brief Where a module's entries live once registered. */
enum class LuaApiScope : uint8_t {
    Table,      ///< A table at `path` ("gfx", "engine.tween", "gfx.COLOR")
    Globals,    ///< Bare globals; `path` is ""
    Methods,    ///< Methods of userdata type `path`, called as v:name(...)
    Metatable,  ///< Metamethods of userdata type `path` (__add, __index, ...)
};

/** @brief A descriptor array plus where it is registered. */
struct LuaApiModule {
    LuaApiScope        scope = LuaApiScope::Table;
    const char*        path = "";
    const char*        summary = nullptr;
    const LuaApiEntry* entries = nullptr;
    size_t             count = 0;
};

template <size_t N>
constexpr LuaApiModule luaApiModule(LuaApiScope scope, const char* path, const char* summary,
                                    const LuaApiEntry (&entries)[N]) {
    return LuaApiModule{scope, path, summary, entries, N};
}

//==============================================================================
// Registration. Every helper records the module in the VM's API registry, so
// luaApiModules() lists exactly what was registered into that VM.
//==============================================================================

/// Fill the existing table at `tableIdx` (a metatable, a methods table).
void luaApiSetFields(lua_State* L, int tableIdx, const LuaApiModule& module);

/// Build a new table from `module` and store it as `parent[leaf of path]`.
void luaApiSetSubtable(lua_State* L, int parentIdx, const LuaApiModule& module);

/// Build a new table from `module` and store it as the global `path`.
void luaApiSetGlobalTable(lua_State* L, const LuaApiModule& module);

/// Register every entry of a Globals-scope module as a bare global.
void luaApiSetGlobals(lua_State* L, const LuaApiModule& module);

/// The modules registered into this VM, in registration order. Nested tables
/// are reached through their parent's Table entries, not listed separately.
std::vector<const LuaApiModule*> luaApiModules(lua_State* L);

//==============================================================================
// Signatures (grammar at the top of this file)
//==============================================================================

struct LuaApiParam {
    std::string name;          ///< "..." for a variadic
    std::string type;          ///< "" only for an untyped variadic
    bool        optional = false;
    std::string defaultValue;  ///< As written, quotes included; "" = none
};

struct LuaApiResult {
    std::string name;  ///< "" when unnamed; "..." for a variadic
    std::string type;
    bool        optional = false;
};

struct LuaApiOverload {
    std::vector<LuaApiParam>  params;
    std::vector<LuaApiResult> results;  ///< Empty = "nil"
};

struct LuaApiSignature {
    std::vector<LuaApiOverload> overloads;
};

/// Parse `text`; on failure return false and describe the problem in `error`.
bool parseLuaApiSignature(const char* text, LuaApiSignature& out, std::string* error = nullptr);

/// The canonical text of a parsed signature (parse -> format round-trips).
std::string formatLuaApiSignature(const LuaApiSignature& sig);

/// Split an entry's `args` into (name, description) pairs, in order.
std::vector<std::pair<std::string, std::string>> luaApiArgLines(const LuaApiEntry& entry);

/**
 * @brief Check a module's descriptors against the grammar and each other.
 *
 * Every Function/Lifecycle signature parses; its argument lines name exactly
 * its distinct parameters, in order; every lowercase type is a builtin and
 * every enum the entry declares is non-empty; Table entries recurse. Returns
 * false with one line per problem in `errors`.
 */
bool validateLuaApiModule(const LuaApiModule& module, std::string* errors = nullptr);

} // namespace enjin2
