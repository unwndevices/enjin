#include "enjin2/graphics/palette_file.hpp"

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

const char* const kPaletteRoleNames[ROLE_COUNT] = {
    "bg", "surface", "muted", "text", "accent", "accent2", "accent3", "accent4",
};

namespace {

constexpr size_t kPaletteFileLimit = 4096;

void instructionLimit(lua_State* L, lua_Debug*) {
    luaL_error(L, "palette file instruction limit exceeded");
}

// On device plain malloc is internal RAM only: prefer PSRAM, like the skin
// manifest reader (Tomodachi #277).
void* paletteAlloc(void*, void* ptr, size_t, size_t nsize) {
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

// Runs under lua_pcall, so running out of memory fails the read instead of
// aborting. The skin and applet manifest sandbox: text only (no bytecode), an
// empty environment (no libraries), and the chunk must return a table.
int readTable(lua_State* L) {
    const auto* source = static_cast<const std::string*>(lua_touserdata(L, 1));
    if (luaL_loadbufferx(L, source->data(), source->size(), "=palette file", "t") != LUA_OK)
        return lua_error(L);
    lua_newtable(L);
    lua_setupvalue(L, -2, 1);  // the chunk's _ENV
    lua_call(L, 0, 1);
    if (!lua_istable(L, -1)) return luaL_error(L, "a palette file must return a table");
    return 1;
}

int hexDigit(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

// Exactly "#rrggbb" (parseHexColor also takes looser forms).
bool parseRole(const char* s, RGB& out) {
    if (!s || s[0] != '#' || std::strlen(s) != 7) return false;
    uint8_t ch[3];
    for (int i = 0; i < 3; ++i) {
        const int hi = hexDigit(s[1 + 2 * i]);
        const int lo = hexDigit(s[2 + 2 * i]);
        if (hi < 0 || lo < 0) return false;
        ch[i] = static_cast<uint8_t>(hi * 16 + lo);
    }
    out = RGB(ch[0], ch[1], ch[2]);
    return true;
}

// A string field of the table at `table`, or empty. False when the field is
// set but is not a string.
bool readString(lua_State* L, int table, const char* key, std::string& out) {
    lua_getfield(L, table, key);
    const int type = lua_type(L, -1);
    bool ok = true;
    if (type == LUA_TSTRING) {
        size_t len = 0;
        const char* s = lua_tolstring(L, -1, &len);
        out.assign(s, len);
    } else {
        out.clear();
        ok = type == LUA_TNIL;
    }
    lua_pop(L, 1);
    return ok;
}

bool fail(std::string* error, const std::string& msg) {
    if (error) *error = msg;
    return false;
}

bool readFields(lua_State* L, int table, PaletteFile& out, std::string* error) {
    if (!readString(L, table, "name", out.name) || out.name.empty())
        return fail(error, "name is missing");
    if (!readString(L, table, "author", out.author))
        return fail(error, "author is not a string");
    if (!readString(L, table, "source", out.source))
        return fail(error, "source is not a string");
    for (uint8_t i = 0; i < ROLE_COUNT; ++i) {
        lua_getfield(L, table, kPaletteRoleNames[i]);
        const char* hex = lua_type(L, -1) == LUA_TSTRING ? lua_tostring(L, -1) : nullptr;
        const bool ok = parseRole(hex, out.colors[i]);
        lua_pop(L, 1);
        if (!ok) {
            return fail(error, std::string(kPaletteRoleNames[i]) +
                                   (hex ? " is not #rrggbb" : " is missing"));
        }
    }
    return true;
}

} // namespace

bool parsePaletteFile(const std::string& source, PaletteFile& out, std::string* error) {
    lua_State* L = lua_newstate(paletteAlloc, nullptr);
    if (!L) return fail(error, "cannot allocate a Lua state");
    lua_sethook(L, instructionLimit, LUA_MASKCOUNT, 100000);
    lua_pushcfunction(L, readTable);
    lua_pushlightuserdata(L, const_cast<std::string*>(&source));
    const int status = lua_pcall(L, 1, 1, 0);
    lua_sethook(L, nullptr, 0, 0);
    bool ok;
    if (status != LUA_OK) {
        const char* msg = lua_tostring(L, -1);
        ok = fail(error, msg ? msg : "not a palette file");
    } else {
        ok = readFields(L, lua_gettop(L), out, error);
    }
    lua_close(L);
    if (ok && error) error->clear();
    return ok;
}

bool loadPaletteFile(const std::string& path, PaletteFile& out, std::string* error) {
    struct stat st {};
    if (stat(path.c_str(), &st) || !S_ISREG(st.st_mode))
        return fail(error, "not a file");
    if (st.st_size <= 0 || static_cast<size_t>(st.st_size) > kPaletteFileLimit)
        return fail(error, "empty or larger than 4 KB");
    FILE* file = std::fopen(path.c_str(), "rb");
    if (!file) return fail(error, "cannot open");
    std::string source(static_cast<size_t>(st.st_size), '\0');
    const bool read = std::fread(source.data(), 1, source.size(), file) == source.size();
    std::fclose(file);
    if (!read) return fail(error, "cannot read");
    return parsePaletteFile(source, out, error);
}

} // namespace enjin2
