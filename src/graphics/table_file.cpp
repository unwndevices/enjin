#include "enjin2/graphics/table_file.hpp"

extern "C" {
#include "lauxlib.h"
#include "lua.h"
#include "lualib.h"
}

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/stat.h>

#ifdef ESP32
#include <esp_heap_caps.h>
#endif

namespace enjin2 {

namespace {

void* reallocBlock(void* ptr, size_t nsize) {
#ifdef ESP32
    void* p = heap_caps_realloc(ptr, nsize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    return p ? p : heap_caps_realloc(ptr, nsize, MALLOC_CAP_8BIT);
#else
    return std::realloc(ptr, nsize);
#endif
}

bool fail(std::string* error, const std::string& msg) {
    if (error) *error = msg;
    return false;
}

// VM instructions a file (and its validator) may run before the read is cut
// off: far past what any real skin, palette or manifest runs.
constexpr int kInstructionLimit = 100000;

void instructionLimitHit(lua_State* L, lua_Debug*) {
    luaL_error(L, "instruction limit exceeded");
}

// What the state holds against what it may: growth past the limit fails,
// which Lua turns into LUA_ERRMEM. Shrinking and freeing never fail.
struct Budget {
    size_t used = 0;
    size_t limit = 0;
};

// On device plain malloc is internal RAM only, which a whole VM does not fit
// beside TinyUSB and BLE (Tomodachi #277): prefer PSRAM, like the applet VM
// (LuaPlatform::createState).
void* tableFileAlloc(void* ud, void* ptr, size_t osize, size_t nsize) {
    auto* budget = static_cast<Budget*>(ud);
    const size_t old = ptr ? osize : 0;  // without a block osize is the object's type
    if (nsize == 0) {
        std::free(ptr);
        budget->used -= old;
        return nullptr;
    }
    if (nsize > old && budget->used - old + nsize > budget->limit) return nullptr;
    void* p = reallocBlock(ptr, nsize);
    if (p) budget->used = budget->used - old + nsize;
    return p;
}

struct Chunk {
    std::string_view source;
    const char* what;
    const char* validator;
};

// A validator's libraries: base (without the file loaders), string and math.
void openValidatorLibraries(lua_State* L) {
    luaL_requiref(L, LUA_GNAME, luaopen_base, 1);
    luaL_requiref(L, LUA_STRLIBNAME, luaopen_string, 1);
    luaL_requiref(L, LUA_MATHLIBNAME, luaopen_math, 1);
    lua_pop(L, 3);
    lua_pushnil(L);
    lua_setglobal(L, "dofile");
    lua_pushnil(L);
    lua_setglobal(L, "loadfile");
}

// Under lua_pcall, so running out of memory anywhere in it is an error: load
// text only and run it in an empty _ENV, or hand it to the validator, and
// require a table back. No C++ object lives in this frame: a Lua error
// leaves it by longjmp.
int runChunk(lua_State* L) {
    const auto* chunk = static_cast<const Chunk*>(lua_touserdata(L, 1));
    char name[64];
    if (chunk->validator) {
        openValidatorLibraries(L);
        std::snprintf(name, sizeof name, "=%s validator", chunk->what);
        if (luaL_loadbufferx(L, chunk->validator, std::strlen(chunk->validator), name, "t") != LUA_OK)
            return lua_error(L);
        lua_pushlstring(L, chunk->source.data(), chunk->source.size());
        lua_call(L, 1, 1);
    } else {
        std::snprintf(name, sizeof name, "=%s", chunk->what);
        if (luaL_loadbufferx(L, chunk->source.data(), chunk->source.size(), name, "t") != LUA_OK)
            return lua_error(L);
        lua_newtable(L);
        lua_setupvalue(L, -2, 1);  // the chunk's _ENV
        lua_call(L, 0, 1);
    }
    if (!lua_istable(L, -1)) return luaL_error(L, "%s must return a table", chunk->what);
    return 1;
}

} // namespace

bool readTableFileText(const std::string& path, size_t limit, std::string& out,
                       std::string* error) {
    out.clear();
    struct stat st {};
    if (stat(path.c_str(), &st) || !S_ISREG(st.st_mode)) return fail(error, "not a file");
    if (st.st_size <= 0 || static_cast<size_t>(st.st_size) > limit)
        return fail(error, "empty or larger than " +
                               (limit % 1024 ? std::to_string(limit) + " bytes"
                                             : std::to_string(limit / 1024) + " KB"));
    FILE* file = std::fopen(path.c_str(), "rb");
    if (!file) return fail(error, "cannot open");
    out.resize(static_cast<size_t>(st.st_size));
    const bool read = std::fread(out.data(), 1, out.size(), file) == out.size();
    std::fclose(file);
    if (!read) {
        out.clear();
        return fail(error, "cannot read");
    }
    if (out.find('\0') != std::string::npos) {
        out.clear();
        return fail(error, "has a NUL byte");
    }
    return true;
}

bool readTableFile(std::string_view source, const char* what, const TableFieldReader& read,
                   std::string* error, const TableFileOptions& options) {
    Budget budget;
    budget.limit = options.memoryLimit;
    lua_State* L = lua_newstate(tableFileAlloc, &budget);
    if (!L) return fail(error, "cannot allocate a Lua state");
    lua_sethook(L, instructionLimitHit, LUA_MASKCOUNT, kInstructionLimit);
    Chunk chunk{source, what, options.validator};
    lua_pushcfunction(L, runChunk);
    lua_pushlightuserdata(L, &chunk);
    const int status = lua_pcall(L, 1, 1, 0);
    lua_sethook(L, nullptr, 0, 0);
    bool ok;
    if (status != LUA_OK) {
        const char* msg = lua_tostring(L, -1);
        ok = fail(error, msg ? msg : std::string(what) + " could not be read");
    } else {
        // The reader runs outside the protection (it is C++: a Lua error's
        // longjmp would skip its destructors), so it must not run out: its
        // few allocations (field names) are not held to the budget.
        budget.limit = SIZE_MAX;
        ok = read(L, lua_gettop(L), error);
    }
    lua_close(L);
    return ok;
}

} // namespace enjin2
