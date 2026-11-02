#include "enjin2/graphics/table_file.hpp"

extern "C" {
#include "lauxlib.h"
#include "lua.h"
}

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/stat.h>

#ifdef ESP32
#include <esp_heap_caps.h>
#endif

namespace enjin2 {

namespace {

bool fail(std::string* error, const std::string& msg) {
    if (error) *error = msg;
    return false;
}

void instructionLimit(lua_State* L, lua_Debug*) {
    luaL_error(L, "instruction limit exceeded");
}

// On device plain malloc is internal RAM only, which a whole VM does not fit
// beside TinyUSB and BLE (Tomodachi #277): prefer PSRAM, like the applet VM
// (LuaPlatform::createState).
void* tableFileAlloc(void*, void* ptr, size_t, size_t nsize) {
    if (nsize == 0) {
        std::free(ptr);
        return nullptr;
    }
#ifdef ESP32
    void* p = heap_caps_realloc(ptr, nsize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    return p ? p : heap_caps_realloc(ptr, nsize, MALLOC_CAP_8BIT);
#else
    return std::realloc(ptr, nsize);
#endif
}

struct Chunk {
    const std::string* source;
    const char* what;
};

// Under lua_pcall: load text only, run in an empty _ENV, require a table back.
int runChunk(lua_State* L) {
    const auto* chunk = static_cast<const Chunk*>(lua_touserdata(L, 1));
    const std::string name = std::string("=") + chunk->what;
    if (luaL_loadbufferx(L, chunk->source->data(), chunk->source->size(), name.c_str(), "t") != LUA_OK)
        return lua_error(L);
    lua_newtable(L);
    lua_setupvalue(L, -2, 1);  // the chunk's _ENV
    lua_call(L, 0, 1);
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

bool readTableFile(const std::string& source, const char* what, const TableFieldReader& read,
                   std::string* error) {
    lua_State* L = lua_newstate(tableFileAlloc, nullptr);
    if (!L) return fail(error, "cannot allocate a Lua state");
    lua_sethook(L, instructionLimit, LUA_MASKCOUNT, 100000);
    Chunk chunk{&source, what};
    lua_pushcfunction(L, runChunk);
    lua_pushlightuserdata(L, &chunk);
    const int status = lua_pcall(L, 1, 1, 0);
    lua_sethook(L, nullptr, 0, 0);
    bool ok;
    if (status != LUA_OK) {
        const char* msg = lua_tostring(L, -1);
        ok = fail(error, msg ? msg : std::string(what) + " could not be read");
    } else {
        ok = read(L, lua_gettop(L), error);
    }
    lua_close(L);
    return ok;
}

} // namespace enjin2
