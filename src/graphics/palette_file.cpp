#include "enjin2/graphics/palette_file.hpp"
#include "enjin2/scripting/table_file.hpp"

extern "C" {
#include "lua.h"
}

#include <cstring>

namespace enjin2 {

const char* const kPaletteRoleNames[ROLE_COUNT] = {
    "bg", "surface", "muted", "text", "accent", "accent2", "accent3", "accent4",
};

namespace {

constexpr size_t kPaletteFileLimit = 4096;

// Exactly "#rrggbb" (parseHexColor also takes looser forms).
bool parseRole(const char* s, RGB& out) {
    if (!s || s[0] != '#' || std::strlen(s) != 7) return false;
    uint8_t ch[3];
    for (int i = 0; i < 3; ++i) {
        const int hi = hexDigitValue(s[1 + 2 * i]);
        const int lo = hexDigitValue(s[2 + 2 * i]);
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
    const bool ok = readTableFile(
        source, "palette file",
        [&out](lua_State* L, int table, std::string* why) { return readFields(L, table, out, why); },
        error);
    if (ok && error) error->clear();
    return ok;
}

bool loadPaletteFile(const std::string& path, PaletteFile& out, std::string* error) {
    std::string source;
    if (!readTableFileText(path, kPaletteFileLimit, source, error)) return false;
    return parsePaletteFile(source, out, error);
}

} // namespace enjin2
